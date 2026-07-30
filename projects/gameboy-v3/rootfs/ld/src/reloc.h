/*
 * reloc.h — the PURE dynamic-linking logic: describe a loaded object, look up a
 * symbol in it, and apply a relocation. No syscalls, no ARM specifics, no
 * globals — so the exact same code is exercised by the x86 host unit tests
 * (ld/test) AND run on the ARM target. This is where linker bugs are cheapest to
 * catch, so it's isolated here on purpose.
 */
#ifndef GV3_LD_RELOC_H
#define GV3_LD_RELOC_H

#include "elf32.h"

/*
 * A dynamic object we've mapped (the program, or libc.so). `base` is the load
 * bias: the address the object was mapped at MINUS its lowest p_vaddr. For a
 * non-PIE ET_EXEC linked at a fixed address, base == 0 and VAs are absolute; for
 * a PIC .so mapped anywhere, base is where it landed. Every VA below is turned
 * into a real pointer by adding `base`. The table pointers are resolved from
 * .dynamic (see dynamic.c) and already include the base.
 */
typedef struct {
	Elf32_Addr        base;      /* load bias (add to any p_vaddr in this object) */
	const Elf32_Sym  *symtab;    /* DT_SYMTAB (already base-relocated)             */
	const char       *strtab;    /* DT_STRTAB                                      */
	const uint32_t   *hash;      /* DT_HASH (SysV): [nbucket][nchain][buckets][chain] */
	Elf32_Word        syment;    /* DT_SYMENT (sizeof one Elf32_Sym)              */

	/* relocation tables (filled by dl_parse; consumed by the S4 apply loop) */
	const Elf32_Rel  *jmprel;    /* DT_JMPREL: PLT relocations (JUMP_SLOT)         */
	Elf32_Word        pltrelsz;  /* DT_PLTRELSZ: bytes of jmprel                   */
	const Elf32_Rel  *rel;       /* DT_REL: other dynamic relocs (GLOB_DAT/RELATIVE) */
	Elf32_Word        relsz;     /* DT_RELSZ: bytes of rel                         */
	Elf32_Word        relent;    /* DT_RELENT: sizeof one Elf32_Rel (=8)          */

	const char       *needed[8]; /* DT_NEEDED names (strtab ptrs); NULL-terminated */
	int               nneeded;
} dso_t;

/* SysV ELF hash of a symbol name (the classic algorithm; matches DT_HASH). */
static inline uint32_t elf_hash(const char *name)
{
	uint32_t h = 0, g;
	while (*name) {
		h = (h << 4) + (unsigned char)*name++;
		g = h & 0xf0000000u;
		if (g) h ^= g >> 24;
		h &= ~g;
	}
	return h;
}

/*
 * Look up `name` in `d`'s dynamic symbol table via the SysV hash. Returns the
 * DEFINED symbol's absolute address (st_value + base), or 0 if not found or the
 * symbol is undefined (SHN_UNDEF) in this object. Only GLOBAL/WEAK defined
 * symbols are considered (the export set of a .so).
 */
static inline Elf32_Addr dso_lookup(const dso_t *d, const char *name)
{
	if (!d->hash || !d->symtab || !d->strtab)
		return 0;
	uint32_t nbucket = d->hash[0];
	/* nchain = d->hash[1]; */
	const uint32_t *bucket = &d->hash[2];
	const uint32_t *chain  = &bucket[nbucket];

	uint32_t h = elf_hash(name);
	for (uint32_t i = bucket[h % nbucket]; i != 0 /*STN_UNDEF*/; i = chain[i]) {
		const Elf32_Sym *s = &d->symtab[i];
		if (s->st_shndx == SHN_UNDEF)          /* imported here, not a definition */
			continue;
		int bind = ELF32_ST_BIND(s->st_info);
		if (bind != STB_GLOBAL && bind != STB_WEAK)
			continue;
		const char *sname = d->strtab + s->st_name;
		/* inline strcmp (no libc dependency) */
		const char *a = sname, *b = name;
		while (*a && *a == *b) { a++; b++; }
		if (*a == *b)
			return s->st_value + d->base;
	}
	return 0;
}

/* Result of resolving one relocation's VALUE (pure arithmetic — no memory poke,
 * so it's testable with plain integers even on a 64-bit host where a real
 * pointer wouldn't fit in Elf32_Addr). The caller stores `value` at the slot. */
typedef struct {
	Elf32_Addr value;   /* the value to store at the slot          */
	int        ok;      /* 0 = unresolved symbol / unsupported type */
} reloc_value_t;

/*
 * The PURE fix-up formula for one ARM REL relocation — just arithmetic over the
 * inputs, no dereferencing. `base` is the object's load bias, `S` the resolved
 * symbol address (0 if none/unresolved), `addend` the value currently in the
 * slot (ARM uses REL: the addend lives at *where; the caller reads it).
 *
 *   R_ARM_RELATIVE : base + A          (no symbol; S unused)
 *   R_ARM_GLOB_DAT : S + A
 *   R_ARM_JUMP_SLOT: S                 (A ignored for PLT)
 *   R_ARM_ABS32    : S + A
 */
static inline reloc_value_t reloc_value(uint32_t type, Elf32_Addr base,
                                        Elf32_Addr S, Elf32_Word addend)
{
	reloc_value_t out = { 0, 0 };
	switch (type) {
	case R_ARM_RELATIVE:
		out.value = base + addend; out.ok = 1; break;
	case R_ARM_JUMP_SLOT:
		if (S == 0) break;              /* unresolved -> ok stays 0 */
		out.value = S; out.ok = 1; break;
	case R_ARM_GLOB_DAT:
	case R_ARM_ABS32:
		if (S == 0) break;
		out.value = S + addend; out.ok = 1; break;
	default: break;                         /* unsupported type -> ok = 0 */
	}
	return out;
}

/*
 * Apply one relocation to a mapped object: read the addend from the slot,
 * resolve any symbol (imports against `provider`, then a local fallback in
 * `self`), compute the value, and store it. Returns 1 on success, 0 if an
 * import was unresolved or the type is unsupported (caller should error out).
 * This is the ARM-side glue that DOES touch memory; the arithmetic it defers to
 * reloc_value(). (On a 32-bit target the pointer math is exact.)
 */
static inline int reloc_apply(const dso_t *self, const dso_t *provider,
                              const Elf32_Rel *r)
{
	Elf32_Addr *where = (Elf32_Addr *)(uintptr_t)(r->r_offset + self->base);
	uint32_t type   = ELF32_R_TYPE(r->r_info);
	uint32_t symidx = ELF32_R_SYM(r->r_info);
	Elf32_Word addend = *where;             /* REL: addend lives in the slot */

	Elf32_Addr S = 0;
	if (type != R_ARM_RELATIVE) {
		const char *name = self->strtab + self->symtab[symidx].st_name;
		S = dso_lookup(provider, name);
		if (S == 0)
			S = dso_lookup(self, name);   /* local definition fallback */
	}

	reloc_value_t rv = reloc_value(type, self->base, S, addend);
	if (!rv.ok)
		return 0;
	*where = rv.value;
	return 1;
}

#endif /* GV3_LD_RELOC_H */
