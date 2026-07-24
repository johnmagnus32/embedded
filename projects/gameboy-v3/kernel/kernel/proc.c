/*
 * proc.c — the S8 process model: PCB table, fork/execve/wait/exit, and a
 * minimal cooperative scheduler.
 *
 * Model (honest scope): switching happens at SYSCALL boundaries. syscall_trap
 * gets the caller's full trapframe; process syscalls copy/rewrite frames and
 * pick the next RUNNABLE process, and svc_entry/user_return resumes whatever
 * frame we hand back. Preemptive scheduling (switch on timer IRQ, per-process
 * kernel stacks) is a later refinement — flagged, not built.
 *
 * pmm<->pagetable consistency: address-space lifetime is owned entirely by vm.c
 * (vm_create / vm_copy / vm_destroy); proc.c never frees a page directly, so the
 * "free-while-mapped" class of bug can't arise here.
 */

#include <stdint.h>
#include "proc.h"
#include "vm.h"
#include "elf.h"
#include "pmm.h"
#include "file.h"
#include "ramfs.h"
#include "vfs.h"
#include "libk.h"

int printf(const char *fmt, ...);
void user_return(struct trapframe *tf);   /* start.S — resume a frame (no return) */
void vfp_save(void *dst);                 /* vfp.S — save d0..d31 + FPSCR         */
void vfp_restore(const void *src);        /* vfp.S — restore d0..d31 + FPSCR      */

static struct proc proctab[NPROC];
static struct proc *cur;
static int next_pid = 1;

/* current process's saved frame — used by the exit path in syscall_trap. */
struct trapframe *proc_current_tf(void) { return &cur->tf; }
struct proc *proc_current(void) { return cur; }

/* S9 accessors for the file-syscall layer (always the current process). */
struct fdtable  *proc_fds(void) { return &cur->fds; }
struct rf_inode *proc_cwd(void) { return cur->cwd; }
void             proc_set_cwd(struct rf_inode *ino) { cur->cwd = ino; }

void proc_init(void)
{
	for (int i = 0; i < NPROC; i++)
		proctab[i].state = P_UNUSED;
	cur = 0;
}

static struct proc *alloc_proc(void)
{
	/* Claim a free slot atomically: mark it P_EMBRYO under an IRQ-masked section
	 * so that once preemption exists a timer-driven reschedule can't observe the
	 * half-initialized slot (still P_UNUSED) and hand it out twice. In the
	 * current cooperative model there's no concurrency, but doing it right now
	 * costs nothing and removes a real hazard the moment the tick can preempt. */
	uint32_t flags = irq_save();
	struct proc *p = 0;
	for (int i = 0; i < NPROC; i++)
		if (proctab[i].state == P_UNUSED) {
			p = &proctab[i];
			p->state = P_EMBRYO;            /* reserve the slot before releasing */
			break;
		}
	irq_restore(flags);
	if (!p)
		return 0;                               /* table full */

	p->pid = next_pid++;
	p->parent = 0;
	p->exit_code = 0;
	p->killed_sig = 0;
	p->wait_chan = 0;
	p->brk = 0;
	p->brk_ceiling = MMAP_TOP;              /* safe default; load_program refines it */
	for (int v = 0; v < 33; v++) p->vfp[v] = 0;     /* clean VFP: d0..d31=0, FPSCR=0 */
	p->tls = 0;                                     /* no TLS until set_tls */
	p->sig_blocked = 0;                             /* nothing blocked initially */
	for (int s = 0; s < NSIG; s++)                  /* all signals -> SIG_DFL (0) */
		p->sigact[s].handler = p->sigact[s].flags =
		p->sigact[s].restorer = p->sigact[s].mask_lo =
		p->sigact[s].mask_hi = 0;
	return p;                                       /* still P_EMBRYO; caller sets RUNNABLE */
}

/* pick the next RUNNABLE process (round-robin after cur). Returns 0 if none. */
static struct proc *pick_next(void)
{
	int start = (int)(cur - proctab);
	for (int k = 1; k <= NPROC; k++) {
		struct proc *p = &proctab[(start + k) % NPROC];
		if (p->state == P_RUNNABLE)
			return p;
	}
	/* maybe cur itself is still runnable */
	if (cur->state == P_RUNNABLE || cur->state == P_RUNNING)
		return cur;
	return 0;
}

/* ---- Build the initial user stack for a fresh exec ------------------------ *
 * Reproduces the layout the Linux kernel builds (create_elf_tables) that musl's
 * _start_c consumes, low->high from the entry SP:
 *   [sp] argc | argv[0..argc-1] | NULL | envp[0..] | NULL | auxv{type,val}... AT_NULL
 * with the actual argv/envp STRING bytes living near the top of the page, above
 * the auxv, pointed at by the arrays. musl re-aligns sp itself, but we still
 * 16-byte-align argc's address (glibc would require it). Static binary, so the
 * auxv is minimal: AT_PAGESZ, AT_RANDOM (16 bytes musl reads for the stack /
 * pointer guard), AT_NULL.
 *
 * argv/envp are arrays of kernel-side C string pointers (NULL-terminated), each
 * pointing at a string we copy into the new stack page. Returns the entry SP
 * (user VA) or 0 on failure (strings don't fit in the one stack page). */
/* auxv types (see gv3-dynlink-abi). For a static exec only PAGESZ/RANDOM/NULL
 * matter; a dynamic exec additionally needs PHDR/PHENT/PHNUM/ENTRY/BASE so ld.so
 * can find the program + relocate it, plus UID/GID/SECURE to avoid musl's
 * "secure mode" (which would scrub the environment). */
#define AT_NULL     0
#define AT_PHDR     3
#define AT_PHENT    4
#define AT_PHNUM    5
#define AT_PAGESZ   6
#define AT_BASE     7
#define AT_ENTRY    9
#define AT_UID     11
#define AT_EUID    12
#define AT_GID     13
#define AT_EGID    14
#define AT_SECURE  23
#define AT_RANDOM  25

/* Auxv inputs the loader computes so setup_user_stack can emit the right vector.
 * For a static ET_EXEC, `dynamic` is 0 and only entry/brk matter (the stack gets
 * the minimal auxv). For a dynamic exec, these describe the PROGRAM (phdr/entry)
 * and the INTERPRETER (interp_base); the initial PC is the interpreter entry. */
struct load_result {
	uint32_t pc;          /* initial user PC: app entry (static) or ld.so entry */
	uint32_t brk_end;     /* initial program break (page-rounded app image top) */
	int      dynamic;     /* 1 if we loaded an interpreter (emit the full auxv)  */
	uint32_t phdr_va;     /* AT_PHDR: program's phdrs in memory                  */
	uint32_t phent;       /* AT_PHENT                                            */
	uint32_t phnum;       /* AT_PHNUM                                            */
	uint32_t app_entry;   /* AT_ENTRY: the program's own entry (biased)          */
	uint32_t interp_base; /* AT_BASE: where we loaded the interpreter            */
};

/* Load bases for PIE executables and the dynamic linker. Both are ET_DYN linking
 * at vaddr 0, so they need distinct, non-overlapping, 64KB-aligned bases (p_align
 * is 0x10000), clear of the ELF's own low addresses, the heap, MMAP_TOP, and the
 * stack. The app (if PIE) goes low; ld.so higher. Chosen well below MMAP_TOP. */
#define PIE_BASE     0x00200000u   /* PIE executable load base (if app is ET_DYN) */
#define INTERP_BASE  0x20000000u   /* dynamic linker load base                    */

/* Initial user stack size. One page held the argv frame for our tiny test
 * programs, but a real shell (musl) touches more than 4 KB deep almost
 * immediately, and we have no demand-paging to grow it on fault — so pre-map a
 * modest fixed region. 32 pages = 128 KiB, mapped just below USER_VA_MAX. */
#define USTACK_PAGES  32

/* Map USTACK_PAGES fresh zeroed pages ending at USER_VA_MAX into `l1`. Returns 0
 * on success, negative on OOM (rolling back what it mapped). */
static int map_user_stack(uint32_t l1)
{
	uint32_t top = USER_VA_MAX;
	for (uint32_t i = 1; i <= USTACK_PAGES; i++) {
		uint32_t va = top - i * PAGE_SIZE;
		uint32_t pa = pmm_alloc_page();
		if (!pa || vm_map_page(l1, va, pa) != 0) {
			if (pa) pmm_free_page(pa);
			for (uint32_t j = 1; j < i; j++) {
				uint32_t v = top - j * PAGE_SIZE, rp = vm_walk(l1, v);
				if (rp) { vm_unmap_page(l1, v); pmm_free_page(rp); }
			}
			return -1;
		}
	}
	return 0;
}

static uint32_t setup_user_stack(uint32_t l1, uint32_t stack_top,
                                 const char *const *argv, const char *const *envp,
                                 const struct load_result *lr)
{
	uint32_t page_va = (stack_top - 1) & ~(PAGE_SIZE - 1);
	uint32_t pa = vm_walk(l1, page_va);
	if (!pa) return 0;
	uint32_t page_lo = page_va;                 /* lowest user VA in this page */

	int argc = 0, envc = 0;
	while (argv && argv[argc]) { if (argc >= 63) return 0; argc++; }
	while (envp && envp[envc]) { if (envc >= 63) return 0; envc++; }

	/* Convert a user VA in this page to the kernel-writable physical address. */
	#define UVA2K(va)  ((uint8_t *)(uintptr_t)(pa + ((va) - page_lo)))

	/* 1) copy the strings to the very top of the page, downward; record VAs. */
	uint32_t sp = stack_top;
	uint32_t argv_va[64], envp_va[64];
	for (int i = argc - 1; i >= 0; i--) {
		uint32_t len = (uint32_t)strlen(argv[i]) + 1;
		sp -= len;
		if (sp < page_lo) return 0;             /* doesn't fit in one page */
		memcpy(UVA2K(sp), argv[i], len);
		argv_va[i] = sp;
	}
	for (int i = envc - 1; i >= 0; i--) {
		uint32_t len = (uint32_t)strlen(envp[i]) + 1;
		sp -= len;
		if (sp < page_lo) return 0;
		memcpy(UVA2K(sp), envp[i], len);
		envp_va[i] = sp;
	}

	/* 2) 16 random-ish bytes for AT_RANDOM (fixed pattern — we have no RNG). */
	sp -= 16;
	if (sp < page_lo) return 0;
	uint32_t at_random_va = sp;
	for (int i = 0; i < 16; i++) UVA2K(sp)[i] = (uint8_t)(0xA5 + i);

	/* 3) reserve the argc/argv/NULL/envp/NULL/auxv vector, 16-byte aligned so that
	 * argc's address is aligned. auxv pair count: static = 3 (PAGESZ,RANDOM,NULL);
	 * dynamic = 13 (adds PHDR,PHENT,PHNUM,ENTRY,BASE,UID,EUID,GID,EGID,SECURE). */
	uint32_t nauxpairs = (lr && lr->dynamic) ? 13u : 3u;
	uint32_t nwords = 1 + (uint32_t)argc + 1 + (uint32_t)envc + 1 + 2 * nauxpairs;
	if (sp < page_lo + nwords * 4) return 0;    /* no room (also avoids wrap) */
	uint32_t vec = (sp - nwords * 4) & ~15u;
	if (vec < page_lo) return 0;

	volatile uint32_t *w = (volatile uint32_t *)UVA2K(vec);
	uint32_t k = 0;
	w[k++] = (uint32_t)argc;
	for (int i = 0; i < argc; i++) w[k++] = argv_va[i];
	w[k++] = 0;                                 /* argv terminator */
	for (int i = 0; i < envc; i++) w[k++] = envp_va[i];
	w[k++] = 0;                                 /* envp terminator */
	/* auxv. For a dynamic exec ld.so needs the program's phdrs+entry and its own
	 * load base; UID/GID/SECURE=0 keep musl out of secure mode. We deliberately
	 * OMIT AT_SYSINFO_EHDR (no vDSO — musl then issues all syscalls via svc). */
	if (lr && lr->dynamic) {
		w[k++] = AT_PHDR;   w[k++] = lr->phdr_va;
		w[k++] = AT_PHENT;  w[k++] = lr->phent;
		w[k++] = AT_PHNUM;  w[k++] = lr->phnum;
		w[k++] = AT_ENTRY;  w[k++] = lr->app_entry;
		w[k++] = AT_BASE;   w[k++] = lr->interp_base;
		w[k++] = AT_UID;    w[k++] = 0;
		w[k++] = AT_EUID;   w[k++] = 0;
		w[k++] = AT_GID;    w[k++] = 0;
		w[k++] = AT_EGID;   w[k++] = 0;
		w[k++] = AT_SECURE; w[k++] = 0;
	}
	w[k++] = AT_PAGESZ;  w[k++] = PAGE_SIZE;
	w[k++] = AT_RANDOM;  w[k++] = at_random_va;
	w[k++] = AT_NULL;    w[k++] = 0;

	#undef UVA2K
	return vec;                                 /* entry SP points at argc */
}

/* Load an executable ELF (already resolved to `app`) into address space `l1`,
 * plus its dynamic-linker interpreter if it has one. Fills *lr with the initial
 * PC, break, and (for a dynamic exec) the auxv inputs. Returns 0 or -errno.
 *
 * Static ET_EXEC: bias 0, PC = app entry, dynamic = 0.
 * ET_DYN (PIE)  : app loaded at PIE_BASE.
 * If PT_INTERP present: resolve + load the interpreter (ld.so, an ET_DYN) at
 * INTERP_BASE; PC = interpreter entry (ld.so runs first, relocates, then jumps
 * to the program's AT_ENTRY). The kernel does NO relocation. */
static int load_program(uint32_t l1, struct rf_inode *app, struct load_result *lr)
{
	/* Peek e_type (Elf32_Ehdr offset 16, u16 LE) WITHOUT loading — an ET_DYN
	 * links at vaddr 0, so loading it at bias 0 would map page 0 (breaking
	 * null-deref protection). Choose the bias first, then load exactly once. */
	uint8_t hdr[20];
	if (ramfs_read(app, 0, hdr, sizeof(hdr)) != (long)sizeof(hdr))
		return -8;                              /* -ENOEXEC */
	uint16_t etype = (uint16_t)(hdr[16] | (hdr[17] << 8));
	uint32_t app_bias = (etype == 3 /*ET_DYN*/) ? PIE_BASE : 0;

	struct elf_info ai;
	int rc = vfs_load_elf(app, l1, app_bias, &ai);
	if (rc != 0) return rc;

	lr->dynamic     = 0;
	lr->pc          = ai.entry;
	lr->brk_end     = ai.brk_end;
	lr->phdr_va     = ai.phdr_va;
	lr->phent       = ai.phent;
	lr->phnum       = ai.phnum;
	lr->app_entry   = ai.entry;
	lr->interp_base = 0;

	if (!ai.has_interp) {
		/* No interpreter. A plain ET_EXEC is a static binary — run it directly.
		 * But an ET_DYN with no PT_INTERP is a static-PIE: we loaded it at
		 * PIE_BASE, yet without emitting AT_PHDR/AT_ENTRY it has no way to
		 * self-locate its relocations. Static-PIE is out of scope — reject it
		 * rather than run a mis-set-up process. */
		if (ai.is_dyn) { printf("exec: static-PIE unsupported\n"); return -8; }
		return 0;                               /* static ET_EXEC */
	}

	/* Dynamic: load the interpreter (ld.so) at INTERP_BASE. */
	struct rf_inode *interp = vfs_resolve(ramfs_root(), ai.interp, 1);
	if (!interp || interp->type != RF_REG) {
		printf("exec: interpreter '%s' not found\n", ai.interp);
		return -2;                              /* -ENOENT */
	}
	struct elf_info ii;
	rc = vfs_load_elf(interp, l1, INTERP_BASE, &ii);
	if (rc != 0) { printf("exec: bad interpreter %s\n", ai.interp); return rc; }
	if (!ii.is_dyn) { printf("exec: interp not ET_DYN\n"); return -8; }

	lr->dynamic     = 1;
	lr->pc          = ii.entry;                 /* run ld.so first, not the app */
	lr->interp_base = INTERP_BASE;
	/* brk stays the APP's image top; ld.so's own top isn't the program break */
	return 0;
}

/* Give a fresh process its default open files: fd 0/1/2 all bound to
 * /dev/console (falling back to no fds if the node isn't present yet). The
 * console inode is a char device; the write/read syscalls special-case it to
 * the UART. cwd starts at the root. */
static void init_stdio(struct proc *p)
{
	fdtable_init(&p->fds);
	p->cwd = ramfs_root();
	struct rf_inode *con = vfs_resolve(p->cwd, "/dev/console", 1);
	if (con) {
		struct file *f0 = file_open_inode(con, 0 /*O_RDONLY*/);
		struct file *f1 = file_open_inode(con, 1 /*O_WRONLY*/);
		struct file *f2 = file_open_inode(con, 1 /*O_WRONLY*/);
		if (f0) p->fds.fd[0] = f0;
		if (f1) p->fds.fd[1] = f1;
		if (f2) p->fds.fd[2] = f2;
	}
}

static int parse_shebang(const uint8_t *hdr, uint32_t hlen, char *interp,
                         uint32_t icap, char *arg, uint32_t acap, int *has_arg);

/* ---- spawn the first process (pid 1) from a filesystem PATH --------------- *
 * Handles the common case that /init is a `#!/bin/sh` SCRIPT, not an ELF: if so,
 * resolve the interpreter and run IT with argv = [interp, (arg), scriptpath].
 * (One shebang level — the interpreter, e.g. busybox, is a real ELF.) A plain
 * ELF is loaded directly with argv = [path]. Empty environment. */
struct proc *proc_spawn_elf(const char *path, const char *name)
{
	(void)name;
	/* resolve the target; peek for a shebang */
	struct rf_inode *ino = vfs_resolve(ramfs_root(), path, 1);
	if (!ino || ino->type != RF_REG) { printf("spawn: %s not found\n", path); return 0; }

	const char *argv[4]; int argc = 0;
	char interp[128], sharg[128];
	const char *load_path = path;

	uint8_t hdr[128];
	long got = ramfs_read(ino, 0, hdr, sizeof(hdr));
	int has_arg = 0;
	if (got >= 2 && parse_shebang(hdr, (uint32_t)got, interp, sizeof(interp),
	                              sharg, sizeof(sharg), &has_arg) == 1) {
		/* script: argv = [interp, (arg), scriptpath]; load the interpreter ELF */
		argv[argc++] = interp;
		if (has_arg) argv[argc++] = sharg;
		argv[argc++] = path;
		load_path = interp;
		ino = vfs_resolve(ramfs_root(), interp, 1);
		if (!ino || ino->type != RF_REG) { printf("spawn: interp %s not found\n", interp); return 0; }
	} else {
		argv[argc++] = path;            /* plain ELF */
	}
	argv[argc] = 0;
	const char *env0[1] = { 0 };

	struct proc *p = alloc_proc();
	if (!p) return 0;
	p->l1_pa = vm_create();
	if (!p->l1_pa) { p->state = P_UNUSED; return 0; }

	struct load_result lr;
	if (load_program(p->l1_pa, ino, &lr) != 0) {
		printf("spawn: %s not loadable\n", load_path);
		vm_destroy(p->l1_pa); p->state = P_UNUSED; return 0;
	}

	if (map_user_stack(p->l1_pa) != 0) {
		vm_destroy(p->l1_pa); p->state = P_UNUSED; return 0;
	}
	uint32_t usp = setup_user_stack(p->l1_pa, USER_VA_MAX, argv, env0, &lr);
	if (!usp) { vm_destroy(p->l1_pa); p->state = P_UNUSED; return 0; }

	p->brk_start = p->brk = lr.brk_end;
	p->brk_ceiling = lr.dynamic ? INTERP_BASE : MMAP_TOP;
	p->mmap_top  = MMAP_TOP;
	init_stdio(p);

	for (int i = 0; i < 13; i++) p->tf.r[i] = 0;
	p->tf.sp_usr = usp;
	p->tf.lr_usr = 0;
	p->tf.pc     = lr.pc;
	p->tf.cpsr   = 0x10;
	p->state = P_RUNNABLE;
	printf("proc: spawned '%s' pid %d pc 0x%x l1 0x%x%s\n",
	       load_path, p->pid, lr.pc, p->l1_pa, lr.dynamic ? " [dynamic]" : "");
	return p;
}

/* ---- run the first process (does not return) ------------------------------ */
void proc_run_first(void)
{
	for (int i = 0; i < NPROC; i++)
		if (proctab[i].state == P_RUNNABLE) {
			cur = &proctab[i];
			cur->state = P_RUNNING;
			vm_switch(cur->l1_pa);
			vfp_restore(cur->vfp);        /* load its (clean) FP state */
			__asm__ volatile("mcr p15, 0, %0, c13, c0, 3" :: "r"(cur->tls)); /* TLS */
			user_return(&cur->tf);        /* into user mode; never returns */
		}
	printf("proc: no runnable process\n");
}

/* switch the running process to `p` (saves outgoing VFP, updates cur + TTBR0,
 * restores incoming VFP). `cur` is the OUTGOING process on entry (every caller
 * switches away from the currently-running proc). Saving/restoring the VFP file
 * per switch is what lets two FP-using processes be time-sliced without
 * corrupting each other's float registers. If cur has already been torn down
 * (exit path zeroed l1_pa) we still save its VFP harmlessly into its (dead) PCB;
 * cheap and avoids a special case. */
static void switch_to(struct proc *p)
{
	if (cur) {
		vfp_save(cur->vfp);           /* stash outgoing FP state */
		/* Save the outgoing user thread pointer (TPIDRURO, CP15 c13,c0,3). It's
		 * part of the user-visible context (musl's __aeabi_read_tp reads it), so
		 * it must be per-process — otherwise two processes with different TLS
		 * pointers would clobber each other. set_tls writes the live reg, which
		 * always belongs to cur, so reading it here captures cur's current value. */
		__asm__ volatile("mrc p15, 0, %0, c13, c0, 3" : "=r"(cur->tls));
	}
	cur = p;
	p->state = P_RUNNING;
	vm_switch(p->l1_pa);
	vfp_restore(p->vfp);                  /* load incoming FP state */
	__asm__ volatile("mcr p15, 0, %0, c13, c0, 3" :: "r"(p->tls));  /* restore TLS */
}

/* Set the current process's syscall return value (into its saved frame). */
static void set_ret(long v) { cur->tf.r[0] = (uint32_t)v; }

/* ---- preemption (timer-IRQ driven, USER-MODE ONLY) ----------------------- *
 * Called from the IRQ path (irq_dispatch) with the interrupted context captured
 * in `tf`. If the interrupt landed in USER mode and another process is runnable,
 * round-robin to it: snapshot the interrupted frame into cur->tf, switch_to the
 * next proc, and return ITS frame (the IRQ path resumes that instead). If the
 * interrupt landed in the KERNEL (a syscall in progress, the idle/boot path, or
 * cur==0 during early boot), we do NOT switch — returning the same frame resumes
 * the interrupted context in place. This is what keeps the kernel non-reentrant
 * (a syscall always runs to completion) so no in-kernel locking is needed.
 *
 * Unlike the blocking switches (proc_sleep/wait), the preempted process stays
 * P_RUNNABLE and its pc is NOT rewound — it resumes exactly where the tick hit
 * it when the scheduler next picks it. */
struct trapframe *proc_preempt(struct trapframe *tf)
{
	if (!cur)
		return tf;                      /* early boot: no process yet */
	if ((tf->cpsr & 0x1f) != 0x10)
		return tf;                      /* interrupted the KERNEL — never switch */

	struct proc *nx = pick_next();
	if (!nx || nx == cur)
		return tf;                      /* nobody else to run — resume in place */

	cur->tf = *tf;                          /* save the interrupted user context */
	cur->state = P_RUNNABLE;                /* still runnable, just not running   */
	switch_to(nx);                          /* cur = nx, TTBR0 -> nx (+ TLB flush)*/
	return &cur->tf;                        /* resume nx's frame instead          */
}

/* ---- fork / clone --------------------------------------------------------- *
 * Uniform model: cur->tf already holds the parent's frame (snapshotted by
 * syscall_trap). We DON'T switch to the child here — the parent returns from
 * fork first (gets the child pid); the child is RUNNABLE and runs when the
 * parent later blocks/exits (e.g. in wait). This keeps the flow simple and
 * matches the cooperative model.
 *
 * do_fork() is the shared core: duplicate the caller into a new RUNNABLE child
 * (copied address space, shared open files, inherited cwd/brk/signal state) and
 * set the parent's return to the child pid. `child_sp`, if nonzero, overrides
 * the child's user stack pointer (clone(CLONE_VM=0) with a stack argument). */
static void do_fork(uint32_t child_sp)
{
	struct proc *child = alloc_proc();
	if (!child) { set_ret(-11); return; }   /* -EAGAIN, parent continues */

	child->l1_pa = vm_create();
	if (!child->l1_pa) { child->state = P_UNUSED; set_ret(-12); return; }
	if (vm_copy(child->l1_pa, cur->l1_pa) != 0) {
		vm_destroy(child->l1_pa); child->state = P_UNUSED; set_ret(-12); return;
	}

	child->parent = cur;
	child->tf = cur->tf;                    /* identical frame ...            */
	child->tf.r[0] = 0;                     /* ... except child sees fork()=0 */
	if (child_sp)
		child->tf.sp_usr = child_sp;    /* clone with an explicit stack   */
	fdtable_copy(&child->fds, &cur->fds);   /* share open-file descriptions   */
	child->cwd = cur->cwd;
	child->brk_start = cur->brk_start;      /* inherit the memory map layout  */
	child->brk = cur->brk;
	child->brk_ceiling = cur->brk_ceiling;
	child->mmap_top = cur->mmap_top;
	child->sig_blocked = cur->sig_blocked;  /* inherit the signal mask + acts */
	for (int s = 0; s < NSIG; s++)
		child->sigact[s] = cur->sigact[s];
	vfp_save(child->vfp);                   /* child inherits parent's LIVE FP regs */
	__asm__ volatile("mrc p15, 0, %0, c13, c0, 3" : "=r"(child->tls));  /* + live TLS */
	child->state = P_RUNNABLE;

	set_ret(child->pid);                    /* parent gets the child's pid */
}

void proc_fork(void) { do_fork(0); }

/*
 * proc_clone — the fork-flags form of clone(2) that musl uses for fork() and
 * posix_spawn(). ARM clone ABI: r0=flags, r1=child_stack, r2=ptid, r3=tls,
 * r4=ctid. We support ONLY the fork-equivalent use (a new process = new address
 * space): CLONE_VM (0x100) is NOT set. If CLONE_VM is requested (real shared-
 * memory threads), we can't honor it in this per-process-address-space kernel,
 * so return -ENOSYS and let musl fall back. A zero child_stack means "use the
 * parent's SP" (musl's fork() path), matching do_fork(0).
 */
#define CLONE_VM 0x00000100u
void proc_clone(uint32_t flags, uint32_t child_stack)
{
	if (flags & CLONE_VM) {                 /* shared-VM threads unsupported  */
		set_ret(-38);                   /* -ENOSYS (proc.c uses raw errnos) */
		return;
	}
	do_fork(child_stack);
}

/* ---- execve argument marshalling (Linux's bprm, minimally) ---------------- *
 * The caller's argv/envp are user-VA arrays of user-VA string pointers, valid
 * only while the CALLER's address space is active. So before building the new
 * address space we COPY every string into a kernel-side buffer (argstr[]), and
 * keep kernel pointer arrays (kargv/kenvp) into it. Then shebang splicing and
 * the stack builder all work on these kernel copies. */
#define EXEC_MAXARG   64
#define EXEC_ARGBUF   4096
#define EXEC_MAXDEPTH 4          /* shebang recursion bound (Linux ~4) */

struct bprm {
	const char *argv[EXEC_MAXARG + 1];   /* NULL-terminated */
	const char *envp[EXEC_MAXARG + 1];
	int   argc, envc;
	char  buf[EXEC_ARGBUF];              /* backing store for the strings */
	uint32_t used;
};

/* copy a user (or kernel) C string into bprm->buf, return a pointer to it or 0
 * if the buffer is full. `s` is dereferenced in the current address space. */
static const char *bprm_dup(struct bprm *b, const char *s)
{
	uint32_t start = b->used;
	while (b->used < EXEC_ARGBUF) {
		char c = s[b->used - start];
		b->buf[b->used++] = c;
		if (c == '\0')
			return &b->buf[start];
	}
	return 0;                            /* overflow */
}

/* snapshot a user-VA argv/envp (array of user-VA char* ptrs) into the bprm. */
static int bprm_copy_vec(struct bprm *b, uint32_t vec_uptr,
                         const char **out, int *countp)
{
	int n = 0;
	if (vec_uptr) {
		const char *const *v = (const char *const *)(uintptr_t)vec_uptr;
		while (v[n]) {
			if (n >= EXEC_MAXARG) return -1;
			const char *k = bprm_dup(b, v[n]);
			if (!k) return -1;
			out[n] = k;
			n++;
		}
	}
	out[n] = 0;
	*countp = n;
	return 0;
}

/* Commit a loaded image to the current process: adopt the new address space,
 * build its stack with argv/envp, reset the trapframe to entry. Consumes new_l1.
 * A stack-build failure happens BEFORE the address-space swap (argv/envp are
 * kernel bprm copies, still valid), so it's recoverable: tear down new_l1 and
 * set_ret(-errno) — the caller (still on its old AS) sees a failed execve. Only
 * a failure AFTER vm_switch would be irrecoverable, and there is none. */
static void exec_commit(uint32_t new_l1, const struct load_result *lr,
                        const char *const *argv, const char *const *envp)
{
	if (map_user_stack(new_l1) != 0) {
		vm_destroy(new_l1); set_ret(-12); return;
	}
	uint32_t usp = setup_user_stack(new_l1, USER_VA_MAX, argv, envp, lr);
	if (!usp) { vm_destroy(new_l1); set_ret(-7); return; }   /* -E2BIG-ish */

	uint32_t old_l1 = cur->l1_pa;
	cur->l1_pa = new_l1;
	vm_switch(new_l1);
	vm_destroy(old_l1);

	cur->brk_start = cur->brk = lr->brk_end;
	cur->brk_ceiling = lr->dynamic ? INTERP_BASE : MMAP_TOP;
	cur->mmap_top  = MMAP_TOP;
	for (int i = 0; i < 13; i++) cur->tf.r[i] = 0;
	cur->tf.sp_usr = usp;
	cur->tf.lr_usr = 0;
	cur->tf.pc     = lr->pc;
	cur->tf.cpsr   = 0x10;
}

/* Parse a `#!` first line from the file into interp path + optional single arg.
 * `hdr`/`hlen` is the first chunk of the file. Writes NUL-terminated interp and
 * (optional) arg into the provided buffers. Returns 1 if it's a shebang, 0 if
 * not, negative on a malformed shebang. (Linux: one arg, not tokenized.) */
static int parse_shebang(const uint8_t *hdr, uint32_t hlen,
                         char *interp, uint32_t icap, char *arg, uint32_t acap,
                         int *has_arg)
{
	if (hlen < 2 || hdr[0] != '#' || hdr[1] != '!')
		return 0;
	/* only the first line matters; bound to the buffer (Linux caps at 256). */
	uint32_t end = hlen < 256 ? hlen : 256;
	uint32_t i = 2;
	while (i < end && (hdr[i] == ' ' || hdr[i] == '\t')) i++;
	uint32_t is = i;
	while (i < end && hdr[i] != ' ' && hdr[i] != '\t' && hdr[i] != '\n' && hdr[i]) i++;
	uint32_t ilen = i - is;
	if (ilen == 0 || ilen >= icap) return -1;
	memcpy(interp, hdr + is, ilen); interp[ilen] = '\0';

	/* optional single argument: rest of the line after spaces, up to newline */
	while (i < end && (hdr[i] == ' ' || hdr[i] == '\t')) i++;
	uint32_t as = i;
	while (i < end && hdr[i] != '\n' && hdr[i]) i++;
	uint32_t alen = i - as;
	*has_arg = 0;
	if (alen > 0 && alen < acap) {
		memcpy(arg, hdr + as, alen); arg[alen] = '\0';
		*has_arg = 1;
	}
	return 1;
}

/* ---- execve --------------------------------------------------------------- *
 * `path`/`argv_uptr`/`envp_uptr` are user VAs in the CALLER's still-active
 * address space. We snapshot the args, resolve + load the target: a real ELF is
 * loaded directly; a `#!` script is handled by splicing the interpreter path
 * (and optional arg) in front of argv and loading the INTERPRETER instead
 * (bounded recursion). fds + cwd survive exec (POSIX). */
void proc_execve(const char *path, uint32_t argv_uptr, uint32_t envp_uptr)
{
	/* One shared bprm: it's ~8 KB (two arg buffers + pointer arrays), too big for
	 * the single kernel stack. Sharing it is safe under our concurrency model:
	 * cooperative today, and the planned first preemptive cut is USER-MODE-ONLY
	 * preemption (a syscall runs to completion before any switch), so two execves
	 * never interleave. If we ever allow preemption INSIDE a syscall, this must
	 * become per-PCB (or execve must hold a lock) — flagged, not yet needed. */
	static struct bprm b;
	b.used = 0;

	/* snapshot argv/envp from the caller's memory FIRST (before we disturb it) */
	if (bprm_copy_vec(&b, argv_uptr, b.argv, &b.argc) != 0 ||
	    bprm_copy_vec(&b, envp_uptr, b.envp, &b.envc) != 0) {
		set_ret(-7); return;                /* -E2BIG */
	}

	/* Resolve the initial path; then loop, following shebangs. `cur_path` is a
	 * kernel-side copy so it stays valid across the resolve/splice. */
	char cur_path[128];
	strlcpy_(cur_path, path, sizeof(cur_path));

	for (int depth = 0; depth < EXEC_MAXDEPTH; depth++) {
		struct rf_inode *ino = vfs_resolve(cur->cwd, cur_path, 1);
		if (!ino || ino->type != RF_REG) { set_ret(-2); return; }   /* -ENOENT */

		/* peek the first bytes to check for a shebang */
		uint8_t hdr[128];
		long got = ramfs_read(ino, 0, hdr, sizeof(hdr));
		if (got < 0) { set_ret(-8); return; }

		if (got >= 2 && hdr[0] == '#' && hdr[1] == '!') {
			char interp[128], arg[128]; int has_arg = 0;
			int r = parse_shebang(hdr, (uint32_t)got, interp, sizeof(interp),
			                      arg, sizeof(arg), &has_arg);
			if (r <= 0) { set_ret(-8); return; }

			/* Splice argv: [interp, (arg?), scriptpath, orig argv[1..]].
			 * Build a fresh vector in a temp, then copy back into b.argv. */
			const char *nv[EXEC_MAXARG + 1];
			int m = 0;
			nv[m++] = bprm_dup(&b, interp);
			if (has_arg) nv[m++] = bprm_dup(&b, arg);
			nv[m++] = bprm_dup(&b, cur_path);          /* script path as argv */
			for (int i = 1; i <= b.argc && m < EXEC_MAXARG; i++) {
				if (i == b.argc) break;                /* skip old argv[0] */
				nv[m++] = b.argv[i];
			}
			nv[m] = 0;
			for (int i = 0; i <= m; i++) b.argv[i] = nv[i];
			b.argc = m;

			/* continue: exec the interpreter next iteration */
			strlcpy_(cur_path, interp, sizeof(cur_path));
			continue;
		}

		/* real binary: load it (+ its interpreter, if dynamic) into a fresh AS */
		uint32_t new_l1 = vm_create();
		if (!new_l1) { set_ret(-12); return; }
		struct load_result lr;
		if (load_program(new_l1, ino, &lr) != 0) {
			vm_destroy(new_l1); set_ret(-8); return;   /* -ENOEXEC, caller intact */
		}
		exec_commit(new_l1, &lr, b.argv, b.envp);
		return;
	}
	set_ret(-40);                           /* -ELOOP: too many shebang levels */
}

/* ---- sleep / wakeup (block on a wait channel) ---------------------------- *
 * A syscall that must wait for another process to make progress (reading an
 * empty pipe whose writer hasn't run; a parent waiting for a child) sleeps on a
 * `chan` — any stable pointer identifying the resource. The sleeper leaves the
 * run queue (P_BLOCKED), so it is never scheduled until proc_wakeup(chan) wakes
 * it; then its `svc` re-executes (pc was rewound) and it re-checks the
 * condition. This replaces the old "stay RUNNABLE and re-poll every schedule"
 * trick, which busy-loops (fine when cooperative, a livelock once preemptive).
 *
 * The pc-rewind is still how we re-run the syscall on one shared kernel stack;
 * what changed is the process is OFF the run queue while waiting. Returns 0 (and
 * does NOT block) when no other process can run, so callers keep their existing
 * non-blocking fallback (EOF / -ECHILD) instead of deadlocking the machine. */
int proc_sleep(const void *chan)
{
	struct proc *nx = pick_next();          /* another runnable process? */
	if (!nx || nx == cur)
		return 0;                       /* nobody else to run — don't block */
	cur->wait_chan = chan;
	cur->state = P_BLOCKED;                 /* leave the run queue */
	cur->tf.pc -= 4;                        /* re-execute the svc when woken */
	switch_to(nx);
	return 1;
}

void proc_wakeup(const void *chan)
{
	for (int i = 0; i < NPROC; i++) {
		struct proc *p = &proctab[i];
		if (p->state == P_BLOCKED && p->wait_chan == chan) {
			p->wait_chan = 0;
			p->state = P_RUNNABLE;
		}
	}
}

/* ---- exit ----------------------------------------------------------------- */
/* Turn `cur` into a zombie, release its resources, switch to the next runnable
 * process, and reclaim the dead address space. Shared by the exit syscall and
 * the fault-kill path (fault_trap). Does not return normally: either it
 * switch_to()s another process (and syscall_trap/user_return resumes cur=nx) or
 * it halts when nothing else can run. */
static void reap_current_and_switch(int code, int sig)
{
	cur->exit_code = code;
	cur->killed_sig = sig;                  /* 0 = normal exit; else the signal */
	cur->state = P_ZOMBIE;
	fdtable_closeall(&cur->fds);            /* drop this process's open files */
	uint32_t dead_l1 = cur->l1_pa;          /* free AFTER we switch off it */
	cur->l1_pa = 0;

	/* REPARENT our children (and orphaned zombies) to init before we go. Parent
	 * identity is a raw PCB pointer; if we left our children pointing at this
	 * soon-to-be-freed slot, a later slot reuse would make their ->parent dangle
	 * and alias a different process (mis-harvest / spurious wakeup). init (pid 1)
	 * never exits, so its PCB pointer stays valid. An already-ZOMBIE orphan would
	 * otherwise leak its slot forever; handing it to init lets init reap it.
	 * (If cur IS init, there's no reparent target — the system is ending; the
	 * pick_next below will find nothing and halt.) */
	struct proc *initp = 0;
	for (int i = 0; i < NPROC; i++)
		if (proctab[i].pid == 1 && proctab[i].state != P_UNUSED) { initp = &proctab[i]; break; }
	if (initp && initp != cur) {
		for (int i = 0; i < NPROC; i++) {
			struct proc *c = &proctab[i];
			if (c != cur && c->state != P_UNUSED && c->parent == cur) {
				c->parent = initp;
				if (c->state == P_ZOMBIE)
					proc_wakeup(initp);   /* init may be waiting; let it reap */
			}
		}
	}

	/* Wake our parent if it's blocked in wait4 (it sleeps on its own PCB). Must
	 * happen BEFORE pick_next so a parent that's the only other process becomes
	 * P_RUNNABLE and gets scheduled to reap us — otherwise pick_next sees no
	 * runnable process and halts with a zombie unreaped. */
	if (cur->parent)
		proc_wakeup(cur->parent);

	struct proc *nx = pick_next();          /* uses cur's index — call first */
	if (!nx) {
		/* Nothing else to run. Don't free dead_l1 — TTBR0 still points at it
		 * (no address space to switch to) and we're about to halt forever, so
		 * reclaiming it would be a use-after-free on the live page tables for
		 * no benefit. */
		printf("[kernel] last process exited; halting.\n");
		for (;;) __asm__ volatile("wfi");
	}
	switch_to(nx);                          /* TTBR0 -> nx (+ TLB flush) ... */
	vm_destroy(dead_l1);                    /* ... now safe to reclaim old AS */
	/* caller's return path resumes cur=nx */
}

void proc_exit(int code)
{
	reap_current_and_switch(code, 0);       /* normal exit: no signal */
}

/* ---- fault handling (undef / prefetch abort / data abort) ----------------- *
 * Called from the exception vectors (start.S) with the faulting context already
 * captured into a `struct trapframe` on the abort/undef stack. kind: 0=undef,
 * 1=prefetch-abort, 2=data-abort. Returns the trapframe to resume (user_return
 * restores it) — which, for a killed user process, is the NEXT runnable
 * process's frame.
 *
 * A fault from USER mode is that process's own bug: report it, kill the process
 * (exit code 128+signal, mirroring how a shell reports a killed child), and
 * reschedule — the rest of the system keeps running. A fault from a privileged
 * mode is a KERNEL bug (a bad pointer in a syscall handler, etc.): there is no
 * process to blame and continuing would corrupt state, so we panic (halt).
 *
 * Safe under the current cooperative model: entry masked IRQs, and this code
 * only touches identity-mapped kernel memory + the UART, so it never faults
 * (which would clobber the single abort stack). */
struct trapframe *fault_trap(uint32_t kind, struct trapframe *tf)
{
	static const char *nm[3] = { "undefined-instruction", "prefetch-abort",
	                             "data-abort" };
	const char *what = nm[kind < 3 ? kind : 0];

	/* A fault before the first process is scheduled (cur==0) is an early-boot
	 * kernel bug. We must NOT dereference cur (it would write through 0, which
	 * isn't mapped -> a recursive data abort that eventually overflows the abort
	 * stack). Report from the passed-in frame and halt. */
	if (!cur) {
		printf("\n*** KERNEL PANIC: %s before any process (cur=0) ***\n", what);
		printf("    pc=0x%x cpsr=0x%x\n", tf->pc, tf->cpsr);
		for (;;) __asm__ volatile("wfi");
	}

	cur->tf = *tf;                          /* snapshot into the PCB */

	/* Fault address + status from CP15 (verified encodings): data abort uses
	 * DFAR/DFSR, prefetch abort uses IFAR/IFSR; undef has neither. */
	uint32_t far = 0, fsr = 0;
	if (kind == 2) {
		__asm__ volatile("mrc p15,0,%0,c6,c0,0" : "=r"(far));   /* DFAR */
		__asm__ volatile("mrc p15,0,%0,c5,c0,0" : "=r"(fsr));   /* DFSR */
	} else if (kind == 1) {
		__asm__ volatile("mrc p15,0,%0,c6,c0,2" : "=r"(far));   /* IFAR */
		__asm__ volatile("mrc p15,0,%0,c5,c0,1" : "=r"(fsr));   /* IFSR */
	}

	uint32_t from_mode = cur->tf.cpsr & 0x1f;
	int from_user = (from_mode == 0x10);    /* USR mode = 0x10 */

	if (!from_user) {
		/* KERNEL fault — unrecoverable. Dump enough to locate it, then halt.
		 * NB: the trapframe's sp/lr slots always hold the USER-banked r13/r14
		 * (the `stmia {r13,r14}^` in the vector runs regardless of faulting
		 * mode), so for a kernel fault they are NOT the SVC sp/lr where the bug
		 * lives — label them usr_sp/usr_lr honestly. The faulting kernel frame is
		 * found from pc + a disassembly, not from these. */
		printf("\n*** KERNEL PANIC: %s in privileged mode ***\n", what);
		printf("    pc=0x%x cpsr=0x%x (mode 0x%x)\n",
		       cur->tf.pc, cur->tf.cpsr, from_mode);
		if (kind) printf("    fault addr=0x%x status=0x%x\n", far, fsr);
		printf("    r0=0x%x r1=0x%x r2=0x%x r3=0x%x usr_sp=0x%x usr_lr=0x%x\n",
		       cur->tf.r[0], cur->tf.r[1], cur->tf.r[2], cur->tf.r[3],
		       cur->tf.sp_usr, cur->tf.lr_usr);
		for (;;) __asm__ volatile("wfi");
	}

	/* USER fault: report + kill this process, then reschedule. SIGSEGV=11 for
	 * aborts, SIGILL=4 for an undefined instruction; wait status is 128+signal
	 * by convention (what a shell prints for a killed child). */
	int sig = (kind == 0) ? 4 : 11;
	printf("[kernel] pid %d killed by %s (pc=0x%x", cur->pid, what, cur->tf.pc);
	if (kind) printf(", addr=0x%x", far);
	printf(")\n");

	reap_current_and_switch(0, sig);        /* zombie (killed by sig) + switch */
	return proc_current_tf();               /* resume the process we switched to */
}

/* ---- wait4 ---------------------------------------------------------------- *
 * Find a ZOMBIE child and harvest it. If a child is still RUNNABLE (hasn't run
 * yet — the usual case right after fork), switch to it and mark the parent
 * RUNNABLE; the parent's wait re-runs when the child exits and picks the parent
 * again. status_uptr is a user VA in the CURRENT (parent's) address space, which
 * is active here when we harvest — so the write lands in the parent's memory. */
void proc_wait(uint32_t status_uptr)
{
	for (int i = 0; i < NPROC; i++) {
		struct proc *c = &proctab[i];
		if (c->state == P_ZOMBIE && c->parent == cur) {
			if (status_uptr) {
				/* POSIX wait status: killed-by-signal -> low 7 bits = signal
				 * number (WIFSIGNALED); normal exit -> (code & 0xff) << 8
				 * (WIFEXITED, WEXITSTATUS in bits 15:8). */
				int st = c->killed_sig ? (c->killed_sig & 0x7f)
				                       : ((c->exit_code & 0xff) << 8);
				*(volatile int *)(uintptr_t)status_uptr = st;
			}
			int pid = c->pid;
			c->state = P_UNUSED;            /* reap the PCB slot */
			set_ret(pid);
			return;
		}
	}
	/* No zombie yet. If any live child exists, SLEEP on our own PCB until a child
	 * exits (proc_exit / fault-kill calls proc_wakeup(parent)); the wait then
	 * re-runs and harvests the zombie. Sleeping on `cur` (a stable pointer) means
	 * the parent leaves the run queue instead of busy re-polling. */
	int have_child = 0;
	for (int i = 0; i < NPROC; i++) {
		struct proc *c = &proctab[i];
		if (c->parent == cur && c->state != P_UNUSED) { have_child = 1; break; }
	}
	if (have_child && proc_sleep(cur))
		return;                         /* slept; wait re-runs when woken       */
	set_ret(-10);                           /* -ECHILD: no children (or none can run) */
}
