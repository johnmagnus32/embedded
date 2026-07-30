/*
 * file.h — the VFS glue: open files + per-process file-descriptor tables (S9).
 *
 * A `struct file` is an OPEN instance of an inode: it pairs the inode with a
 * cursor (offset) and the open flags, and is reference-counted so dup() and
 * fork() can share one open-file description (POSIX semantics: dup'd fds and
 * fork'd fds share the same offset). A process's fd table maps small integer
 * fds -> struct file*.
 *
 * This mirrors real Unix: fd (per-process int) -> struct file (shared open
 * description, has the offset) -> inode (the actual object). Three levels.
 */
#ifndef GV3K_FILE_H
#define GV3K_FILE_H

#include <stdint.h>

struct rf_inode;

struct file {
	struct rf_inode *inode;
	uint32_t         off;      /* current read/write cursor */
	uint32_t         flags;    /* open flags (O_RDONLY/WRONLY/RDWR/APPEND/...) */
	int              refcnt;   /* shared by dup()'d and fork()'d fds */
	/* directory read cursor for getdents64 (index into the child list) */
	uint32_t         dir_pos;
};

#define NOFILE 16          /* max open fds per process (small, fixed) */

/* Per-process fd table. Lives in struct proc (S9 adds it). fd[i]==NULL = free. */
struct fdtable {
	struct file *fd[NOFILE];
};

/* ---- open-file lifetime ------------------------------------------------- */
struct file *file_open_inode(struct rf_inode *ino, uint32_t flags);
struct file *file_dup(struct file *f);       /* ++refcnt, returns same file */
void         file_close(struct file *f);     /* --refcnt; frees at 0 */

/* ---- fd table ops (operate on a given process's table) ------------------ */
void fdtable_init(struct fdtable *t);
int  fd_alloc(struct fdtable *t, struct file *f);        /* lowest free fd, or -1 */
int  fd_alloc_from(struct fdtable *t, struct file *f, int min);  /* F_DUPFD */
struct file *fd_get(struct fdtable *t, int fd);          /* NULL if not open */
int  fd_install_at(struct fdtable *t, int fd, struct file *f);   /* dup2 target */
int  fd_close(struct fdtable *t, int fd);
void fdtable_copy(struct fdtable *dst, const struct fdtable *src);  /* fork */
void fdtable_closeall(struct fdtable *t);                /* exit */

#endif /* GV3K_FILE_H */
