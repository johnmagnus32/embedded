/*
 * sram.ld — link the Stage-1 bootloader to run from T113 SRAM at 0x00020000.
 *
 * The BROM loads our image here (verified: SUNXI_SRAM_ADDRESS=0x20000 for the
 * NCAT2/R528 family) and jumps to offset 0. So the eGON header (.text.head)
 * MUST be the very first thing in the image. 160 KiB of SRAM is available in
 * boot mode; Stage 1 uses a tiny fraction.
 */

ENTRY(_start)

/* Load/run address. Defaults to 0x20000 (the SPL slot the BROM uses for a
 * medium boot). For FEL delivery, xfel cannot bulk-write 0x20000 (it collides
 * with the live FEL USB buffers in SRAM A1), so the FEL build overrides this to
 * a higher SRAM address via `-Wl,--defsym=LOAD_ADDR=0x28000`. Nothing in the
 * code hardcodes 0x20000 (the eGON branch is relative; stack/BSS use symbols),
 * so the image runs correctly at whichever address it's linked + loaded to. */
LOAD_ADDR = DEFINED(LOAD_ADDR) ? LOAD_ADDR : 0x00020000;

SECTIONS
{
	. = LOAD_ADDR;

	.text : {
		KEEP(*(.text.head))     /* eGON header first — BROM enters here */
		*(.text*)
	}

	.rodata : { *(.rodata*) }
	.data   : { *(.data*)   }

	. = ALIGN(8);
	_bss_start = .;
	.bss : { *(.bss*) *(COMMON) }
	. = ALIGN(8);
	_bss_end = .;

	/* Stack grows down from the top of the SRAM region. */
	. = ALIGN(8);
	. = . + 0x2000;             /* 8 KiB stack */
	_stack_top = .;

	/DISCARD/ : { *(.comment) *(.ARM.attributes) *(.note*) }
}
