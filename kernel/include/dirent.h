/*
 * dirent.h — struct linux_dirent64 for getdents64 (nr 217).
 *
 * The byte-exact layout + reclen math now live in the shared UAPI
 * (uapi/abi.h) as `struct dirent64` / DIRENT64_*, so the kernel and
 * the rootfs libc agree by construction. We alias the kernel's historical names
 * (linux_dirent64, DIRENT64_NAMEOFF, DIRENT64_RECLEN) so existing kernel code is
 * unchanged.
 */
#ifndef K_DIRENT_H
#define K_DIRENT_H

#include "uapi/abi.h"

#define linux_dirent64      dirent64        /* `struct linux_dirent64` == `struct dirent64` */
/* DIRENT64_NAMEOFF / DIRENT64_RECLEN(n) come directly from the UAPI (uapi/abi.h). */

#endif /* K_DIRENT_H */
