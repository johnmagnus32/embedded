/*
 * fcntl.h — open() + its flags. The FLAG VALUES (O_*, AT_*) come from the shared
 * kernel UAPI (gv3_abi.h, installed by `make headers`), so the libc and kernel
 * agree by construction — we don't re-type them here. This header adds only the
 * POSIX function declarations.
 */
#ifndef _GV3_FCNTL_H
#define _GV3_FCNTL_H

#include <gv3_abi.h>    /* O_RDONLY/O_CREAT/O_DIRECTORY/..., AT_FDCWD, ... */

int open(const char *path, int flags, ...);
int openat(int dirfd, const char *path, int flags, int mode);

#endif /* _GV3_FCNTL_H */
