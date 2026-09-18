/*
 * fcntl.h — open() + its flags. The FLAG VALUES (O_*, AT_*) come from the shared
 * kernel UAPI (abi.h, installed by `make headers`), so the libc and kernel
 * agree by construction — we don't re-type them here. This header adds only the
 * POSIX function declarations.
 */
#ifndef _LIBC_FCNTL_H
#define _LIBC_FCNTL_H

#include <abi.h>    /* O_RDONLY/O_CREAT/O_DIRECTORY/..., AT_FDCWD, ... */
#include <sys/types.h>   /* off_t, pid_t (struct flock) */

/* fcntl() commands + advisory-lock types (asm-generic values; the POSIX set a complete
 * libc must expose — e.g. libgcc's gcov coverage support uses F_SETLKW + struct flock). */
#define F_DUPFD  0
#define F_GETFD  1
#define F_SETFD  2
#define F_GETFL  3
#define F_SETFL  4
#define F_GETLK  5
#define F_SETLK  6
#define F_SETLKW 7
#define F_RDLCK  0
#define F_WRLCK  1
#define F_UNLCK  2

struct flock {
	short l_type;      /* F_RDLCK / F_WRLCK / F_UNLCK */
	short l_whence;    /* SEEK_SET / SEEK_CUR / SEEK_END */
	off_t l_start;
	off_t l_len;
	pid_t l_pid;
};

int open(const char *path, int flags, ...);
int openat(int dirfd, const char *path, int flags, int mode);
int fcntl(int fd, int cmd, ...);

#endif /* _LIBC_FCNTL_H */
