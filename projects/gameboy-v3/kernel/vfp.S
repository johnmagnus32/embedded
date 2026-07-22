/*
 * vfp.S — save/restore the VFP/NEON register file for context switching.
 *
 * The kernel is built -mgeneral-regs-only (it never uses FP), but USER programs
 * do: the hardfloat musl BusyBox uses VFP pervasively. Under PREEMPTION two
 * processes can be time-sliced while both mid-FP-computation, so the scheduler
 * must save/restore each process's FP state or their float registers corrupt
 * each other. This is the eager (save-on-every-switch) approach — simple and
 * correct; a lazy trap-on-first-use scheme is a later optimization.
 *
 * Cortex-A7 implements VFPv4 + NEON with the D32 register file: 32 double-word
 * registers d0..d31 (= s0..s31 overlaid on d0..d15) plus the FPSCR. State size:
 *   32 * 8 bytes (d0..d31) + 4 bytes (FPSCR) = 260 bytes; we reserve 264 (8-aligned).
 *
 * .fpu neon-vfpv4 tells the ASSEMBLER to accept the vstm/vldm/vmrs/vmsr mnemonics
 * even though the C compiler flag forbids FP codegen — this file is hand-written.
 */
	.syntax unified
	.arch armv7-a
	.fpu neon-vfpv4
	.text

/* void vfp_save(void *dst) — store d0..d31 then FPSCR into the 264-byte buffer
 * at r0. Uses vstmia with writeback in two halves (many assemblers cap a single
 * vstm at 16 doubles). */
	.global vfp_save
	.type vfp_save, %function
vfp_save:
	vstmia  r0!, {d0-d15}         /* d0..d15  -> [dst +0 .. +127]   */
	vstmia  r0!, {d16-d31}        /* d16..d31 -> [dst +128 .. +255] */
	vmrs    r1, fpscr            /* r1 = FPSCR                     */
	str     r1, [r0]             /* FPSCR    -> [dst +256]         */
	bx      lr

/* void vfp_restore(const void *src) — inverse of vfp_save. */
	.global vfp_restore
	.type vfp_restore, %function
vfp_restore:
	vldmia  r0!, {d0-d15}
	vldmia  r0!, {d16-d31}
	ldr     r1, [r0]
	vmsr    fpscr, r1
	bx      lr
