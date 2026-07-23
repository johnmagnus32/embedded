/*
 * gv3_syscalls.h — the ARM 32-bit EABI syscall numbers our kernel implements.
 *
 * THIS IS THE UAPI (userspace API) — the ONE source of truth for the syscall
 * ABI, owned by the kernel and shared with userspace. It is the analog of the
 * mainline kernel's <asm/unistd.h>: the kernel's dispatch (syscall.c) and every
 * userspace C library (rootfs/libc) both consume THIS file, so the two sides
 * can never drift. The rootfs build installs a copy into the libc's include
 * tree (like `make headers_install`); the kernel includes it directly.
 *
 * Numbers are the ARM EABI (arch/arm/tools/syscall.tbl) — VERIFIED against the
 * table (musl uses the *at / *64 / statx / *_time64 forms, not the legacy
 * open(5)/lseek(19)/stat(106)/getdents(141) variants). Do NOT add a number here
 * unless the kernel actually handles it: the whole point is that this file
 * bounds what userspace may issue.
 *
 * Contains ONLY the numeric contract — no code, no kernel-internal declarations,
 * no libc function prototypes. Safe to include from either side.
 */
#ifndef GV3_UAPI_SYSCALLS_H
#define GV3_UAPI_SYSCALLS_H

/* ---- process ------------------------------------------------------------- */
#define SYS_restart_syscall  0
#define SYS_exit             1
#define SYS_fork             2
#define SYS_execve          11
#define SYS_getpid          20
#define SYS_wait4          114
#define SYS_clone          120
#define SYS_exit_group     248
#define SYS_set_tid_address 256

/* ---- file I/O ------------------------------------------------------------ */
#define SYS_read             3
#define SYS_write            4
#define SYS_open             5    /* legacy; kernel routes to openat(AT_FDCWD,) */
#define SYS_close            6
#define SYS_lseek           19    /* legacy 32-bit; _llseek(140) for 64-bit off */
#define SYS_dup             41
#define SYS_pipe            42
#define SYS_ioctl           54
#define SYS_dup2            63
#define SYS__llseek        140
#define SYS_readv          145
#define SYS_writev         146
#define SYS_fcntl64        221
#define SYS_openat         322
#define SYS_pipe2          359
#define SYS_poll           168

/* ---- directories / cwd --------------------------------------------------- */
#define SYS_chdir           12
#define SYS_access          33    /* legacy; musl uses faccessat */
#define SYS_getcwd         183
#define SYS_getdents64     217
#define SYS_fchdir         133
#define SYS_readlinkat     332
#define SYS_faccessat      334
#define SYS_faccessat2     439

/* ---- stat ---------------------------------------------------------------- */
#define SYS_stat64         195
#define SYS_lstat64        196
#define SYS_fstat64        197
#define SYS_fstatat64      327
#define SYS_statx          397

/* ---- memory -------------------------------------------------------------- */
#define SYS_brk             45
#define SYS_munmap          91
#define SYS_mprotect       125
#define SYS_mmap2          192
#define SYS_madvise        220

/* ---- signals ------------------------------------------------------------- */
#define SYS_rt_sigreturn   173
#define SYS_rt_sigaction   174
#define SYS_rt_sigprocmask 175

/* ---- time (musl 1.2 is time64 -> the *_time64 forms; legacy kept too) ---- */
#define SYS_gettimeofday    78
#define SYS_sysinfo        116
#define SYS_nanosleep      162
#define SYS_clock_gettime  263
#define SYS_clock_nanosleep 265
#define SYS_clock_gettime64        403
#define SYS_clock_nanosleep_time64 407

/* ---- identity / limits / misc -------------------------------------------- */
#define SYS_getppid         64
#define SYS_setpgid         57
#define SYS_getpgrp         65
#define SYS_setsid          66
#define SYS_setrlimit       75
#define SYS_uname          122
#define SYS_getpgid        132
#define SYS_getsid         147
#define SYS_sched_yield    158
#define SYS_prctl          172
#define SYS_ugetrlimit     191
#define SYS_getuid          24
#define SYS_getgid          47
#define SYS_geteuid         49
#define SYS_getegid         50
#define SYS_getuid32       199
#define SYS_getgid32       200
#define SYS_geteuid32      201
#define SYS_getegid32      202
#define SYS_setuid32       213
#define SYS_setgid32       214
#define SYS_gettid         224
#define SYS_times           43
#define SYS_sched_getaffinity 242

/* ---- mount (accepted/stubbed by the kernel) ------------------------------ */
#define SYS_mount           21
#define SYS_umount2         52

#endif /* GV3_UAPI_SYSCALLS_H */
