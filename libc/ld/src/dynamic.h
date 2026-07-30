/*
 * dynamic.h — S1: parse a mapped ELF object's PT_DYNAMIC into a dso_t.
 *
 * This is the "gather the questions" half that L2's pure logic (reloc.h) assumes
 * someone else did: given an object already present in memory (its LOAD segments
 * mapped) plus its load bias `base`, find the .dynamic array and extract the
 * pointers L2 needs — STRTAB/SYMTAB/HASH and the relocation tables.
 *
 * Still dependency-free (no syscalls) so it's host-testable against our real
 * libc.so: map the file, call dl_parse, assert the tables it found. The ARM
 * runtime later maps the object for real, then calls this identical code.
 *
 * A `.dynamic` entry's d_val is EITHER an address (a VA within the object, needs
 * +base to become a pointer) OR a plain scalar (a size/count). The ELF spec
 * fixes which per tag; we apply +base only to the address-typed ones.
 */
#ifndef GV3_LD_DYNAMIC_H
#define GV3_LD_DYNAMIC_H

#include "elf32.h"
#include "reloc.h"

/*
 * Find PT_DYNAMIC among `phnum` program headers at `phdr` and return a pointer
 * to the .dynamic array (as a real pointer: p_vaddr + base). NULL if none.
 * `phdr`/base describe an object already in memory.
 */
static inline const Elf32_Dyn *dl_find_dynamic(const Elf32_Phdr *phdr,
                                               int phnum, Elf32_Addr base)
{
	for (int i = 0; i < phnum; i++) {
		if (phdr[i].p_type == PT_DYNAMIC)
			return (const Elf32_Dyn *)(uintptr_t)(phdr[i].p_vaddr + base);
	}
	return 0;
}

/*
 * Parse the .dynamic array into `d`. `base` is the object's load bias. Returns 1
 * on success (found STRTAB+SYMTAB+HASH — the minimum for symbol resolution), 0
 * if the object has no usable dynamic info. Relocation tables are optional (an
 * object may legitimately have none).
 */
static inline int dl_parse(dso_t *d, const Elf32_Dyn *dyn, Elf32_Addr base)
{
	/* zero everything, then fill from the tags we recognize */
	d->base = base;
	d->symtab = 0; d->strtab = 0; d->hash = 0; d->syment = sizeof(Elf32_Sym);
	d->jmprel = 0; d->pltrelsz = 0; d->rel = 0; d->relsz = 0; d->relent = sizeof(Elf32_Rel);
	d->nneeded = 0;
	for (int i = 0; i < 8; i++) d->needed[i] = 0;

	/* First pass: collect address/size tags. DT_NEEDED holds a STRTAB OFFSET, so
	 * we must resolve strtab first — do NEEDED in a second pass. */
	Elf32_Word needed_off[8]; int nneeded = 0;

	for (const Elf32_Dyn *e = dyn; e->d_tag != DT_NULL; e++) {
		switch (e->d_tag) {
		/* ---- address-typed: d_val is a VA, add base to get a pointer ---- */
		case DT_SYMTAB: d->symtab = (const Elf32_Sym *)(uintptr_t)(e->d_val + base); break;
		case DT_STRTAB: d->strtab = (const char *)(uintptr_t)(e->d_val + base); break;
		case DT_HASH:   d->hash   = (const uint32_t *)(uintptr_t)(e->d_val + base); break;
		case DT_JMPREL: d->jmprel = (const Elf32_Rel *)(uintptr_t)(e->d_val + base); break;
		case DT_REL:    d->rel    = (const Elf32_Rel *)(uintptr_t)(e->d_val + base); break;
		/* ---- scalar-typed: d_val is a size/count, used as-is ---- */
		case DT_SYMENT:   d->syment   = e->d_val; break;
		case DT_PLTRELSZ: d->pltrelsz = e->d_val; break;
		case DT_RELSZ:    d->relsz    = e->d_val; break;
		case DT_RELENT:   d->relent   = e->d_val; break;
		case DT_NEEDED:
			if (nneeded < 8) needed_off[nneeded++] = e->d_val;  /* strtab offset */
			break;
		default: break;   /* ignore tags we don't use (PLTREL, STRSZ, DEBUG, …) */
		}
	}

	if (!d->strtab || !d->symtab || !d->hash)
		return 0;   /* not enough to resolve symbols */

	/* Second pass now that strtab is known: turn NEEDED offsets into names. */
	for (int i = 0; i < nneeded; i++)
		d->needed[d->nneeded++] = d->strtab + needed_off[i];

	return 1;
}

#endif /* GV3_LD_DYNAMIC_H */
