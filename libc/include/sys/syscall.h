/*
 * sys/syscall.h — the ARM EABI syscall numbers, for gv3libc.
 *
 * We do NOT hand-maintain the numbers here anymore. They are the KERNEL's UAPI
 * (the single source of truth); the rootfs build installs a copy of the kernel
 * header into this include tree as <gv3_syscalls.h> (see rootfs/Makefile's
 * `headers` step, the analog of `make headers_install`). This header just
 * re-exports it under the conventional <sys/syscall.h> name so program/libc
 * source can include it the usual way.
 *
 * Consequence: the libc can only ever issue syscalls the kernel implements —
 * the two sides read the exact same list, so they cannot drift.
 */
#ifndef _GV3_SYS_SYSCALL_H
#define _GV3_SYS_SYSCALL_H

#include <gv3_syscalls.h>   /* installed from kernel/include/uapi/gv3_syscalls.h */

#endif /* _GV3_SYS_SYSCALL_H */
