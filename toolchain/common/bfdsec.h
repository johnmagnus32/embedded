/* bfdsec.h — WHICH SECTIONS BFD SHOWS. An ELF file's section headers are not the list a binutils tool
 * prints: BFD consumes the symbol tables and their string tables as symbol data, and a relocation section
 * as a property of the section it relocates, so none of those appear as sections of their own. `objdump -h`
 * and `size -A` both list exactly what this predicate accepts. */
#ifndef OS_BFDSEC_H
#define OS_BFDSEC_H
#include "elfread.h"
/* A REL/RELA section BFD shows as an ordinary section rather than consuming as relocations. */
int rel_is_normal_section(const ElfFile *f, int i);
/* Does BFD present section `i` as a section at all? */
int bfd_has_section(const ElfFile *f, int i);
#endif
