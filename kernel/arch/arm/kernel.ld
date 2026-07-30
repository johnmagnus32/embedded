/*
 * kernel.ld — link the S5 kernel to run from DRAM at 0x41000000.
 *
 * Our bootloader (or U-Boot) loads this image into DRAM and jumps to its first
 * instruction. 0x41000000 is the same slot the bootloader uses for the Linux
 * kernel (DRAM base 0x40000000 + 16 MiB), so it's known-free and DRAM is already
 * initialized by the time we run. Unlike the bootloader (which ran from 160 KiB
 * of SRAM), we have all of DRAM to work with.
 */

ENTRY(_start)

/* Load/link address. Default = 0x41000000 (T113: our bootloader loads the kernel
 * there). Override for other boot paths via -Wl,--defsym=LOAD_ADDR=0x... — e.g.
 * QEMU `-M virt` raw -kernel loads at RAM base + 0x10000 = 0x40010000. */
LOAD_ADDR = DEFINED(LOAD_ADDR) ? LOAD_ADDR : 0x41000000;

SECTIONS
{
	. = LOAD_ADDR;

	.text : {
		KEEP(*(.text.head))     /* zImage boot header: b _start @0x00, magic @0x24 */
		KEEP(*(.text.boot))     /* _vectors + _start                              */
		*(.text*)
	}

	.rodata : { *(.rodata*) }
	.data   : { *(.data*)   }

	/* End of the LOADABLE image (matches the raw .bin size: everything below is
	 * NOBITS .bss / reserved stack space, absent from the file). */
	. = ALIGN(4);
	_image_end = .;

	. = ALIGN(8);
	_bss_start = .;
	.bss : { *(.bss*) *(COMMON) }
	. = ALIGN(8);
	_bss_end = .;

	/* Per-mode stacks. SVC is the main/kernel stack; IRQ is the interrupt
	 * stack the CPU switches to on an IRQ; ABT and UND are the fault-handler
	 * stacks (Prefetch+Data Abort share Abort mode; Undefined has its own). Each
	 * fault handler builds a full trapframe here and calls into C, so these must
	 * be real stacks, not the previous save-a-word-and-spin scheme. 8-byte
	 * aligned per AAPCS (we call C from the handlers). Generous — we have DRAM. */
	. = ALIGN(8);
	. = . + 0x10000;            /* 64 KiB IRQ stack  */
	_irq_stack_top = .;
	. = . + 0x8000;             /* 32 KiB Abort stack (Prefetch + Data Abort) */
	_abt_stack_top = .;
	. = . + 0x8000;             /* 32 KiB Undefined-instruction stack */
	_und_stack_top = .;
	. = . + 0x40000;            /* 256 KiB SVC stack */
	_svc_stack_top = .;

	/* End of everything the kernel image occupies. The physical page
	 * allocator manages DRAM from here (rounded up) to the top of RAM. */
	. = ALIGN(4096);
	_kernel_end = .;

	/DISCARD/ : { *(.comment) *(.ARM.attributes) *(.note*) }
}
/* The zImage header field values live in start.S (.text.head) as literals, NOT
 * as linker symbols referenced from asm: this kernel links as a PIE, so a
 * `.word <absolute-symbol>` would emit an R_ARM_RELATIVE dynamic relocation that
 * nothing processes here and needlessly grows the image. See the header comment
 * in arch/arm/start.S. Only _image_start/_image_end (defined above) are needed,
 * and start.S uses them as an assemble-relative difference (folded, no reloc). */
