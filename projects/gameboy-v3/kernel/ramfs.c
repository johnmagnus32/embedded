/*
 * ramfs.c — the in-memory read-write filesystem (S9).
 *
 * A tree of inodes in kernel heap (kmalloc). Directories hold a singly-linked
 * list of dirents; regular files store data in a growable array of 4 KB pmm
 * pages. Path resolution walks components from a start dir, following symlinks
 * on intermediate components. See ramfs.h for the model.
 */

#include <stdint.h>
#include "ramfs.h"
#include "kmalloc.h"
#include "pmm.h"
#include "libk.h"

static struct rf_inode *root;
static uint32_t next_ino = 1;

struct rf_inode *ramfs_root(void) { return root; }

static struct rf_inode *inode_new(enum rf_type t, uint32_t mode)
{
	struct rf_inode *n = kzalloc(sizeof(*n));
	if (!n)
		return 0;
	n->type  = t;
	n->ino   = next_ino++;
	n->mode  = mode;
	n->nlink = 1;
	return n;
}

void ramfs_init(void)
{
	root = inode_new(RF_DIR, 040755);
	root->parent = root;               /* "/.." == "/" */
	next_ino = 2;                      /* root is ino 1 */
}

/* attach `child` under `dir` with name `name` (no dup check by caller's design) */
static int dir_link(struct rf_inode *dir, const char *name, struct rf_inode *child)
{
	struct rf_dirent *d = kzalloc(sizeof(*d));
	if (!d)
		return -1;
	strlcpy_(d->name, name, sizeof(d->name));
	d->inode = child;
	d->next = dir->children;
	dir->children = d;
	return 0;
}

struct rf_inode *ramfs_child(struct rf_inode *dir, const char *name)
{
	if (!dir || dir->type != RF_DIR)
		return 0;
	if (strcmp(name, ".") == 0)
		return dir;
	if (strcmp(name, "..") == 0)
		return dir->parent;
	for (struct rf_dirent *d = dir->children; d; d = d->next)
		if (strcmp(d->name, name) == 0)
			return d->inode;
	return 0;
}

struct rf_inode *ramfs_mkreg(struct rf_inode *dir, const char *name, uint32_t mode)
{
	struct rf_inode *n = inode_new(RF_REG, (mode & 07777) | 0100000);
	if (!n) return 0;
	if (dir_link(dir, name, n) != 0) { kfree(n); return 0; }
	return n;
}

struct rf_inode *ramfs_mkdir(struct rf_inode *dir, const char *name, uint32_t mode)
{
	struct rf_inode *n = inode_new(RF_DIR, (mode & 07777) | 040000);
	if (!n) return 0;
	n->parent = dir;
	if (dir_link(dir, name, n) != 0) { kfree(n); return 0; }
	return n;
}

struct rf_inode *ramfs_mklnk(struct rf_inode *dir, const char *name, const char *target)
{
	struct rf_inode *n = inode_new(RF_LNK, 0120777);
	if (!n) return 0;
	uint32_t len = (uint32_t)strlen(target);
	n->target = kmalloc(len + 1);
	if (!n->target) { kfree(n); return 0; }
	strcpy(n->target, target);
	n->size = len;
	if (dir_link(dir, name, n) != 0) { kfree(n->target); kfree(n); return 0; }
	return n;
}

struct rf_inode *ramfs_mknod(struct rf_inode *dir, const char *name, uint32_t mode,
                             uint32_t maj, uint32_t min)
{
	struct rf_inode *n = inode_new(RF_CHR, mode);
	if (!n) return 0;
	n->rdev_major = maj;
	n->rdev_minor = min;
	if (dir_link(dir, name, n) != 0) { kfree(n); return 0; }
	return n;
}

/* ---- path resolution ---------------------------------------------------- */

#define SYMLINK_MAX 8

/* Internal resolver carrying a SHARED symlink budget through recursion, so a
 * cyclic symlink is bounded (returns 0 = -ELOOP) instead of overflowing the
 * single kernel stack. `*budget` decrements on every symlink follow across the
 * whole resolution, not per-call. */
static struct rf_inode *lookup_b(struct rf_inode *start, const char *path,
                                 int follow, int *budget)
{
	struct rf_inode *cur = (path[0] == '/') ? root : (start ? start : root);
	const char *p = path;

	while (*p == '/') p++;                 /* skip leading slashes */

	while (*p) {
		/* extract one component into `comp` */
		char comp[64];
		uint32_t i = 0;
		while (p[i] && p[i] != '/' && i < sizeof(comp) - 1) { comp[i] = p[i]; i++; }
		comp[i] = '\0';
		/* component longer than we can hold -> fail (caller maps to -ENAMETOOLONG)
		 * rather than silently splitting the tail into a bogus next component. */
		if (p[i] && p[i] != '/')
			return 0;
		const char *nextp = p + i;
		int last = 1;
		{
			const char *q = nextp;
			while (*q == '/') q++;
			if (*q) last = 0;
		}

		if (!cur || cur->type != RF_DIR)
			return 0;                      /* tried to descend a non-dir */

		struct rf_inode *child = ramfs_child(cur, comp);
		if (!child)
			return 0;                      /* missing component */

		/* follow symlink: on intermediates always; on the final only if asked */
		if (child->type == RF_LNK && (!last || follow)) {
			if (--(*budget) < 0)
				return 0;                  /* loop guard (-ELOOP), shared budget */
			struct rf_inode *base = (child->target[0] == '/') ? root : cur;
			child = lookup_b(base, child->target, 1, budget);
			if (!child)
				return 0;
		}
		cur = child;

		p = nextp;
		while (*p == '/') p++;
	}
	return cur;
}

struct rf_inode *ramfs_lookup(struct rf_inode *start, const char *path, int follow)
{
	int budget = SYMLINK_MAX;
	return lookup_b(start, path, follow, &budget);
}

/* ---- regular-file data (page-backed, growable) -------------------------- */

/* Ensure the file has at least `pages` page slots + backing frames. */
static int ensure_pages(struct rf_inode *ino, uint32_t pages)
{
	if (pages > ino->cap) {
		uint32_t newcap = ino->cap ? ino->cap * 2 : 4;
		if (newcap < pages) newcap = pages;
		uint32_t *np = krealloc(ino->pages, newcap * sizeof(uint32_t));
		if (!np)
			return -1;
		for (uint32_t i = ino->cap; i < newcap; i++) np[i] = 0;
		ino->pages = np;
		ino->cap = newcap;
	}
	for (uint32_t i = ino->npages; i < pages; i++) {
		uint32_t pa = pmm_alloc_page();    /* zeroed */
		if (!pa)
			return -1;
		ino->pages[i] = pa;
	}
	if (pages > ino->npages)
		ino->npages = pages;
	return 0;
}

long ramfs_read(struct rf_inode *ino, uint32_t off, void *dst, uint32_t n)
{
	if (ino->type != RF_REG)
		return -21;                        /* -EISDIR-ish */
	if (off >= ino->size)
		return 0;
	if (n > ino->size - off)               /* no overflow: off < size here */
		n = ino->size - off;
	uint8_t *d = dst;
	uint32_t done = 0;
	while (done < n) {
		uint32_t pos = off + done;
		uint32_t pg = pos / PAGE_SIZE, poff = pos % PAGE_SIZE;
		uint32_t chunk = PAGE_SIZE - poff;
		if (chunk > n - done) chunk = n - done;
		const uint8_t *src = (const uint8_t *)(uintptr_t)(ino->pages[pg] + poff);
		memcpy(d + done, src, chunk);
		done += chunk;
	}
	return (long)done;
}

long ramfs_write(struct rf_inode *ino, uint32_t off, const void *src, uint32_t n)
{
	if (ino->type != RF_REG)
		return -21;
	if (n > 0xFFFFFFFFu - off)             /* off+n would wrap 32 bits */
		return -22;                        /* -EINVAL */
	uint32_t end = off + n;
	uint32_t need_pages = (end + PAGE_SIZE - 1) / PAGE_SIZE;
	if (ensure_pages(ino, need_pages) != 0)
		return -12;                        /* -ENOMEM */
	const uint8_t *s = src;
	uint32_t done = 0;
	while (done < n) {
		uint32_t pos = off + done;
		uint32_t pg = pos / PAGE_SIZE, poff = pos % PAGE_SIZE;
		uint32_t chunk = PAGE_SIZE - poff;
		if (chunk > n - done) chunk = n - done;
		uint8_t *dst = (uint8_t *)(uintptr_t)(ino->pages[pg] + poff);
		memcpy(dst, s + done, chunk);
		done += chunk;
	}
	if (end > ino->size)
		ino->size = end;
	return (long)done;
}

int ramfs_truncate(struct rf_inode *ino, uint32_t newsize)
{
	if (ino->type != RF_REG)
		return -21;
	uint32_t need_pages = (newsize + PAGE_SIZE - 1) / PAGE_SIZE;
	if (newsize > ino->size) {
		if (ensure_pages(ino, need_pages) != 0)
			return -12;
	} else {
		/* free pages beyond the new end */
		for (uint32_t i = need_pages; i < ino->npages; i++) {
			if (ino->pages[i]) { pmm_free_page(ino->pages[i]); ino->pages[i] = 0; }
		}
		if (need_pages < ino->npages)
			ino->npages = need_pages;
	}
	ino->size = newsize;
	return 0;
}

const uint8_t *ramfs_page_at(struct rf_inode *ino, uint32_t off, uint32_t *avail)
{
	if (ino->type != RF_REG || off >= ino->size) { *avail = 0; return 0; }
	uint32_t pg = off / PAGE_SIZE, poff = off % PAGE_SIZE;
	uint32_t a = PAGE_SIZE - poff;
	if (a > ino->size - off) a = ino->size - off;
	*avail = a;
	return (const uint8_t *)(uintptr_t)(ino->pages[pg] + poff);
}

/* ---- pipes (anonymous, ring-buffered) ----------------------------------- *
 * A pipe is an inode with a fixed ring buffer and reader/writer-open counts.
 * The open counts are maintained by the file layer (file_open_inode /
 * file_close), so they track live open-file descriptions across fork/dup — the
 * write end stays "open" until the last fd referencing it anywhere is closed.
 *
 * Blocking: a read of an empty pipe whose write end is still open must WAIT for
 * a writer (a `$(...)` child hasn't run yet). That wait is cooperative and lives
 * in the syscall dispatcher (see syscall.c SYS_read): it yields to the runnable
 * writer and re-runs the read. By the time this function is actually called the
 * ring either has data or the write end is closed — so returning `got` (0 when
 * empty) correctly means EOF. write returns a short count if the ring fills. */
struct rf_inode *ramfs_pipe(void)
{
	struct rf_inode *p = inode_new(RF_PIPE, 0010600);   /* S_IFIFO | rw------- */
	if (!p) return 0;
	p->pbuf = kmalloc(PIPE_CAP);
	if (!p->pbuf) { kfree(p); return 0; }
	p->phead = p->ptail = 0;
	p->size = 0;
	p->wr_open = p->rd_open = 0;   /* file_open_inode() bumps per access mode */
	return p;
}

long ramfs_pipe_read(struct rf_inode *p, void *dst, uint32_t n)
{
	if (p->type != RF_PIPE) return -22;      /* -EINVAL */
	uint8_t *d = dst;
	uint32_t got = 0;
	while (got < n && p->size > 0) {
		d[got++] = p->pbuf[p->ptail];
		p->ptail = (p->ptail + 1) % PIPE_CAP;
		p->size--;
	}
	/* Empty here means the dispatcher already decided not to block (write end
	 * closed, or nobody to yield to) -> got==0 is EOF. */
	return (long)got;
}

long ramfs_pipe_write(struct rf_inode *p, const void *src, uint32_t n)
{
	if (p->type != RF_PIPE) return -22;
	if (!p->rd_open) return -32;             /* -EPIPE: no readers */
	const uint8_t *s = src;
	uint32_t put = 0;
	while (put < n && p->size < PIPE_CAP) {
		p->pbuf[p->phead] = s[put++];
		p->phead = (p->phead + 1) % PIPE_CAP;
		p->size++;
	}
	return (long)put;                        /* short write if the ring fills */
}
