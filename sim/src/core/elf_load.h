/*
 * elf_load.h — Stripped ELF loader (segments only, no symbols/DWARF)
 *
 * Used by sim-core. sim-dbg uses the full elf_sym.c instead.
 */
#ifndef ELF_LOAD_H
#define ELF_LOAD_H

#include <stdint.h>

/* Load ELF program segments into flash/RAM. Returns 0 on success. */
int elf_load_segments(const char *path, uint8_t *flash, uint8_t *ram);

#endif
