/*
 * syscall.h — the kernel's syscall dispatch (Linux ARM EABI: nr in r7,
 * args r0-r6, return in r0). S7 implements a tiny subset; the dispatch
 * machinery is what later stages extend.
 */
#ifndef GV3K_SYSCALL_H
#define GV3K_SYSCALL_H

#include <stdint.h>

/* The syscall NUMBERS are the shared kernel<->userspace ABI: they live in the
 * UAPI header (kernel/uapi/gv3_syscalls.h), the single source of truth that both
 * this dispatch and the rootfs libc consume, so the two can never drift. See
 * that file's header comment. Everything below is kernel-PRIVATE (errno codes
 * the dispatch returns, and the trap entry prototypes). */
#include "uapi/gv3_syscalls.h"

#define K_ENOSYS   38   /* Function not implemented */
#define K_EINTR     4   /* Interrupted system call    */
#define K_EINVAL_S 22   /* Invalid argument           */

struct trapframe;
struct trapframe *syscall_trap(struct trapframe *tf);
struct trapframe *proc_current_tf(void);

#endif /* GV3K_SYSCALL_H */
