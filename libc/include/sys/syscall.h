/*
 * sys/syscall.h — the ARM EABI syscall numbers, for libc.
 *
 * We do NOT hand-maintain the numbers here anymore. They are the KERNEL's UAPI
 * (the single source of truth); the rootfs build installs a copy of the kernel
 * header into this include tree as <syscalls.h> (see rootfs/Makefile's
 * `headers` step, the analog of `make headers_install`). This header just
 * re-exports it under the conventional <sys/syscall.h> name so program/libc
 * source can include it the usual way.
 *
 * Consequence: the libc can only ever issue syscalls the kernel implements —
 * the two sides read the exact same list, so they cannot drift.
 */
#ifndef _LIBC_SYS_SYSCALL_H
#define _LIBC_SYS_SYSCALL_H

#include <syscalls.h>   /* installed from kernel/include/uapi/syscalls.h */

/*
 * Additional ARM EABI syscall numbers the libc issues on a MAINLINE kernel. These are
 * NOT in the from-scratch kernel's UAPI — it doesn't implement them (a call there would
 * ENOSYS). They live HERE, in the libc, because syscall NUMBERS are an arch/ABI fact, not
 * a kernel-capability one: the same libc runs on either kernel, and which numbers actually
 * work is a runtime property. Values are the arm 32-bit EABI table (arch/arm/tools/syscall.tbl).
 * Guarded so a future kernel UAPI that defines one doesn't clash.
 */
#ifndef SYS_unlink
#define SYS_unlink          10
#endif
#ifndef SYS_sync
#define SYS_sync            36
#endif
#ifndef SYS_kill
#define SYS_kill            37
#endif
#ifndef SYS_mkdir
#define SYS_mkdir           39
#endif
#ifndef SYS_rmdir
#define SYS_rmdir           40
#endif
#ifndef SYS_symlink
#define SYS_symlink         83
#endif
#ifndef SYS_reboot
#define SYS_reboot          88
#endif
#ifndef SYS_epoll_ctl
#define SYS_epoll_ctl      251
#endif
#ifndef SYS_epoll_wait
#define SYS_epoll_wait     252
#endif
#ifndef SYS_socket
#define SYS_socket         281
#endif
#ifndef SYS_bind
#define SYS_bind           282
#endif
#ifndef SYS_connect
#define SYS_connect        283
#endif
#ifndef SYS_listen
#define SYS_listen         284
#endif
#ifndef SYS_accept
#define SYS_accept         285
#endif
#ifndef SYS_setsockopt
#define SYS_setsockopt     294
#endif
#ifndef SYS_sendmsg
#define SYS_sendmsg        296
#endif
#ifndef SYS_recvmsg
#define SYS_recvmsg        297
#endif
#ifndef SYS_mkdirat
#define SYS_mkdirat        323
#endif
#ifndef SYS_unlinkat
#define SYS_unlinkat       328
#endif
#ifndef SYS_epoll_pwait
#define SYS_epoll_pwait    346
#endif
#ifndef SYS_timerfd_create
#define SYS_timerfd_create 350
#endif
#ifndef SYS_timerfd_settime
#define SYS_timerfd_settime 353
#endif
#ifndef SYS_signalfd4
#define SYS_signalfd4      355
#endif
#ifndef SYS_epoll_create1
#define SYS_epoll_create1  357
#endif
#ifndef SYS_accept4
#define SYS_accept4        366
#endif

#endif /* _LIBC_SYS_SYSCALL_H */
