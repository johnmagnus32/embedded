/*
 * start.S — first instructions of our custom T113-S3 bootloader (Stage 1).
 *
 * The Allwinner BROM loads this image from the SD 8 KiB offset into SRAM at
 * 0x00020000 and jumps to offset 0 — i.e. straight into the eGON header's first
 * word, which we make an ARM branch that jumps *over* the 96-byte header into
 * _start. See bootloader/README for the full BROM contract.
 *
 * At entry the BROM guarantees very little: a valid CPU with SRAM, MMU/caches
 * effectively off. We must set up our own stack, get into a known CPU state,
 * zero BSS, and call C. No DRAM yet (Stage 1 lives entirely in SRAM).
 */

/* ---- eGON.BT0 boot header (96 bytes, 0x60) ------------------------------- *
 * The mk-egon tool fills in check_sum and length after link; we lay out the
 * exact fields the BROM reads. Offsets must match struct boot_file_head.
 */
.section .text.head, "ax"
.global _egon_head
_egon_head:
	b       _start                  /* 0x00: branch over header to _start   */
	.byte   'e','G','O','N','.','B','T','0'  /* 0x04: magic[8] (not a C str) */
	.word   0x5f0a6c39              /* 0x0C: check_sum — seeded w/ BROM stamp,
	                                 *       finalized by mk-egon.            */
	.word   0                       /* 0x10: length — filled in by mk-egon    */
	.byte   'S','P','L',0x02        /* 0x14: sunxi SPL signature + version    */
	.word   0                       /* 0x18: fel_script_address               */
	.word   0                       /* 0x1C: fel_uEnv_length                  */
	.word   0                       /* 0x20: dt_name_offset                   */
	.word   0                       /* 0x24: dram_size (filled by SPL later)  */
	.word   0                       /* 0x28: boot_media (written by BROM)     */
	/* 0x2C: string_pool[13] = 52 bytes of padding to reach 0x60             */
	.word   0,0,0,0,0,0,0,0,0,0,0,0,0

/* ---- reset / C entry ----------------------------------------------------- */
.global _start
_start:
	/* Enter SVC mode, mask IRQ+FIQ (CPSR: mode=0x13, I=1, F=1).             */
	msr     cpsr_c, #0xd3

	/* Minimal CP15: ensure MMU + caches are OFF (BROM should leave them so,
	 * but be explicit). Clear SCTLR bits M(0) A(1) C(2) I(12) V(13).        */
	mrc     p15, 0, r0, c1, c0, 0
	bic     r0, r0, #(1 << 0)       /* M: MMU off                            */
	bic     r0, r0, #(1 << 1)       /* A: alignment off                      */
	bic     r0, r0, #(1 << 2)       /* C: D-cache off                        */
	bic     r0, r0, #(1 << 12)      /* I: I-cache off                        */
	bic     r0, r0, #(1 << 13)      /* V: normal exception base              */
	mcr     p15, 0, r0, c1, c0, 0
	isb

	/* Set up the stack (top of SRAM region, from the linker script).        */
	ldr     sp, =_stack_top

	/* Zero the .bss so C globals start at 0.                                */
	ldr     r0, =_bss_start
	ldr     r1, =_bss_end
	mov     r2, #0
1:	cmp     r0, r1
	bcs     2f
	str     r2, [r0], #4
	b       1b
2:

	/* Into C. main() should not return; if it does, spin.                   */
	bl      main
hang:
	b       hang

/* ---- boot_kernel(entry, dtb) -------------------------------------------- *
 * Hand off to a 32-bit ARM Linux zImage per Documentation/arm/booting.rst:
 *   r0 = 0
 *   r1 = machine type (~0 / 0xffffffff when booting with a device tree)
 *   r2 = physical address of the DTB
 *   CPU in SVC mode, IRQ+FIQ disabled, MMU off, D-cache off, I-cache off,
 *   enter in ARM state with a direct branch to the kernel's first instruction.
 * C prototype:  void boot_kernel(uint32_t entry, uint32_t dtb);
 *   r0 = entry (kernel load addr), r1 = dtb  (AAPCS: first two args)
 */
.global boot_kernel
boot_kernel:
	mov     r4, r0                  /* r4 = kernel entry (preserve)          */
	mov     r5, r1                  /* r5 = dtb phys addr                    */

	/* Mask IRQ+FIQ, ensure SVC mode. */
	cpsid   if
	mrs     r0, cpsr
	bic     r0, r0, #0x1f
	orr     r0, r0, #0x13           /* SVC mode                              */
	msr     cpsr_c, r0

	/* Set the ARM generic timer frequency (CNTFRQ) = 24 MHz (0x016E3600).
	 * The BROM/our SPL never programs it; the kernel reads CNTFRQ for the
	 * arch timer, and a 0 value causes a div-by-zero ("arch_timer: frequency
	 * not available" -> "Failed to initialize '/timer'") that kills the boot
	 * before userspace. T113 generic-timer clock = the 24 MHz oscillator.
	 * (U-Boot normally does this for us; on the FEL/custom path we must.)   */
	movw    r0, #0x3600
	movt    r0, #0x016E
	mcr     p15, 0, r0, c14, c0, 0  /* CNTFRQ                                 */

	/* Turn off MMU + caches via SCTLR (should already be off from _start,
	 * but the kernel REQUIRES MMU off + D-cache off, so be explicit).       */
	mrc     p15, 0, r0, c1, c0, 0
	bic     r0, r0, #(1 << 0)       /* M:  MMU off                           */
	bic     r0, r0, #(1 << 2)       /* C:  D-cache off                       */
	bic     r0, r0, #(1 << 12)     /* I:  I-cache off                       */
	mcr     p15, 0, r0, c1, c0, 0
	isb

	/* Invalidate I-cache + branch predictor + TLBs so stale entries from
	 * our own execution can't haunt the kernel. */
	mov     r0, #0
	mcr     p15, 0, r0, c7, c5, 0   /* ICIALLU: invalidate I-cache           */
	mcr     p15, 0, r0, c7, c5, 6   /* BPIALL:  invalidate branch predictor  */
	mcr     p15, 0, r0, c8, c7, 0   /* TLBIALL: invalidate unified TLB       */
	dsb
	isb

	/* Load the boot protocol registers and jump. */
	mov     r0, #0                  /* r0 = 0                                */
	mvn     r1, #0                  /* r1 = ~0 (0xffffffff), DT machine      */
	mov     r2, r5                  /* r2 = dtb                              */
	bx      r4                      /* enter the kernel (never returns)      */
