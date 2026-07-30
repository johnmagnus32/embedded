/*
 * elf.h — load an ARM ELF (ET_EXEC or ET_DYN) into an address space.
 */
#ifndef GV3K_ELF_H
#define GV3K_ELF_H

#include <stdint.h>

/* Result of loading one ELF image. For a dynamic executable the caller needs
 * the phdr location + interpreter to build the auxv and to load ld.so. */
struct elf_info {
	uint32_t entry;      /* e_entry + bias (first instruction to run)          */
	uint32_t brk_end;    /* highest page-rounded PT_LOAD end (initial break)    */
	uint32_t phdr_va;    /* in-memory VA of the program headers (AT_PHDR)       */
	uint32_t phent;      /* e_phentsize (AT_PHENT), 32 for ELF32               */
	uint32_t phnum;      /* e_phnum (AT_PHNUM)                                  */
	int      is_dyn;     /* 1 if ET_DYN (loaded at `bias`), 0 if ET_EXEC        */
	int      has_interp; /* 1 if a PT_INTERP program header was present         */
	char     interp[128];/* the interpreter path (valid iff has_interp)         */
};

/* Load the ELF at `img` (size `sz`) into address space `l1_pa`, applying load
 * `bias` to every p_vaddr (0 for ET_EXEC; a chosen base for ET_DYN). Maps + copies
 * each PT_LOAD (zeroing bss), fills *out, returns 0 (negative on error).
 *
 * The kernel applies NO relocations — for a dynamic object ld.so does that.
 * A non-zero bias on an ET_EXEC is rejected (ET_EXEC must load at its p_vaddr). */
int elf_load(uint32_t l1_pa, const void *img, uint32_t sz, uint32_t bias,
             struct elf_info *out);

#endif /* GV3K_ELF_H */
