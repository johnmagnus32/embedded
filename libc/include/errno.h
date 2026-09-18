/*
 * errno.h — a few errno values (subset), matching Linux/asm-generic numbers so
 * they agree with the negated-errno our kernel returns. Single-process, single-
 * threaded: errno is a plain global.
 */
#ifndef _LIBC_ERRNO_H
#define _LIBC_ERRNO_H

extern int errno;

#define EPERM         1
#define ENOENT        2
#define ESRCH         3
#define EINTR         4
#define EIO           5
#define ENXIO         6
#define E2BIG         7
#define ENOEXEC       8
#define EBADF         9
#define ECHILD       10
#define EAGAIN       11
#define ENOMEM       12
#define EACCES       13
#define EFAULT       14
#define EBUSY        16
#define EEXIST       17
#define EXDEV        18
#define ENODEV       19
#define ENOTDIR      20
#define EISDIR       21
#define EINVAL       22
#define EMFILE       24
#define ENOTTY       25
#define ENOSPC       28
#define ESPIPE       29
#define EPIPE        32
#define ERANGE       34
#define ENAMETOOLONG 36
#define ENOSYS       38
#define ELOOP        40

#endif /* _LIBC_ERRNO_H */
