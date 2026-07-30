/*
 * dl.ld — link the dynamic linker (ld-gv3.so.1) as a PIC shared object with a
 * real e_entry. ENTRY(_start) so the kernel knows where to jump after mapping it;
 * the .interp request in the user program names this file as the PT_INTERP target.
 *
 * Placed at VMA 0 (standard for a PIC .so); the kernel maps it wherever it finds
 * space and passes the load address as AT_BASE in the auxv.
 */
ENTRY(_start)

SECTIONS
{
	. = SIZEOF_HEADERS;

	.text : { *(.text._start) *(.text*) } :text
	.rodata : { *(.rodata*) }
	.hash : { *(.hash) }
	.dynsym : { *(.dynsym) }
	.dynstr : { *(.dynstr) }
	.rel.dyn : { *(.rel.dyn) *(.rel.plt) }
	.plt : { *(.plt) }

	. = ALIGN(0x1000);
	.dynamic : { *(.dynamic) } :data :dynamic
	.got : { *(.got*) }
	.data : { *(.data*) }
	.bss : { *(.bss*) *(COMMON) }

	/DISCARD/ : { *(.comment) *(.ARM.attributes) *(.note*) }
}

PHDRS
{
	text    PT_LOAD FLAGS(5);   /* R-X */
	data    PT_LOAD FLAGS(6);   /* RW  */
	dynamic PT_DYNAMIC FLAGS(6);
}
