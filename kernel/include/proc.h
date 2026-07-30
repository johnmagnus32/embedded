/*
 * proc.h — the S8 process model: trap frame, PCB, and process ops.
 *
 * Context-switch model (deliberately simple, honest about scope): ONE kernel
 * stack, cooperative switching at SYSCALL boundaries only. Each process's full
 * user register state is captured in a `trapframe` (built by svc_entry) and
 * copied into its PCB when we switch away; switching in rewrites the on-stack
 * frame with the target PCB's saved frame and swaps TTBR0. This is enough for
 * fork/exec/wait (every process makes syscalls); true PREEMPTIVE scheduling
 * (switch on the timer IRQ, per-process kernel stacks) is a later refinement.
 */
#ifndef GV3K_PROC_H
#define GV3K_PROC_H

#include <stdint.h>
#include "file.h"

struct rf_inode;

/*
 * trapframe — the full user context, laid out to MATCH how svc_entry pushes it
 * on the kernel stack (low address -> high):
 *   sp_usr, lr_usr, r0..r12, pc(return), cpsr(spsr)
 * = 17 words. svc_entry passes a pointer to this to the C dispatcher.
 */
struct trapframe {
	uint32_t sp_usr;   /* user r13 */
	uint32_t lr_usr;   /* user r14 */
	uint32_t r[13];    /* r0..r12  */
	uint32_t pc;       /* return address (lr_svc) */
	uint32_t cpsr;     /* saved user CPSR (spsr_svc) */
};

/* P_EMBRYO: slot claimed by alloc_proc but not yet fully initialized/runnable —
 * so a second allocator (once preemption exists) can't hand out the same slot.
 * P_BLOCKED: waiting on a resource (wait_chan) — NOT eligible to be scheduled
 * until proc_wakeup(chan) marks it P_RUNNABLE again. Replaces the old
 * "stay RUNNABLE and busy re-poll" trick, which livelocks under preemption. */
enum pstate { P_UNUSED = 0, P_EMBRYO, P_RUNNABLE, P_RUNNING, P_BLOCKED, P_ZOMBIE };

/*
 * ksig — one signal disposition, laid out to MATCH the ARM rt_sigaction user
 * `struct sigaction` (20 bytes: sa_handler, sa_flags, sa_restorer, then the
 * 8-byte sa_mask), so rt_sigaction can memcpy act/oldact straight through.
 * S10 STORES dispositions so musl's setup + queries succeed; it does not yet
 * DELIVER signals (no async signal source in the cooperative model), so the
 * handler is recorded but never invoked. See syscall.c.
 */
#define NSIG 65                              /* signals 1..64 (index 0 unused) */
struct ksig {
	uint32_t handler;                    /* sa_handler / SIG_DFL(0) / SIG_IGN(1) */
	uint32_t flags;                      /* sa_flags */
	uint32_t restorer;                   /* sa_restorer (SA_RESTORER trampoline) */
	uint32_t mask_lo, mask_hi;           /* sa_mask (64-bit) */
};

struct proc {
	int              pid;
	enum pstate      state;
	uint32_t         l1_pa;      /* this process's address space (TTBR0) */
	struct proc     *parent;
	int              exit_code;  /* valid when ZOMBIE (normal exit) */
	int              killed_sig; /* if nonzero, ZOMBIE was killed by this signal */
	struct trapframe tf;         /* saved user frame when not RUNNING */
	uint32_t         brk_start;  /* S10: initial break (page-rounded ELF top) */
	uint32_t         brk;        /* S10: current program break (grows up) */
	uint32_t         brk_ceiling;/* heap must not grow to/past this VA (the next
	                              * region above the heap: the dynamic interpreter
	                              * at INTERP_BASE if present, else mmap_top) */
	uint32_t         mmap_top;   /* S10: next anon mmap goes just below this */
	struct fdtable   fds;        /* S9: open file descriptors */
	struct rf_inode *cwd;        /* S9: current working directory */
	const void      *wait_chan;  /* if P_BLOCKED: the resource being waited on */
	uint64_t         sig_blocked;        /* S10: rt_sigprocmask blocked mask */
	struct ksig      sigact[NSIG];        /* S10: per-signal dispositions */
	/* VFP/NEON state saved across a context switch: d0..d31 (32*8) + FPSCR (4),
	 * padded to 8-align. uint64_t element => the struct is 8-aligned, which the
	 * vstmia/vldmia in vfp.S want. See switch_to / vfp_save / vfp_restore. */
	uint64_t         vfp[33];            /* [0..31]=d0..d31, [32] low word=FPSCR */
	uint32_t         tls;                /* user thread pointer (TPIDRURO); set by
	                                      * __ARM_NR_set_tls, saved/restored on switch */
};

#define NPROC 8

void   proc_init(void);
struct proc *proc_current(void);

/* Create the first user process (pid 1) from a filesystem path; handles a
 * `#!` script target by running its interpreter. Returns its PCB (0 on failure). */
struct proc *proc_spawn_elf(const char *path, const char *name);

/* Syscall implementations. Uniform model: each operates on the current PCB's
 * saved frame (cur->tf) and may change `cur` to context-switch; syscall_trap
 * then resumes proc_current_tf(). No trapframe parameter needed. */
void proc_fork(void);                       /* parent r0=pid; may switch to child */
void proc_clone(uint32_t flags, uint32_t child_stack); /* fork-flags clone(2)   */
void proc_execve(const char *path, uint32_t argv_uptr, uint32_t envp_uptr);
void proc_wait(uint32_t status_uptr);       /* status_uptr = user int* (0 if none) */
void proc_exit(int code);                   /* zombie + switch to next runnable */

/* current process's saved frame (what syscall_trap resumes). */
struct trapframe *proc_current_tf(void);

/* Timer-IRQ preemption seam (start.S irq_entry -> kmain irq_dispatch -> here).
 * Given the interrupted context `tf`, round-robin to another runnable process IF
 * the interrupt hit USER mode; otherwise return `tf` unchanged (never preempt the
 * kernel). Returns the frame the IRQ path should resume. See proc.c. */
struct trapframe *proc_preempt(struct trapframe *tf);

/* Sleep the current process on a wait channel `chan` (any stable pointer that
 * identifies the resource, e.g. a pipe inode or a PCB). Marks it P_BLOCKED so it
 * leaves the run queue entirely (no busy re-poll), rewinds its pc so the syscall
 * re-runs when woken, and switches to another runnable process. When later woken
 * (proc_wakeup(chan)) it becomes P_RUNNABLE and re-executes the syscall.
 * Returns 1 if it slept+switched; 0 if NO other process can run — in which case
 * it does NOT block (sleeping would deadlock the machine) and the caller must
 * handle the condition without waiting (e.g. return EOF / -ECHILD). */
int proc_sleep(const void *chan);

/* Wake every process blocked on `chan` (make them P_RUNNABLE). Safe to call with
 * no sleepers. Does not switch — the woken processes run when the scheduler next
 * picks them. */
void proc_wakeup(const void *chan);

/* IRQ-masked critical-section primitive (start.S), nestable — unlike the plain
 * irq_enable/irq_disable. irq_save() masks IRQs and returns the prior CPSR;
 * irq_restore(flags) restores it (re-enabling IRQs only if they were enabled
 * before the matching save). Used to make scheduler state changes atomic w.r.t.
 * the timer IRQ once preemption is enabled. */
uint32_t irq_save(void);
void     irq_restore(uint32_t flags);

/* S9 accessors used by the file syscalls (all operate on the CURRENT process). */
struct fdtable  *proc_fds(void);
struct rf_inode *proc_cwd(void);
void             proc_set_cwd(struct rf_inode *ino);

/* Enter the first user process (does not return). */
void proc_run_first(void);

#endif /* GV3K_PROC_H */
