/*
 * start.S — S5 kernel entry, exception vector table, and IRQ plumbing.
 *
 * Loaded by the bootloader into DRAM at 0x41000000 (MMU off, DRAM already up).
 * We: get into a known CPU state, set up SVC + IRQ stacks, install the vector
 * table via VBAR, zero BSS, then call kmain(). Interrupts are enabled by C code
 * (irq_enable) once the GIC + timer are configured.
 *
 * All register/encoding values verified against the ARM ARMv7-A ARM + Cortex-A7
 * TRM: CPSR modes (irq=0x12, svc=0x13), I-bit=7, VBAR = p15 c12 c0 0 (32-byte
 * aligned), IRQ return = subs pc, lr, #4.
 */

/* ---- zImage-compatible boot header --------------------------------------- *
 * Make this kernel present the SAME interface to a loader as the mainline Linux
 * zImage, so U-Boot's `bootz` (and any zImage-aware loader) boots it identically
 * — one image format, one boot command, for both our kernel and mainline Linux.
 *
 * A loader enters at the load address (offset 0x00); we branch to _start. bootz
 * checks a magic word at offset 0x24 and boots the image IN PLACE at the load
 * address — it does NOT relocate it (verified: bootz_run runs no
 * BOOTM_STATE_LOADOS). Our kernel is position-DEPENDENT (linked at 0x41000000),
 * so this is safe precisely because the boot script loads us at that link
 * address; a real zImage tolerates any address because its decompressor is
 * position-independent — we don't need that, since we control the load address.
 *
 * STRUCTURE — how this differs from mainline arch/arm, deliberately:
 *   - Mainline's zImage magic belongs to the DECOMPRESSOR stage
 *     (arch/arm/boot/compressed/head.S), a separate self-decompressing program
 *     that wraps the raw kernel `Image` — arch/arm/kernel/head.S carries NO magic.
 *     We have no decompressor (our kernel is small and uncompressed), so the
 *     equivalent of `Image` self-identifies as a zImage. A deliberate
 *     simplification, not a port of the decompressor.
 *   - Mainline defines the field VALUES in the linker script (vmlinux.lds.S:
 *     _magic_sig/_magic_start/_magic_end) and head.S emits `.word <symbol>`. We
 *     use literals here instead of linker symbols ON PURPOSE: this kernel links
 *     as a (position-dependent) PIE, so a `.word <absolute-symbol>` emits an
 *     R_ARM_RELATIVE dynamic relocation — dead metadata nothing processes in a
 *     freestanding kernel, and it bloats the image. A literal bakes the value in
 *     with no relocation. (zi_end still needs a link-time value, so we compute
 *     _image_end - _image_start as an ASSEMBLE-relative difference, which the
 *     assembler folds to a constant — no relocation.)
 *
 * Layout matches U-Boot's struct arm_z_header (arch/arm/lib/zimage.c):
 *   0x00      b _start          entry (mainline: 8 nops + branch; we only branch)
 *   0x04-0x23 (8 reserved words) pad up to the magic
 *   0x24      0x016f2818        zi_magic (LINUX_ARM_ZIMAGE_MAGIC; LE, no swap)
 *   0x28      0                 zi_start, 0-relative — matches a real zImage
 *                               byte-for-byte (its PIC decompressor _start links
 *                               at base 0, so mainline's _magic_start is 0 too)
 *   0x2c      _image_end - _image_start   zi_end = loadable image size
 * bootz uses only (zi_end - zi_start) as the number of bytes to reserve at the
 * load address, so a 0-relative start with end = image size is exactly right.
 */
.section .text.head, "ax"
.global _image_start
_image_start:
	b       _start                /* 0x00: loader enters here -> real entry   */
	.word   0, 0, 0, 0, 0, 0, 0, 0 /* 0x04..0x23: reserved (code[1..8])        */
	.word   0x016f2818            /* 0x24: zi_magic (LINUX_ARM_ZIMAGE_MAGIC)   */
	.word   0                     /* 0x28: zi_start (0-relative, as real zImage)*/
	.word   _image_end - _image_start /* 0x2c: zi_end = loadable image size    */

/* ---- exception vector table (must be 32-byte aligned for VBAR) ----------- */
.section .text.boot, "ax"
.align 5                          /* 2^5 = 32-byte alignment */
.global _vectors
_vectors:
	ldr pc, =_start               /* 0x00 Reset            */
	ldr pc, =undef_handler        /* 0x04 Undefined instr  */
	ldr pc, =svc_entry            /* 0x08 Supervisor call (syscall) */
	ldr pc, =pabt_handler         /* 0x0C Prefetch abort   */
	ldr pc, =dabt_handler         /* 0x10 Data abort       */
	ldr pc, =hang_handler         /* 0x14 Reserved/Hyp     */
	ldr pc, =irq_entry            /* 0x18 IRQ              */
	ldr pc, =fiq_handler          /* 0x1C FIQ              */

/* ---- reset / entry ------------------------------------------------------- */
.global _start
_start:
	/* Save the boot handoff registers the bootloader/U-Boot passed us BEFORE we
	 * clobber them (r2 = DTB physical address per Documentation/arm/booting.rst;
	 * r1 = machine type). We can't touch DRAM-backed .data yet in a general
	 * bootloader, but our kernel runs from DRAM (already up), so stashing to a
	 * .data word is safe here. kmain reads it via boot_dtb(). */
	ldr     r3, =_boot_r2
	str     r2, [r3]
	ldr     r3, =_boot_r1
	str     r1, [r3]

	/* Enter SVC mode, mask IRQ+FIQ (CPSR: mode 0x13, I=1 F=1 -> 0xD3). */
	msr     cpsr_c, #0xd3

	/* Ensure MMU + caches off (bootloader left them off; be explicit). */
	mrc     p15, 0, r0, c1, c0, 0
	bic     r0, r0, #(1 << 0)     /* M:  MMU off        */
	bic     r0, r0, #(1 << 2)     /* C:  D-cache off    */
	bic     r0, r0, #(1 << 12)    /* I:  I-cache off    */
	bic     r0, r0, #(1 << 13)    /* V:  low vectors (use VBAR, not 0xFFFF0000) */
	mcr     p15, 0, r0, c1, c0, 0
	isb

	/* Enable SMP coherency (ACTLR.SMP, bit 6) — REQUIRED on Cortex-A7 BEFORE any
	 * cache/MMU enable or set/way maintenance. With SMP=0 the data cache is
	 * effectively disabled for coherency and cache-maintenance ops are
	 * architecturally UNPREDICTABLE (Cortex-A7 TRM 4.3.11): our set/way invalidate
	 * below and the later caches-on path would be undefined on real silicon (QEMU
	 * ignores ACTLR, so this only bites the T113). Even single-core needs it set —
	 * it's what makes the D-cache function correctly, not just SMP snooping.
	 * U-Boot normally sets it, but the FEL/custom-loader path may not, so do it
	 * ourselves. Must precede mmu_flush_caches_tlb (which does set/way ops). */
	mrc     p15, 0, r0, c1, c0, 1     /* ACTLR                                   */
	orr     r0, r0, #(1 << 6)         /* SMP = 1                                 */
	mcr     p15, 0, r0, c1, c0, 1
	isb

	/* Scrub all cache + TLB state the loader may have left. With caches now OFF
	 * (SCTLR.C/I cleared above), invalidate the D-cache (by set/way), I-cache,
	 * branch predictor, and TLBs, so we do NOT inherit stale lines from whoever
	 * loaded us. Our own bootloader runs caches-off (clean handoff), but U-Boot
	 * hands off with a populated D-cache — without this, stale lines shadowed the
	 * freshly-built L1 table and mmu_selftest read back MISMATCH. Must run BEFORE
	 * mmu_init builds/enables the MMU.
	 * mmu_flush_caches_tlb push/pops, so it needs a stack: point SP at the SVC
	 * stack top now (it's reserved space past _bss_end in kernel.ld — valid DRAM,
	 * safe to use before .bss is zeroed; the banked-stack setup below re-sets it). */
	ldr     sp, =_svc_stack_top
	bl      mmu_flush_caches_tlb

	/* Install the vector table base (VBAR). */
	ldr     r0, =_vectors
	mcr     p15, 0, r0, c12, c0, 0
	isb

	/* Enable the VFP/NEON unit. The KERNEL doesn't use FP (-mgeneral-regs-only),
	 * but USER programs do: the unmodified hardfloat musl BusyBox executes VFP
	 * instructions (e.g. `vpush {d8}`), which trap as Undefined unless the FPU is
	 * on. Two steps: (1) CPACR grant CP10+CP11 full access; (2) set FPEXC.EN.
	 * Single VFP context for now (cooperative; no per-process VFP save yet). */
	mrc     p15, 0, r0, c1, c0, 2     /* CPACR */
	orr     r0, r0, #(0xf << 20)      /* CP10 + CP11 = full access (PL0+PL1) */
	mcr     p15, 0, r0, c1, c0, 2
	isb
	mov     r0, #(1 << 30)            /* FPEXC.EN */
	vmsr    fpexc, r0                 /* enable the VFP unit */
	isb

	/* Set up the per-mode banked stacks, then return to SVC and set its stack.
	 * Abort mode (0x17) is entered by BOTH Prefetch and Data Abort; Undefined
	 * mode (0x1B) by the Undefined-instruction exception. Their banked SPs are
	 * UNKNOWN at reset, so a fault handler's first push/bl would write through
	 * garbage unless we initialize them here (verified: ARM ARM B1.3.2). */
	cps     #0x12                 /* IRQ mode  */
	ldr     sp, =_irq_stack_top
	cps     #0x17                 /* Abort mode */
	ldr     sp, =_abt_stack_top
	cps     #0x1b                 /* Undefined mode */
	ldr     sp, =_und_stack_top
	cps     #0x13                 /* SVC mode */
	ldr     sp, =_svc_stack_top

	/* Zero .bss */
	ldr     r0, =_bss_start
	ldr     r1, =_bss_end
	mov     r2, #0
1:	cmp     r0, r1
	bcs     2f
	str     r2, [r0], #4
	b       1b
2:
	bl      kmain                 /* into C; should not return */
hang:
	b       hang

/* ---- IRQ entry: build a full trapframe, dispatch, resume (maybe switched) - *
 * The timer IRQ can PREEMPT a user process. To switch to a different process we
 * must save the COMPLETE interrupted context (not just scratch regs) in the same
 * `struct trapframe` layout svc_entry uses, and be able to resume EITHER the
 * interrupted frame (kernel/idle context, or preemption off) or a DIFFERENT
 * process's frame — via the shared user_return tail.
 *
 * IRQ entry state (verified vs ARM ARM): mode=IRQ(0x12), I=1 (IRQs masked, so the
 * handler is not re-entered — one IRQ stack suffices), F unchanged; LR_irq =
 * return_addr + 4, so correct it by -4 first. r0-r12 are the interrupted values
 * (r8-r12 are only FIQ-banked, not IRQ-banked); r13/r14 are IRQ-banked, so the
 * ^ form of stm/ldm gets the USER sp/lr. Build the frame on SP_irq, hand &frame
 * to irq_dispatch(tf), which returns the frame to resume (same one, or another
 * process's when it preempts). Then user_return restores it and RFEs — RFE sets
 * CPSR from the frame's saved word, so a user frame (cpsr=0x10) lands in user
 * mode even though we execute the RFE while physically in IRQ mode. */
.global irq_entry
irq_entry:
	sub     lr, lr, #4            /* LR_irq -> the instruction to resume       */
	ldr     sp, =_irq_stack_top   /* reset IRQ stack (a prior preempt left SP_irq
	                               * pointing into a PCB via user_return's mov sp;
	                               * mirrors svc_entry's stack reset)           */
	srsdb   sp!, #0x12            /* push {pc=lr_irq, cpsr=spsr_irq}           */
	stmfd   sp!, {r0-r12}         /* push r0..r12 (interrupted values)         */
	sub     sp, sp, #8            /* room for sp_usr, lr_usr (lowest 2 words)  */
	stmia   sp, {r13, r14}^       /* store USER-banked sp/lr into slots 0,1    */
	nop                           /* (^ form: no writeback; needs a gap insn)  */

	mov     r0, sp                /* r0 = &trapframe (arg 1)                   */
	bic     sp, sp, #4            /* 8-align SP for the C call (frame is 68B    */
	                              /* from an 8-aligned top -> 4 mod 8). Frame   */
	                              /* ptr kept in r0; we don't pop (b user_return). */
	bl      irq_dispatch          /* -> C: returns &frame to resume            */
	b       user_return           /* resume that frame (interrupted or switched) */

/* ---- fault handlers: build a trapframe, hand off to C ---------------------- *
 * Undefined-instruction, Prefetch-Abort and Data-Abort each enter their banked
 * mode (Undef=0x1B; both aborts share Abort=0x17) with a banked LR pointing PAST
 * the faulting instruction (ARM state, verified vs ARM ARM B1.8.x):
 *   Undef        lr = fault + 4   (faulting instr = lr - 4)
 *   Prefetch Abt lr = fault + 4   (faulting instr = lr - 4)
 *   Data Abort   lr = fault + 8   (faulting instr = lr - 8)
 * We first correct LR to the faulting PC, then build the SAME `struct trapframe`
 * layout svc_entry uses (sp_usr, lr_usr, r0-r12, pc, cpsr) and call
 *   struct trapframe *fault_trap(uint32_t kind, struct trapframe *tf);
 * which either kills the faulting user process and returns the next process's
 * frame (resumed via user_return), or panics for a kernel-mode fault. kind:
 * 0=undef, 1=prefetch-abort, 2=data-abort.
 *
 * On entry the CPU has masked IRQs (I=1) for all three, so these handlers are
 * not re-entered by an IRQ; the single Abort stack is safe as long as the
 * handler never itself faults (it only touches identity-mapped kernel memory +
 * the UART). FIQ is NOT masked on abort/undef entry, but fiq_handler is a bare
 * spin that never uses a stack (sp_fiq is intentionally left uninitialized), so
 * an FIQ can't corrupt anything here — it would just hang, and no FIQ source is
 * enabled (only the timer IRQ). If FIQ is ever used, init sp_fiq in _start.
 */
.global undef_handler, pabt_handler, dabt_handler, fiq_handler, hang_handler

/* Build the full trapframe and hand off to C. On entry to this macro: LR has
 * been corrected to the faulting instruction, the banked SP has been reset to
 * its stack top, and SRSDB has pushed {pc,cpsr}. We save r0..r12 (the user's
 * REAL values — untouched so far) and the user-banked sp/lr, and ONLY THEN load
 * `kind` into r0 (arg 1) — the user's r0 is already safe in the frame. \kind is
 * an assembler-time immediate (0/1/2). */
.macro FAULT_TAIL kind
	stmfd   sp!, {r0-r12}         /* push r0..r12 (r0 lowest) — real user regs */
	sub     sp, sp, #8            /* room for sp_usr, lr_usr (lowest 2 words)  */
	stmia   sp, {r13, r14}^       /* store USER-banked sp/lr into slots 0,1    */
	nop                           /* (^ form: legacy pipeline gap; harmless)   */
	mov     r1, sp                /* r1 = &trapframe (arg 2)                    */
	bic     sp, sp, #4            /* 8-align SP for the C call (AAPCS). The frame
	                               * is 68 bytes from an 8-aligned top, so SP is
	                               * 4 mod 8 here; round down. Frame ptr kept in
	                               * r1; we never pop (we `b user_return`).       */
	mov     r0, #\kind            /* r0 = kind (arg 1); user r0 already saved   */
	bl      fault_trap            /* C: returns &frame to resume (or panics)   */
	b       user_return           /* resume the returned frame (next process)  */
.endm

/* Each handler MUST reset its banked SP before SRSDB: user_return (reached via
 * the tail's `b user_return`) does `mov sp, r0` while STILL in this abort/undef
 * mode, leaving the banked SP pointing into a PCB — so without the reset the
 * NEXT fault of this class would build its frame on top of a live PCB. Reloading
 * the stack top here mirrors svc_entry's `ldr sp,=_svc_stack_top`. Safe: entry
 * masks IRQs and each handler is single-shot per fault. */
undef_handler:
	sub     lr, lr, #4            /* LR -> faulting instruction (ARM state)    */
	ldr     sp, =_und_stack_top   /* reset undefined-mode banked SP (essential)*/
	srsdb   sp!, #0x1b            /* push {pc=lr_und, cpsr=spsr_und}           */
	FAULT_TAIL 0                  /* kind = undef */

pabt_handler:
	sub     lr, lr, #4            /* LR -> aborting instruction                */
	ldr     sp, =_abt_stack_top   /* reset abort-mode banked SP (essential)    */
	srsdb   sp!, #0x17            /* push {pc, cpsr}                            */
	FAULT_TAIL 1                  /* kind = prefetch abort */

dabt_handler:
	sub     lr, lr, #8            /* LR -> aborting instruction (data abort)   */
	ldr     sp, =_abt_stack_top   /* reset abort-mode banked SP (essential)    */
	srsdb   sp!, #0x17            /* push {pc, cpsr}                            */
	FAULT_TAIL 2                  /* kind = data abort */

fiq_handler:    b fiq_handler
hang_handler:   b hang_handler

/* ---- SVC entry: the syscall trap ---------------------------------------- *
 * User code runs `svc 0` with the EABI syscall number in r7 and args in r0-r6.
 * On entry the CPU is in SVC mode; lr_svc = the instruction AFTER the svc, and
 * SPSR_svc = the user CPSR.
 *
 * We build a full `struct trapframe` and call syscall_trap(&frame). It dispatches
 * (possibly switching processes) and returns the frame to resume — which
 * user_return restores. See proc.c/syscall.c for the process model.
 */
.global svc_entry
svc_entry:
	/* Reset the kernel stack to its fixed top. The previous user_return repointed
	 * sp_svc into a PCB's trapframe (in .bss) and rfe'd, so sp_svc is now stale;
	 * if we built the new frame there we'd corrupt the PCB. One kernel stack,
	 * cooperative switching at syscall boundaries, and SVC entry masks IRQs (no
	 * reentrancy) => always starting from the top is correct. */
	ldr     sp, =_svc_stack_top

	/* Build a full `struct trapframe` on the kernel stack (S8: fork/exec/switch
	 * need the COMPLETE user register set, not just {pc,cpsr}). Layout, low->high:
	 *   sp_usr, lr_usr, r0..r12, pc(=lr_svc), cpsr(=spsr_svc)  = 17 words.
	 * We push high fields first (descending stack), then hand r0 = &frame to the
	 * C dispatcher, which may switch processes by returning a DIFFERENT frame. */
	srsdb   sp!, #0x13            /* push {pc=lr_svc, cpsr=spsr_svc}            */
	stmfd   sp!, {r0-r12}         /* push r0..r12 (r0 lowest)                  */
	sub     sp, sp, #8            /* room for sp_usr, lr_usr (lowest 2 words)  */
	stmia   sp, {r13, r14}^       /* store the USER-banked sp/lr into slots 0,1*/
	nop                           /* (^ form: no writeback; needs a gap insn)  */

	mov     r0, sp                /* r0 = &trapframe (arg 1)                    */
	bic     sp, sp, #4            /* 8-align SP for the C call (AAPCS): the frame
	                               * is 68 bytes from an 8-aligned top, so SP is
	                               * 4 mod 8 here. Frame ptr kept in r0; we don't
	                               * pop (fall through to user_return).           */
	bl      syscall_trap          /* C: dispatch; returns &frame to resume     */
	/* fallthrough: r0 = frame to return to (usually the same one) */

/* ---- user_return(struct trapframe *tf [r0]) — resume a user frame -------- *
 * Loads a full trapframe and returns to user mode. Reached by falling through
 * from svc_entry, or called directly to launch/switch into a process.
 */
.global user_return
user_return:
	mov     sp, r0                /* point sp at the frame to restore          */
	ldmia   sp, {r13, r14}^       /* restore USER-banked sp/lr from slots 0,1  */
	nop
	add     sp, sp, #8
	ldmfd   sp!, {r0-r12}         /* restore r0..r12                           */
	rfeia   sp!                   /* restore pc + cpsr -> back to user         */

/* ---- tiny CP15 / CPSR helpers callable from C ---------------------------- */
.global irq_enable
irq_enable:
	cpsie   i                     /* clear CPSR.I -> IRQs on */
	bx      lr

.global irq_disable
irq_disable:
	cpsid   i
	bx      lr

/* uint32_t irq_save(void) — mask IRQs, return the PRIOR CPSR (for irq_restore).
 * void irq_restore(uint32_t flags) — restore CPSR (re-enabling IRQs only if they
 * were enabled before). These NEST correctly: a nested save/restore pair leaves
 * IRQs masked because the outer save captured them already-masked. This is the
 * critical-section primitive for the (upcoming) preemptive scheduler — the plain
 * irq_enable/irq_disable above are non-composable (irq_enable unconditionally
 * clears I, so an inner region would wrongly re-enable IRQs the outer masked). */
.global irq_save
irq_save:
	mrs     r0, cpsr              /* r0 = current CPSR (has the I bit to restore) */
	cpsid   i                     /* mask IRQs                                    */
	bx      lr

.global irq_restore
irq_restore:
	/* Restore only the interrupt-mask bits from the saved CPSR; leave the current
	 * mode/flags intact (we may be called from a different point than the save).
	 * MSR cpsr_c writes the control byte (mode + I/F/T); the caller only ever
	 * changes the I bit between save and restore, so writing the saved control
	 * byte back is correct and minimal. */
	msr     cpsr_c, r0
	bx      lr

/* uint32_t boot_dtb(void) — the DTB physical address the bootloader passed in r2
 * (0 / garbage if booted without a DTB; kmain validates the FDT magic).       */
.global boot_dtb
boot_dtb:
	ldr     r0, =_boot_r2
	ldr     r0, [r0]
	bx      lr

/* uint32_t boot_machine(void) — the machine type from r1 (~0 for DT boot).    */
.global boot_machine
boot_machine:
	ldr     r0, =_boot_r1
	ldr     r0, [r0]
	bx      lr

.data
.align 2
_boot_r2:  .word 0
_boot_r1:  .word 0
.text
