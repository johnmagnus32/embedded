/*
 * file.c — open-file descriptions + per-process fd tables (S9).
 *
 * struct file is the shared "open file description": inode + offset + flags,
 * reference-counted. A process's fd table maps small ints to struct file*.
 * dup/dup2 bump the refcount (shared offset); fork copies the table and bumps
 * every open file's refcount; exit closes them all.
 */

#include <stdint.h>
#include "file.h"
#include "ramfs.h"
#include "kmalloc.h"
#include "libk.h"

void proc_wakeup(const void *chan);          /* proc.c — wake sleepers on chan */

/* For a pipe, count this open-file description against the read/write end so a
 * reader can tell when all writers are gone (EOF) and vice versa. dup/fork only
 * bump refcnt (share the same description) and so must NOT touch these counts;
 * only real opens (here) and final closes (file_close) do.
 *
 * On a CLOSE that drops the last writer (wr_open -> 0), wake any reader blocked
 * on this pipe so it sees EOF instead of sleeping forever; symmetrically a
 * dropped last reader wakes blocked writers (they'll get -EPIPE). The wait
 * channel is the inode pointer (matches sys_read/sys_write's proc_sleep/wakeup). */
static void pipe_open_account(struct file *f, int delta)
{
	if (!f->inode || f->inode->type != RF_PIPE)
		return;
	uint32_t acc = f->flags & 3;             /* O_ACCMODE */
	if (acc == 0 /*O_RDONLY*/)      f->inode->rd_open += delta;
	else if (acc == 1 /*O_WRONLY*/) f->inode->wr_open += delta;
	else /* O_RDWR */             { f->inode->rd_open += delta; f->inode->wr_open += delta; }
	if (delta < 0 && (f->inode->wr_open == 0 || f->inode->rd_open == 0))
		proc_wakeup(f->inode);           /* last writer/reader gone -> wake waiters */
}

struct file *file_open_inode(struct rf_inode *ino, uint32_t flags)
{
	struct file *f = kzalloc(sizeof(*f));
	if (!f)
		return 0;
	f->inode  = ino;
	f->off    = 0;
	f->flags  = flags;
	f->refcnt = 1;
	f->dir_pos = 0;
	pipe_open_account(f, +1);
	return f;
}

struct file *file_dup(struct file *f)
{
	if (f)
		f->refcnt++;
	return f;
}

void file_close(struct file *f)
{
	if (!f)
		return;
	if (--f->refcnt <= 0) {
		pipe_open_account(f, -1);        /* last ref to this open description */
		kfree(f);
	}
}

void fdtable_init(struct fdtable *t)
{
	for (int i = 0; i < NOFILE; i++)
		t->fd[i] = 0;
}

int fd_alloc(struct fdtable *t, struct file *f)
{
	return fd_alloc_from(t, f, 0);
}

int fd_alloc_from(struct fdtable *t, struct file *f, int min)
{
	if (min < 0) min = 0;
	for (int i = min; i < NOFILE; i++)
		if (!t->fd[i]) { t->fd[i] = f; return i; }
	return -1;                         /* -EMFILE: table full */
}

struct file *fd_get(struct fdtable *t, int fd)
{
	if (fd < 0 || fd >= NOFILE)
		return 0;
	return t->fd[fd];
}

int fd_install_at(struct fdtable *t, int fd, struct file *f)
{
	if (fd < 0 || fd >= NOFILE)
		return -1;
	if (t->fd[fd])
		file_close(t->fd[fd]);         /* dup2: close whatever was there */
	t->fd[fd] = f;
	return fd;
}

int fd_close(struct fdtable *t, int fd)
{
	if (fd < 0 || fd >= NOFILE || !t->fd[fd])
		return -9;                     /* -EBADF */
	file_close(t->fd[fd]);
	t->fd[fd] = 0;
	return 0;
}

void fdtable_copy(struct fdtable *dst, const struct fdtable *src)
{
	for (int i = 0; i < NOFILE; i++) {
		dst->fd[i] = src->fd[i];
		if (dst->fd[i])
			dst->fd[i]->refcnt++;      /* shared open-file description */
	}
}

void fdtable_closeall(struct fdtable *t)
{
	for (int i = 0; i < NOFILE; i++)
		if (t->fd[i]) { file_close(t->fd[i]); t->fd[i] = 0; }
}
