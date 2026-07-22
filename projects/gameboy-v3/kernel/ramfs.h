/*
 * ramfs.h — a tiny read-write in-memory filesystem (S9).
 *
 * The kernel unpacks the initramfs cpio into this tree at boot, then execve and
 * the file syscalls (open/read/write/getdents/stat) operate on it. It is the
 * "root filesystem" — there is no block device, everything lives in DRAM pages.
 *
 * Design (deliberately simple, honest about scope):
 *   - Every filesystem object is a `struct rf_inode` (regular file, directory,
 *     or symlink). Directories hold a linked list of `struct rf_dirent` naming
 *     child inodes. There is one hard root inode ("/").
 *   - Regular-file data is stored in a growable vector of 4 KB pages from pmm,
 *     so files can be written and extended (RW, per the S9 decision). No holes.
 *   - No permissions enforcement, no uid/gid checks, no mount points, no links
 *     across directories beyond what cpio hardlinks produce (we don't dedup).
 *     Those are later refinements; this is enough for a shell + BusyBox.
 */
#ifndef GV3K_RAMFS_H
#define GV3K_RAMFS_H

#include <stdint.h>

/* inode types (mirrors the S_IFMT high bits of st_mode we care about). */
enum rf_type { RF_FREE = 0, RF_REG, RF_DIR, RF_LNK, RF_CHR, RF_PIPE };

struct rf_inode;

/* a directory entry: a name bound to a child inode, in a sibling list. */
struct rf_dirent {
	char              name[64];   /* component name (NUL-terminated, no '/') */
	struct rf_inode  *inode;      /* the object this name refers to */
	struct rf_dirent *next;       /* next entry in the parent directory */
};

struct rf_inode {
	enum rf_type      type;
	uint32_t          ino;        /* unique inode number (for stat) */
	uint32_t          mode;       /* full st_mode (type bits | perm bits) */
	uint32_t          size;       /* bytes: file length, or symlink target len */
	uint32_t          nlink;      /* link count (>=1 for live inodes) */

	/* directory: list of children (RF_DIR only) */
	struct rf_dirent *children;
	struct rf_inode  *parent;     /* .. (root's parent is itself) */

	/* regular file: page-backed data. pages[] holds physical addresses of
	 * 4 KB frames; npages = ceil(size/4096), cap = allocated slots in pages[]. */
	uint32_t         *pages;      /* array of page PAs (kmalloc'd, grows) */
	uint32_t          npages;
	uint32_t          cap;

	/* symlink: target path stored inline (size = strlen, no NUL needed) */
	char             *target;

	/* char device (e.g. /dev/console): major/minor (we special-case console) */
	uint32_t          rdev_major, rdev_minor;

	/* pipe (RF_PIPE): a fixed ring buffer. `size` counts bytes currently held;
	 * head/tail index into buf[]. writers count via file O_WRONLY, readers
	 * O_RDONLY. wr_closed marks the write end gone (read then sees EOF). */
	uint8_t          *pbuf;       /* kmalloc'd ring (PIPE_CAP bytes) */
	uint32_t          phead, ptail;
	int               wr_open, rd_open;
};

/* ---- lifecycle ---------------------------------------------------------- */
void             ramfs_init(void);          /* create the root inode */
struct rf_inode *ramfs_root(void);

/* ---- pipe (anonymous RF_PIPE inode + ring buffer) ----------------------- */
#define PIPE_CAP 4096
struct rf_inode *ramfs_pipe(void);                 /* new pipe inode (rd+wr open) */
long ramfs_pipe_read(struct rf_inode *p, void *dst, uint32_t n);
long ramfs_pipe_write(struct rf_inode *p, const void *src, uint32_t n);

/* ---- inode creation (used by the cpio loader and by open(O_CREAT)/mkdir) - */
struct rf_inode *ramfs_mkreg(struct rf_inode *dir, const char *name, uint32_t mode);
struct rf_inode *ramfs_mkdir(struct rf_inode *dir, const char *name, uint32_t mode);
struct rf_inode *ramfs_mklnk(struct rf_inode *dir, const char *name, const char *target);
struct rf_inode *ramfs_mknod(struct rf_inode *dir, const char *name, uint32_t mode,
                             uint32_t maj, uint32_t min);

/* ---- path resolution ---------------------------------------------------- *
 * Resolve an absolute or `start`-relative path to an inode. Follows symlinks
 * for intermediate components; `follow` controls whether the FINAL component's
 * symlink is followed. Returns NULL if any component is missing. */
struct rf_inode *ramfs_lookup(struct rf_inode *start, const char *path, int follow);

/* Look up a single child by name in a directory (no path parsing). */
struct rf_inode *ramfs_child(struct rf_inode *dir, const char *name);

/* ---- file data (RW) ----------------------------------------------------- *
 * Byte-wise read/write into a regular file's page-backed store. write grows the
 * file (allocating pages) as needed. Return bytes transferred, or negative. */
long ramfs_read(struct rf_inode *ino, uint32_t off, void *dst, uint32_t n);
long ramfs_write(struct rf_inode *ino, uint32_t off, const void *src, uint32_t n);
int  ramfs_truncate(struct rf_inode *ino, uint32_t newsize);

/* Get a pointer to the physical bytes of file offset `off` and the number of
 * contiguous bytes available in that page (for zero-copy ELF loading). */
const uint8_t *ramfs_page_at(struct rf_inode *ino, uint32_t off, uint32_t *avail);

#endif /* GV3K_RAMFS_H */
