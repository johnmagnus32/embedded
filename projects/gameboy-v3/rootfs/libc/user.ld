/*
 * user.ld — link a gv3libc program as a static ARM ET_EXEC.
 *
 * Same conventions as kernel/user/user.ld: link low (0x10000) so user VAs
 * (< 0x40000000) never collide with the kernel (0x41000000+) or its identity
 * map, and keep segments 4 KB-aligned so the kernel's ELF loader maps clean
 * whole pages. ENTRY(_start) is gv3libc's crt0.
 */
ENTRY(_start)

SECTIONS
{
	. = 0x00010000;

	.text : {
		*(.text._start)     /* crt0 first — not required, but tidy */
		*(.text*)
	}
	.rodata : { *(.rodata*) }
	.data   : { *(.data*)   }

	. = ALIGN(8);
	.bss    : { *(.bss*) *(COMMON) }

	/DISCARD/ : { *(.comment) *(.ARM.attributes) *(.note*) }
}
