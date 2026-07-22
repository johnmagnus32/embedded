/*
 * elf.h — load a static ARM ET_EXEC ELF into an address space.
 */
#ifndef GV3K_ELF_H
#define GV3K_ELF_H

#include <stdint.h>

/* Load the ELF at `img` (size `sz`) into address space `l1_pa`: map + copy each
 * PT_LOAD segment (zeroing bss), at 4 KB granularity. On success writes the
 * entry VA to *entry_out, the initial program break (highest page-rounded
 * PT_LOAD end) to *brk_out (may be NULL), and returns 0. Negative on error. */
int elf_load(uint32_t l1_pa, const void *img, uint32_t sz,
             uint32_t *entry_out, uint32_t *brk_out);

#endif /* GV3K_ELF_H */
