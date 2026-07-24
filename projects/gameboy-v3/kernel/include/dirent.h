/*
 * dirent.h — struct linux_dirent64 for getdents64 (nr 217).
 *
 * The byte-exact layout + reclen math now live in the shared UAPI
 * (uapi/gv3_abi.h) as `struct gv3_dirent64` / GV3_DIRENT64_*, so the kernel and
 * the rootfs libc agree by construction. We alias the kernel's historical names
 * (linux_dirent64, DIRENT64_NAMEOFF, DIRENT64_RECLEN) so existing kernel code is
 * unchanged.
 */
#ifndef GV3K_DIRENT_H
#define GV3K_DIRENT_H

#include "uapi/gv3_abi.h"

#define linux_dirent64      gv3_dirent64        /* `struct linux_dirent64` */
#define DIRENT64_NAMEOFF    GV3_DIRENT64_NAMEOFF
#define DIRENT64_RECLEN(n)  GV3_DIRENT64_RECLEN(n)

#endif /* GV3K_DIRENT_H */
