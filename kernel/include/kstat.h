/*
 * kstat.h — the ARM 'struct stat64' the kernel fills for fstat64/stat64/
 * lstat64/fstatat64 (nr 197/195/196/327).
 *
 * The byte-exact layout now lives in the shared UAPI (uapi/abi.h) as
 * `struct stat64`, so the kernel and the rootfs libc agree by construction.
 * We alias the kernel's historical name `struct kstat64` to it so existing
 * kernel code is unchanged. The _Static_asserts on the layout live in the UAPI.
 */
#ifndef K_KSTAT_H
#define K_KSTAT_H

#include "uapi/abi.h"

#define kstat64 stat        /* `struct kstat64` == the UAPI `struct stat` */

#endif /* K_KSTAT_H */
