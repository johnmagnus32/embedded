#ifndef ELF_SYM_H
#define ELF_SYM_H

#include <stdint.h>

/* Load ELF segments into flash/ram and parse symbol table.
 * Returns 0 on success, -1 if not an ELF (caller should try raw .bin). */
int elf_load(const char *path, uint8_t *flash, uint8_t *ram);

/* Look up a PC address → function name + offset from start.
 * Returns NULL if no symbol found. */
const char *sym_lookup(uint32_t pc, uint32_t *offset);

#endif
