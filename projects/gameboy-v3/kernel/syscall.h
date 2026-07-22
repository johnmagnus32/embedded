/*
 * syscall.h — the kernel's syscall dispatch (Linux ARM EABI: nr in r7,
 * args r0-r6, return in r0). S7 implements a tiny subset; the dispatch
 * machinery is what later stages extend.
 */
#ifndef GV3K_SYSCALL_H
#define GV3K_SYSCALL_H

#include <stdint.h>

/* ARM EABI syscall numbers we handle (from arch/arm/tools/syscall.tbl).
 * VERIFIED against the ARM EABI table (S9 workflow). These are the numbers a
 * static musl BusyBox actually issues — musl uses the *at / *64 / statx forms,
 * not the legacy open(5)/lseek(19)/stat(106)/getdents(141) variants. */
#define SYS_exit         1
#define SYS_fork         2
#define SYS_read         3
#define SYS_write        4
#define SYS_open         5    /* legacy; we route it to openat(AT_FDCWD,...) */
#define SYS_close        6
#define SYS_execve      11
#define SYS_chdir       12
#define SYS_lseek       19    /* legacy; musl uses _llseek */
#define SYS_getpid      20
#define SYS_access      33    /* legacy; musl uses faccessat */
#define SYS_dup         41
#define SYS_pipe        42
#define SYS_brk         45
#define SYS_ioctl       54
#define SYS_mount       21
#define SYS_umount2     52
#define SYS_dup2        63
#define SYS_getppid     64
#define SYS_setpgid     57
#define SYS_setsid      66
#define SYS_getpgid    132
#define SYS_getsid     147
#define SYS_munmap      91
#define SYS_mprotect   125
#define SYS_uname      122
#define SYS_fchdir     133
#define SYS_getpgrp     65
#define SYS_getuid      24
#define SYS_getgid      47
#define SYS_geteuid     49
#define SYS_getegid     50
#define SYS_getuid32   199
#define SYS_getgid32   200
#define SYS_geteuid32  201
#define SYS_getegid32  202
#define SYS_setuid32   213
#define SYS_setgid32   214
#define SYS_gettid     224
#define SYS_prctl      172
#define SYS_ugetrlimit 191
#define SYS_setrlimit   75
#define SYS_sched_yield 158
#define SYS_times       43
#define SYS_restart_syscall 0
#define SYS_madvise    220
#define SYS__llseek    140
#define SYS_readv      145
#define SYS_writev     146
#define SYS_getcwd     183
#define SYS_mmap2      192
#define SYS_stat64     195
#define SYS_lstat64    196
#define SYS_fstat64    197
#define SYS_getdents64 217
#define SYS_fcntl64    221
#define SYS_set_tid_address 256
#define SYS_openat     322
#define SYS_fstatat64  327
#define SYS_readlinkat 332
#define SYS_faccessat  334
#define SYS_pipe2      359
#define SYS_statx      397
#define SYS_sched_getaffinity 242
#define SYS_poll        168
#define SYS_faccessat2 439
#define SYS_wait4      114
#define SYS_exit_group 248

/* S10 group-A: process creation via clone (fork-flags form), signals, and time.
 * musl 1.2 (our 2021.11 toolchain) is time64, so BusyBox issues the *_time64
 * variants (clock_gettime64=403, clock_nanosleep_time64=407); we handle both the
 * legacy time32 numbers and the time64 ones so either path works. */
#define SYS_clone      120
#define SYS_rt_sigreturn   173
#define SYS_rt_sigaction   174
#define SYS_rt_sigprocmask 175
#define SYS_gettimeofday    78
#define SYS_sysinfo        116
#define SYS_nanosleep      162
#define SYS_clock_gettime  263
#define SYS_clock_nanosleep 265
#define SYS_clock_gettime64        403
#define SYS_clock_nanosleep_time64 407

#define K_ENOSYS   38   /* Function not implemented */
#define K_EINTR     4   /* Interrupted system call    */
#define K_EINVAL_S 22   /* Invalid argument           */

struct trapframe;
struct trapframe *syscall_trap(struct trapframe *tf);
struct trapframe *proc_current_tf(void);

#endif /* GV3K_SYSCALL_H */
