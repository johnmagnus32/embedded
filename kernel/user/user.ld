/*
 * user.ld — link a standalone S8 user program as a static ARM ET_EXEC.
 *
 * Loaded by the kernel's ELF loader at its fixed p_vaddr. We link low (0x10000)
 * so user VAs (< 0x40000000) never collide with the kernel (0x41000000+) or its
 * identity map. ENTRY(_ustart) sets e_entry to our entry point wherever it lands.
 * Segments are kept 4 KB-aligned (-z max-page-size=0x1000 in the Makefile) so the
 * loader maps clean whole pages.
 */

ENTRY(_ustart)

SECTIONS
{
	. = 0x00010000;

	.text   : { *(.text*)   }
	.rodata : { *(.rodata*) }
	.data   : { *(.data*)   }

	.bss    : { *(.bss*) *(COMMON) }

	/DISCARD/ : { *(.comment) *(.ARM.attributes) *(.note*) }
}
