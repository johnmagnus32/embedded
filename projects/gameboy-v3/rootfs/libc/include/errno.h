/*
 * errno.h — a few errno values (subset), matching Linux/asm-generic numbers so
 * they agree with the negated-errno our kernel returns. Single-process, single-
 * threaded: errno is a plain global.
 */
#ifndef _GV3_ERRNO_H
#define _GV3_ERRNO_H

extern int errno;

#define EPERM         1
#define ENOENT        2
#define ESRCH         3
#define EINTR         4
#define EIO           5
#define EBADF         9
#define EAGAIN       11
#define ENOMEM       12
#define EACCES       13
#define EFAULT       14
#define ENOTDIR      20
#define EISDIR       21
#define EINVAL       22
#define EMFILE       24
#define ENOTTY       25
#define ESPIPE       29
#define ERANGE       34
#define ENAMETOOLONG 36
#define ENOSYS       38
#define ELOOP        40

#endif /* _GV3_ERRNO_H */
