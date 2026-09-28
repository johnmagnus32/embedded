/*
 * reloc.h — the PURE dynamic-linking logic: describe a loaded object, look up a
 * symbol in it, and apply a relocation. No syscalls, no ARM specifics, no
 * globals — so the exact same code is exercised by the x86 host unit tests
 * (ld/test) AND run on the ARM target. This is where linker bugs are cheapest to
 * catch, so it's isolated here on purpose.
 */
#ifndef LD_RELOC_H
#define LD_RELOC_H

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

	Elf32_Addr        relro;     /* PT_GNU_RELRO, runtime address (0 = none)      */
	Elf32_Word        relrosz;   /* ...and its size                               */
} dso_t;

/* SysV ELF hash of a symbol name (the classic algorithm; matches DT_HASH). */
static inline uint32_t elf_hash(const char *name)
{
	uint32_t h = 0, g;
	while (*name) {
		h = (h << 4) + (unsigned char)*name++;
		g = h & 0xf0000000u;
		if (g) h ^= g >> 24;   /* g is uint32_t -> a logical shift (our cc now emits lsr for unsigned) */
		h &= ~g;
	}
	return h;
}

/*
 * Look up `name` in `d`'s dynamic symbol table via the SysV hash. Returns the
 * symbol's absolute address (st_value + base), or 0 if `d` doesn't provide it.
 * Only GLOBAL/WEAK symbols are considered (the export set). Two lookup classes:
 *   LOOKUP_DATA — a definition, or a PROGRAM's undefined FUNC entry with a value:
 *                 that value is the function's canonical address (its PLT entry in
 *                 a non-PIC program), which every object must use as the
 *                 function's address so pointers compare equal;
 *   LOOKUP_PLT  — a definition only: a PLT slot (JUMP_SLOT) or a COPY source must
 *                 reach the real function/variable, not the program's stub.
 */
enum { LOOKUP_DATA, LOOKUP_PLT };
/* Symbol fields are read with natural struct access: our cc now has a 2-byte type (short/uint16_t), so it
 * lays Elf32_Sym out exactly as the ABI does — st_shndx@14, sizeof 16 — and symtab[i] strides correctly.
 * (DT_SYMENT is 16 for every ELF32; d->syment carries it but the layout is the same either way.) */
static inline Elf32_Addr dso_lookup(const dso_t *d, const char *name, int cls)
{
	if (!d->hash || !d->symtab || !d->strtab)
		return 0;
	uint32_t nbucket = d->hash[0];
	/* nchain = d->hash[1]; */
	const uint32_t *bucket = &d->hash[2];
	const uint32_t *chain  = &bucket[nbucket];

	uint32_t h = elf_hash(name);
	for (uint32_t i = bucket[h % nbucket]; i != 0 /*STN_UNDEF*/; i = chain[i]) {
		const Elf32_Sym *sym = &d->symtab[i];
		if (sym->st_shndx == SHN_UNDEF) {      /* imported here: only a canonical address counts, for data refs */
			int type = ELF32_ST_TYPE(sym->st_info);
			if (cls == LOOKUP_PLT || sym->st_value == 0 || type != STT_FUNC)
				continue;
		}
		int bind = ELF32_ST_BIND(sym->st_info);
		if (bind != STB_GLOBAL && bind != STB_WEAK)
			continue;
		const char *sname = d->strtab + sym->st_name;
		/* inline strcmp (no libc dependency) */
		const char *a = sname, *b = name;
		while (*a && *a == *b) { a++; b++; }
		if (*a == *b)
			return sym->st_value + d->base;
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
 * Apply one relocation to a mapped object: read the addend from the slot, resolve
 * any symbol through the global lookup SCOPE (the program first, then the
 * libraries in load order — the same scope for every object, so all of them bind a
 * name to ONE definition), compute the value, and store it. A COPY's source is
 * looked up past `self` (the program holding the copy). Returns 1 on success, 0 if
 * a symbol was unresolved or the type is unsupported (caller should error out).
 * This is the ARM-side glue that DOES touch memory; the arithmetic it defers to
 * reloc_value(). (On a 32-bit target the pointer math is exact.)
 */
static inline int reloc_apply(const dso_t *self, const dso_t *const *scope, int nscope,
                              const Elf32_Rel *r)
{
	Elf32_Addr *where = (Elf32_Addr *)(uintptr_t)(r->r_offset + self->base);
	uint32_t type   = ELF32_R_TYPE(r->r_info);
	uint32_t symidx = ELF32_R_SYM(r->r_info);
	Elf32_Word addend = *where;             /* REL: addend lives in the slot */

	Elf32_Addr S = 0;
	int is_weak = 0;
	if (type != R_ARM_RELATIVE) {
		const char *name = self->strtab + self->symtab[symidx].st_name;
		int cls = (type == R_ARM_JUMP_SLOT || type == R_ARM_COPY) ? LOOKUP_PLT : LOOKUP_DATA;
		for (int i = 0; i < nscope && S == 0; i++)
			if (!(type == R_ARM_COPY && scope[i] == self))
				S = dso_lookup(scope[i], name, cls);
		if (S == 0) {
			int bind = ELF32_ST_BIND(self->symtab[symidx].st_info);   /* bind on its own line (cc shift-compare) */
			if (bind == STB_WEAK) is_weak = 1;
		}
	}

	/* R_ARM_COPY (non-PIC exe importing a variable): the linker put a slot in our .dynbss at r_offset
	 * and every ref points there; here we copy the variable's bytes from its provider definition (S)
	 * into that slot. Length is the import's st_size (recorded by the linker in our .dynsym). Not a
	 * value-store, so it's handled before reloc_value(). */
	if (type == R_ARM_COPY) {
		if (S == 0)
			return 0;   /* unresolved definition */
		uint8_t *dst = (uint8_t *)where, *src = (uint8_t *)(uintptr_t)S;
		uint32_t n = self->symtab[symidx].st_size;
		for (uint32_t i = 0; i < n; i++)
			dst[i] = src[i];
		return 1;
	}

	reloc_value_t rv = reloc_value(type, self->base, S, addend);
	if (!rv.ok) {
		/* A WEAK symbol that resolves nowhere binds to 0 (ELF gABI): write 0 into the
		 * slot and treat it as resolved, rather than failing. This is what makes a normal
		 * cross-linked binary from the conforming toolchain-bootstrap work — crtbegin.o
		 * imports __register_frame_info/__deregister_frame_info as WEAK and NULL-checks
		 * them, so a minimal libc that doesn't provide them is fine. (The -nostartfiles
		 * LIBC=custom binaries carry no such weak imports, so this path is unreachable there.) */
		if (is_weak) {
			if (type == R_ARM_JUMP_SLOT || type == R_ARM_GLOB_DAT || type == R_ARM_ABS32) {
				*where = 0;
				return 1;
			}
		}
		return 0;
	}
	*where = rv.value;
	return 1;
}

#endif /* LD_RELOC_H */
