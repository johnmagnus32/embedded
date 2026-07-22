/*
 * syscall.c — kernel-side syscall dispatch (S9: process + file syscalls).
 *
 * svc_entry (start.S) builds a full `struct trapframe` and calls syscall_trap()
 * with a pointer to it. We snapshot it into the caller's PCB (cur->tf), read the
 * EABI number (r7) and args (r0-r5), dispatch, write the return into cur->tf.r[0],
 * and resume proc_current_tf() — which may be a DIFFERENT process's frame after
 * fork/exec/wait/exit.
 *
 * Numbers are the ARM EABI (arch/arm/tools/syscall.tbl), the exact ABI a static
 * musl BusyBox uses. File syscalls live in fs_syscall.c; process ones in proc.c.
 */

#include <stdint.h>
#include "syscall.h"
#include "fs_syscall.h"
#include "mm_syscall.h"
#include "uart.h"
#include "proc.h"
#include "libk.h"

/* ---- misc small syscalls (uname/brk/set_tid_address) --------------------- */

/* struct new_utsname is 6 fields x 65 bytes = 390 bytes. musl reads sysname..
 * machine; we fill plausible strings so `uname -a` works. */
static long sys_uname(uint32_t buf)
{
	if (!buf) return -14;                    /* -EFAULT */
	char *u = (char *)(uintptr_t)buf;
	memset(u, 0, 6 * 65);
	strcpy(u + 0 * 65, "Linux");             /* sysname  (ABI-compatible lie) */
	strcpy(u + 1 * 65, "gameboy-v3");        /* nodename */
	strcpy(u + 2 * 65, "0.9-gv3");           /* release  */
	strcpy(u + 3 * 65, "gv3kernel S10");     /* version  */
	strcpy(u + 4 * 65, "armv7l");            /* machine  */
	strcpy(u + 5 * 65, "(none)");            /* domainname */
	return 0;
}

/* ---- signals (rt_sigaction / rt_sigprocmask) ----------------------------- *
 * HONEST SCOPE: this kernel is cooperative — there is no asynchronous signal
 * source (no preemption, no kill(2) between processes), so a signal can never
 * actually be delivered. What the interactive shell needs at startup is for its
 * signal *setup* to SUCCEED: it installs handlers (SIGINT/SIGCHLD/SIGTSTP/…) and
 * blocks/unblocks signals around critical sections. So we faithfully STORE the
 * dispositions and the blocked mask in the PCB (and return the previous values
 * on query) — real bookkeeping, not a fake 0 — but we never invoke a handler.
 * When preemption + a tty-driven SIGINT arrive later, delivery plugs in here
 * using the stored sigact[] (push a signal frame, redirect tf.pc; rt_sigreturn
 * restores it). Until then rt_sigreturn should never be reached. */

/* ARM user `struct sigaction`: { void(*)handler; unsigned long flags;
 * void(*)restorer; sigset_t mask; } — 4-byte fields then an 8-byte mask on
 * arm32. matches struct ksig. */
static long sys_rt_sigaction(int sig, uint32_t act_uptr, uint32_t oldact_uptr,
                             uint32_t sigsetsize)
{
	struct proc *p = proc_current();
	/* musl passes sizeof(sigset_t)=8 on arm; reject anything else defensively. */
	if (sigsetsize != 8) return -K_EINVAL_S;
	/* signals are 1..64; SIGKILL(9)/SIGSTOP(19) can't be caught (but querying
	 * their old disposition is fine). index 0 is unused. */
	if (sig < 1 || sig >= NSIG) return -K_EINVAL_S;

	struct ksig *slot = &p->sigact[sig];
	if (oldact_uptr) {                       /* report the previous disposition */
		struct ksig *o = (struct ksig *)(uintptr_t)oldact_uptr;
		*o = *slot;
	}
	if (act_uptr) {                          /* install the new one             */
		if (sig == 9 || sig == 19) return -K_EINVAL_S;  /* SIGKILL/SIGSTOP */
		const struct ksig *a = (const struct ksig *)(uintptr_t)act_uptr;
		*slot = *a;
	}
	return 0;
}

/* rt_sigprocmask(how, set, oldset, sigsetsize): edit the 64-bit blocked mask. */
#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2
static long sys_rt_sigprocmask(int how, uint32_t set_uptr, uint32_t oldset_uptr,
                               uint32_t sigsetsize)
{
	struct proc *p = proc_current();
	if (sigsetsize != 8) return -K_EINVAL_S;

	if (oldset_uptr) {                       /* report the current mask first   */
		uint32_t *o = (uint32_t *)(uintptr_t)oldset_uptr;
		o[0] = (uint32_t)p->sig_blocked;
		o[1] = (uint32_t)(p->sig_blocked >> 32);
	}
	if (set_uptr) {
		const uint32_t *s = (const uint32_t *)(uintptr_t)set_uptr;
		uint64_t m = (uint64_t)s[0] | ((uint64_t)s[1] << 32);
		switch (how) {
		case SIG_BLOCK:   p->sig_blocked |= m;  break;
		case SIG_UNBLOCK: p->sig_blocked &= ~m; break;
		case SIG_SETMASK: p->sig_blocked = m;   break;
		default:          return -K_EINVAL_S;
		}
	}
	return 0;
}

/* ---- time ---------------------------------------------------------------- *
 * One coarse monotonic source: the timer tick (ktime_uptime_ms). We report it
 * as both wall-clock (gettimeofday/CLOCK_REALTIME) and monotonic — there is no
 * RTC, so "seconds since boot" is our clock. All math is 32-bit on purpose: no
 * 64-bit division is linked (freestanding, no libgcc), and uptime fits in u32. */
uint32_t ktime_uptime_ms(void);              /* kmain.c */

static long sys_gettimeofday(uint32_t tv_uptr, uint32_t tz_uptr)
{
	(void)tz_uptr;
	if (tv_uptr) {
		uint32_t ms = ktime_uptime_ms();
		uint32_t *tv = (uint32_t *)(uintptr_t)tv_uptr;   /* {tv_sec, tv_usec} u32 */
		tv[0] = ms / 1000u;
		tv[1] = (ms % 1000u) * 1000u;            /* microseconds */
	}
	return 0;
}

/* clock_gettime (263, timespec u32 sec) and clock_gettime64 (403, timespec
 * with 64-bit sec). clkid is ignored — we have one clock. */
static long sys_clock_gettime(uint32_t clkid, uint32_t ts_uptr, int time64)
{
	(void)clkid;
	if (ts_uptr) {
		uint32_t ms = ktime_uptime_ms();
		uint32_t sec = ms / 1000u;
		uint32_t nsec = (ms % 1000u) * 1000000u;
		uint32_t *ts = (uint32_t *)(uintptr_t)ts_uptr;
		if (time64) {                            /* struct __timespec64 */
			ts[0] = sec; ts[1] = 0;          /* tv_sec (64-bit LE)  */
			ts[2] = nsec;                    /* tv_nsec (+ pad word) */
			ts[3] = 0;
		} else {
			ts[0] = sec; ts[1] = nsec;
		}
	}
	return 0;
}

/* sysinfo: BusyBox startup probes it; fill uptime + a plausible memory picture
 * from the pmm. struct sysinfo on arm32 is a fixed layout; we fill the leading
 * fields callers read and zero the rest. */
static long sys_sysinfo(uint32_t info_uptr)
{
	if (!info_uptr) return -14;              /* -EFAULT */
	uint32_t *si = (uint32_t *)(uintptr_t)info_uptr;
	/* zero the whole struct (arm32 sizeof(struct sysinfo) = 64 bytes). */
	memset(si, 0, 64);
	si[0] = ktime_uptime_ms() / 1000u;       /* uptime (seconds) */
	/* mem_unit at the end is 1; totalram/freeram in bytes via the pmm. */
	extern uint32_t pmm_total_pages(void);
	extern uint32_t pmm_free_pages(void);
	si[3] = pmm_total_pages() * 4096u;       /* totalram (loads=si[1..2] left 0) */
	si[4] = pmm_free_pages()  * 4096u;       /* freeram  */
	si[14] = 1;                              /* mem_unit = 1 byte */
	return 0;
}

struct trapframe *syscall_trap(struct trapframe *tf)
{
	struct proc *p = proc_current();
	p->tf = *tf;                             /* snapshot caller's frame */

	uint32_t nr = p->tf.r[7];
	uint32_t a0 = p->tf.r[0], a1 = p->tf.r[1], a2 = p->tf.r[2];
	uint32_t a3 = p->tf.r[3], a4 = p->tf.r[4];
	long r = -K_ENOSYS;

	switch (nr) {
	/* ---- process (proc.c) ---- */
	case SYS_fork:       proc_fork();                          goto done_switch;
	case SYS_clone:      proc_clone(a0, a1);                   goto done_switch;
	case SYS_execve:     proc_execve((const char *)(uintptr_t)a0, a1, a2); goto done_switch;
	case SYS_wait4:      proc_wait(a1);                        goto done_switch;
	case SYS_exit:
	case SYS_exit_group: proc_exit((int)a0);                   goto done_switch;
	case SYS_getpid:     r = p->pid; break;

	/* ---- files (fs_syscall.c) ---- */
	case SYS_openat:     r = sys_openat((int)a0, (const char*)(uintptr_t)a1, (int)a2, a3); break;
	case SYS_open:       r = sys_openat(-100, (const char*)(uintptr_t)a0, (int)a1, a2); break; /* AT_FDCWD */
	case SYS_close:      r = sys_close((int)a0); break;
	case SYS_read: {
		/* Blocking pipe read: if this would read an empty pipe whose write end is
		 * still open (the `$(...)` writer hasn't run yet), SLEEP on the pipe inode
		 * (leaves the run queue) until sys_write wakes us; the svc then re-runs.
		 * proc_sleep() returns 1 if it slept+switched (resume the new `cur`, don't
		 * write r0); 0 if nobody else can run (fall through -> sys_read returns
		 * 0 = EOF rather than deadlock). */
		const void *chan = read_block_chan((int)a0, a2);
		if (chan && proc_sleep(chan))
			goto done_switch;
		r = sys_read((int)a0, (void*)(uintptr_t)a1, a2); break;
	}
	case SYS_write:      r = sys_write((int)a0, (const void*)(uintptr_t)a1, a2); break;
	case SYS_writev:     r = sys_writev((int)a0, a1, (int)a2); break;
	case SYS_readv:      r = sys_readv((int)a0, a1, (int)a2); break;
	case SYS__llseek:    r = sys_llseek((int)a0, a1, a2, a3, (int)a4); break;
	case SYS_lseek:      r = sys_lseek((int)a0, (int32_t)a1, (int)a2); break;
	case SYS_dup:        r = sys_dup((int)a0); break;
	case SYS_dup2:       r = sys_dup2((int)a0, (int)a1); break;
	case SYS_pipe:       r = sys_pipe2(a0, 0); break;
	case SYS_pipe2:      r = sys_pipe2(a0, (int)a1); break;
	case SYS_fcntl64:    r = sys_fcntl64((int)a0, (int)a1, a2); break;
	case SYS_getdents64: r = sys_getdents64((int)a0, a1, a2); break;
	case SYS_fstat64:    r = sys_fstat64((int)a0, a1); break;
	case SYS_stat64:     r = sys_stat64((const char*)(uintptr_t)a0, a1, 1); break;
	case SYS_lstat64:    r = sys_stat64((const char*)(uintptr_t)a0, a1, 0); break;
	case SYS_fstatat64:  r = sys_fstatat64((int)a0, (const char*)(uintptr_t)a1, a2, (int)a3); break;
	case SYS_statx:      r = sys_statx((int)a0, (const char*)(uintptr_t)a1, (int)a2, a3, a4); break;
	case SYS_faccessat:  r = sys_faccessat((int)a0, (const char*)(uintptr_t)a1, (int)a2); break;
	case SYS_faccessat2: r = sys_faccessat((int)a0, (const char*)(uintptr_t)a1, (int)a2); break;
	case SYS_access:     r = sys_faccessat(-100, (const char*)(uintptr_t)a0, (int)a1); break;
	case SYS_readlinkat: r = sys_readlinkat((int)a0, (const char*)(uintptr_t)a1, a2, a3); break;
	case SYS_chdir:      r = sys_chdir((const char*)(uintptr_t)a0); break;
	case SYS_getcwd:     r = sys_getcwd(a0, a1); break;
	case SYS_ioctl:      r = sys_ioctl((int)a0, a1, a2); break;
	/* mount/umount2: this kernel has a single ramfs and no mountable filesystems
	 * (no proc/sysfs/devtmpfs), so we can't actually mount anything. But the stock
	 * /init mounts /proc, /sys, /dev at startup and treats a failure as fatal
	 * enough to print errors. The mount points already exist as plain dirs in the
	 * initramfs, so we accept the mount as a no-op success: the dirs stay empty
	 * (honest — nothing populates them) but /init proceeds cleanly to the shell.
	 * A real proc/sysfs is future work; see PLAN. */
	case SYS_mount:      r = 0; break;
	case SYS_umount2:    r = 0; break;
	case SYS_poll: {
		/* poll(fds[], nfds, timeout): struct pollfd = {int fd; short events;
		 * short revents}. Cooperative model has no async readiness, so report
		 * every valid fd as ready for whatever it asked (revents=events); the
		 * caller then does a blocking read/write. Returns the count of ready fds. */
		uint32_t *pf = (uint32_t *)(uintptr_t)a0;   /* array of {fd,events|revents} */
		uint32_t nfds = a1; long ready = 0;
		for (uint32_t i = 0; i < nfds; i++) {
			int fd = (int)pf[i*2];
			uint16_t events = (uint16_t)(pf[i*2+1] & 0xffff);
			uint16_t rev = 0;
			if (fd >= 0 && fd_get(proc_fds(), fd)) rev = events; else rev = 0x20; /* POLLNVAL */
			pf[i*2+1] = (pf[i*2+1] & 0xffff) | ((uint32_t)rev << 16);
			if (rev) ready++;
		}
		r = ready; break;
	}

	/* ---- memory (mm_syscall.c) ---- */
	case SYS_brk:        r = sys_brk(a0); break;
	case SYS_mmap2:      r = sys_mmap2(a0, a1, (int)a2, (int)a3, (int)a4, p->tf.r[5]); break;
	case SYS_munmap:     r = sys_munmap(a0, a1); break;
	case SYS_mprotect:   r = sys_mprotect(a0, a1, (int)a2); break;
	case SYS_madvise:    r = 0; break;             /* advisory — safe no-op */

	/* ---- signals (store dispositions/mask; no delivery yet — see above) ---- */
	case SYS_rt_sigaction:   r = sys_rt_sigaction((int)a0, a1, a2, a3); break;
	case SYS_rt_sigprocmask: r = sys_rt_sigprocmask((int)a0, a1, a2, a3); break;
	case SYS_rt_sigreturn:
		/* Only reached if a handler ran, which can't happen in the
		 * cooperative model (no delivery). Getting here means a bug or an
		 * unexpected signal path — fail loudly rather than corrupt the frame. */
		printf("[kernel] pid %d: rt_sigreturn with no signal delivered\n", p->pid);
		r = -K_EINVAL_S; break;

	/* ---- time (one coarse monotonic clock: the timer tick) ---- */
	case SYS_gettimeofday: r = sys_gettimeofday(a0, a1); break;
	case SYS_clock_gettime:   r = sys_clock_gettime(a0, a1, 0); break;
	case SYS_clock_gettime64: r = sys_clock_gettime(a0, a1, 1); break;
	case SYS_sysinfo:      r = sys_sysinfo(a0); break;
	/* nanosleep: no scheduler to sleep against in the cooperative model, so
	 * return immediately as if the sleep completed (0). rem is left untouched
	 * (musl only reads it on -EINTR, which we don't return). */
	case SYS_nanosleep:
	case SYS_clock_nanosleep:
	case SYS_clock_nanosleep_time64: r = 0; break;

	/* ---- identity / process-group stubs (single-user "root", one session) ---- */
	case SYS_getppid:    r = p->parent ? p->parent->pid : 1; break;
	case SYS_getuid:  case SYS_geteuid:
	case SYS_getgid:  case SYS_getegid:
	case SYS_getuid32: case SYS_geteuid32:
	case SYS_getgid32: case SYS_getegid32: r = 0; break;   /* root */
	case SYS_setuid32: case SYS_setgid32:  r = 0; break;   /* pretend success */
	case SYS_gettid:     r = p->pid; break;
	case SYS_setpgid:    r = 0; break;
	case SYS_setsid:     r = p->pid; break;        /* new session id = pid */
	case SYS_getpgid: case SYS_getpgrp:
	case SYS_getsid:     r = p->pid; break;
	case SYS_prctl:      r = 0; break;
	case SYS_ugetrlimit:
		/* get-family: fill *a1 = struct rlimit {rlim_cur, rlim_max} (u32,u32 on
		 * arm32) with RLIM_INFINITY so callers see "no limit". */
		if (a1) { uint32_t *rl = (uint32_t *)(uintptr_t)a1; rl[0] = 0xFFFFFFFFu; rl[1] = 0xFFFFFFFFu; }
		r = 0; break;
	case SYS_setrlimit:  r = 0; break;      /* accept, ignore */
	case SYS_sched_getaffinity:
		/* (pid, cpusetsize=a1, mask=a2): one CPU online -> bit 0 set. Return the
		 * number of bytes written (glibc/musl/nproc read the popcount). */
		if (a2 && a1 >= 4) { *(uint32_t *)(uintptr_t)a2 = 1; r = 4; }
		else r = -K_EINVAL_S;
		break;
	case SYS_sched_yield: r = 0; break;
	case SYS_times:      r = 0; break;
	case SYS_restart_syscall: r = -K_EINTR; break;

	/* ---- misc ---- */
	case SYS_uname:          r = sys_uname(a0); break;
	case SYS_set_tid_address: r = p->pid; break;   /* returns tid; harmless */

	/* ARM-private syscall space (r7 = 0xf0000+n), NOT the EABI table. musl's
	 * startup calls set_tls(0xf0005) to set the user thread pointer; we write it
	 * to TPIDRURO (CP15 c13,c0,3), which __aeabi_read_tp reads back. */
	case 0xf0005: {                          /* __ARM_NR_set_tls */
		__asm__ volatile("mcr p15, 0, %0, c13, c0, 3" :: "r"(a0));
		r = 0; break;
	}

	default:
		printf("[kernel] pid %d: unimplemented syscall %u -> -ENOSYS\n", p->pid, nr);
		r = -K_ENOSYS;
		break;
	}

	p->tf.r[0] = (uint32_t)r;
done_switch:
	return proc_current_tf();                /* resume whoever `cur` is now */
}
