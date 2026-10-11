ENTRY(_start)
SECTIONS {
	. = 0x40000000;
	.text   : { *(.text*) }
	.rodata : { *(.rodata*) }
	/* the constructors crt.s walks (a script that collects them must name their bounds, as the built-in one does) */
	.init_array : { PROVIDE_HIDDEN(__init_array_start = .); KEEP(*(SORT_BY_NAME(.init_array.*))) KEEP(*(.init_array))
	                PROVIDE_HIDDEN(__init_array_end = .); }
	.fini_array : { PROVIDE_HIDDEN(__fini_array_start = .); KEEP(*(SORT_BY_NAME(.fini_array.*))) KEEP(*(.fini_array))
	                PROVIDE_HIDDEN(__fini_array_end = .); }
	.data   : { *(.data*) }
	. = ALIGN(8);
	__bss_start = .;
	.bss    : { *(.bss*) *(COMMON) }
	. = ALIGN(8);
	__bss_end = .;
	/DISCARD/ : { *(.ARM.attributes) *(.comment) *(.note*) }
}
