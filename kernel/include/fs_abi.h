/*
 * fs_abi.h — kernel-side ABI glue for the filesystem syscalls.
 *
 * The userspace-facing ABI CONSTANTS (S_IF*, O_*, AT_*, SEEK_*, F_*, DT_*) now
 * live in the shared UAPI (uapi/gv3_abi.h) — the single source of truth the
 * rootfs libc also consumes. This header pulls those in and adds only the
 * KERNEL-PRIVATE negated-errno codes the dispatch returns (userspace has its own
 * errno.h with the POSIX-facing values; the K_E* names are an internal return
 * convention, deliberately not part of the shared UAPI).
 */
#ifndef GV3K_FS_ABI_H
#define GV3K_FS_ABI_H

#include "uapi/gv3_abi.h"    /* S_IF*, O_*, AT_*, SEEK_*, F_*, DT_*, PROT_*, MAP_* */

/* ---- errno values we return (negated) — kernel-internal convention ---- */
#define K_EPERM    1
#define K_ENOENT   2
#define K_EBADF    9
#define K_ENOMEM   12
#define K_EACCES   13
#define K_EFAULT   14
#define K_ENOTDIR  20
#define K_EISDIR   21
#define K_EINVAL   22
#define K_EMFILE   24
#define K_ENOSPC   28
#define K_ESPIPE   29
#define K_ENOSYS   38
#define K_ENOTEMPTY 39
#define K_ELOOP    40
#define K_ERANGE   34
#define K_ENAMETOOLONG 36
#define K_EIO_S     5   /* -EIO   */
#define K_ENOEXEC_S 8   /* -ENOEXEC */

#endif /* GV3K_FS_ABI_H */
