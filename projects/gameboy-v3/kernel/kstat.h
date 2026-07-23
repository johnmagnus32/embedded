/*
 * kstat.h — the ARM 'struct stat64' the kernel fills for fstat64/stat64/
 * lstat64/fstatat64 (nr 197/195/196/327).
 *
 * The byte-exact layout now lives in the shared UAPI (uapi/gv3_abi.h) as
 * `struct gv3_stat64`, so the kernel and the rootfs libc agree by construction.
 * We alias the kernel's historical name `struct kstat64` to it so existing
 * kernel code is unchanged. The _Static_asserts on the layout live in the UAPI.
 */
#ifndef GV3K_KSTAT_H
#define GV3K_KSTAT_H

#include "uapi/gv3_abi.h"

#define kstat64 stat        /* `struct kstat64` == the UAPI `struct stat` */

#endif /* GV3K_KSTAT_H */
