/*
 * sram.ld — link the Stage-1 bootloader to run from T113 SRAM at 0x00020000.
 *
 * The BROM loads our image here (verified: SUNXI_SRAM_ADDRESS=0x20000 for the
 * NCAT2/R528 family) and jumps to offset 0. So the eGON header (.text.head)
 * MUST be the very first thing in the image. 160 KiB of SRAM is available in
 * boot mode; Stage 1 uses a tiny fraction.
 */

ENTRY(_start)

MEMORY
{
	SRAM (rwx) : ORIGIN = 0x00020000, LENGTH = 160K
}

SECTIONS
{
	. = 0x00020000;

	.text : {
		KEEP(*(.text.head))     /* eGON header first — BROM enters here */
		*(.text*)
	} > SRAM

	.rodata : { *(.rodata*) } > SRAM
	.data   : { *(.data*)   } > SRAM

	. = ALIGN(8);
	_bss_start = .;
	.bss : { *(.bss*) *(COMMON) } > SRAM
	. = ALIGN(8);
	_bss_end = .;

	/* Stack grows down from the top of the SRAM region. */
	. = ALIGN(8);
	. = . + 0x2000;             /* 8 KiB stack */
	_stack_top = .;

	/DISCARD/ : { *(.comment) *(.ARM.attributes) *(.note*) }
}
