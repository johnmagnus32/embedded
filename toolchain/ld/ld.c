/*
 * ld.c — the linker FRONT-END (architecture- and format-generic linking algorithm) + driver.
 *
 * Produces a STATIC ELF executable from one or more relocatable objects, the inverse of the assembler:
 * `as` emitted section-relative bytes + relocations; `ld` assigns real virtual addresses and patches
 * them in. Four phases: load objects (elf.c) -> lay out allocatable sections at addresses -> resolve
 * the global symbol table -> apply relocations (via the md backend). The write-out is elf.c's job.
 *
 * The generic linking logic lives here; the ELF on-disk details are in elf.c and the per-relocation
 * encoding is in arm.c, so this file changes for neither a new object format nor a new architecture.
 * Milestone 1: one PT_LOAD, fixed base, PROGBITS/NOBITS only (no archives / dynamic linking yet).
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "ld.h"

Obj objs[MAXOBJ]; int nobj;
u32 load_base = 0x00010000u;               /* image base; -Ttext <addr> overrides (e.g. bare-metal 0x40000000) */
int pie = 0;                               /* -pie: ET_DYN, base 0, absolute refs become load-bias fixups */
u32 dynrel[MAXDYNREL]; int ndynrel;        /* PIE/shared: vaddrs needing an R_ARM_RELATIVE (filled by relocate()) */
int shared = 0;                            /* -shared: emit a .so — exported .dynsym/.hash, no entry point */
const char *soname = NULL;                 /* -soname NAME -> DT_SONAME (default: the output basename) */
Export exports[MAXEXPORT]; int nexport;    /* -shared: the exported symbols, collected before layout */
ShLib shlibs[MAXSHLIB]; int nshlib;        /* -l: shared libraries we link against (providers) */
ShExport shexports[MAXSHEXPORT]; int nshexport;  /* the symbols those providers export */
Import imports[MAXIMPORT]; int nimport;    /* undefined refs resolved to a provider -> runtime imports */
int nplt;                                  /* PLT/GOT/rel.plt entries (distinct CALLED imports) */
CopyRel copyrel[MAXCOPYREL]; int ncopyrel; /* R_ARM_COPY entries (one per DATA import), emitted in .rel.dyn */
GotEnt gotents[MAXGOT]; int ngotent;       /* PIC GOT slots (one per GOT_PREL symbol) */
GlobDat globdat[MAXGLOBDAT]; int nglobdat; /* R_ARM_GLOB_DAT entries (one per IMPORTED GOT slot) */
static const char *entry_sym = "_start";   /* entry point symbol; -e/--entry overrides */

/* SysV .hash bucket count: the largest prime (from bfd's table) not exceeding the symbol count, so
 * `h % nbucket` spreads well. Any positive value is correct; a prime just avoids clustering. */
u32 pick_nbucket(u32 nsyms) {
	static const u32 primes[] = { 1,3,17,37,67,97,131,197,263,521,1031,2053,4099,8209,16411,0 };
	u32 best = 1;
	for (int i = 0; primes[i]; i++) { if (primes[i] <= nsyms) best = primes[i]; else break; }
	return best;
}
static u32 dynstr_bytes(void);      /* defined below; used by layout() to size .dynstr */
static int count_dyn_entries(void); /* defined below; used by layout() to size .dynamic */

/* die() is tool-specific (its own "ld:" prefix); rd32/wr32/alignup/Strtab are shared (common/elfutil). */
void die(const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	fputs("ld: ", stderr); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap); exit(1);
}

/* ---- global symbol table ------------------------------------------------------------------------- */
typedef struct { const char *name; u32 vaddr; int defined; } GSym;
#define MAXGSYM 4096
static GSym gsyms[MAXGSYM]; static int ngsym;
static GSym *gsym_find(const char *name) { for (int i = 0; i < ngsym; i++) if (!strcmp(gsyms[i].name, name)) return &gsyms[i]; return NULL; }
static void gsym_define(const char *name, u32 vaddr) {
	GSym *g = gsym_find(name);
	if (g) { if (g->defined) die("duplicate definition of '%s'", name); g->vaddr = vaddr; g->defined = 1; return; }
	if (ngsym >= MAXGSYM) die("too many global symbols");
	gsyms[ngsym++] = (GSym){ name, vaddr, 1 };
}

/* ---- archive member selection -------------------------------------------------------------------- */
/* Does object o define global symbol `name`? */
static int defines_global(Obj *o, const char *name) {
	for (int k = 0; k < o->nsym; k++) { Elf32_Sym *s = &o->sym[k];
		if (ELF32_ST_BIND(s->st_info) == STB_GLOBAL && s->st_shndx != SHN_UNDEF && s->st_name
		    && !strcmp(o->strtab + s->st_name, name)) return 1; }
	return 0;
}
/* Is `name` currently NEEDED: referenced undefined by an active object AND not yet defined by one? */
static int needed_globally(const char *name) {
	for (int i = 0; i < nobj; i++) if (objs[i].active && defines_global(&objs[i], name)) return 0;
	for (int i = 0; i < nobj; i++) if (objs[i].active) for (int k = 0; k < objs[i].nsym; k++) {
		Elf32_Sym *s = &objs[i].sym[k];
		if (ELF32_ST_BIND(s->st_info) == STB_GLOBAL && s->st_shndx == SHN_UNDEF && s->st_name
		    && !strcmp(objs[i].strtab + s->st_name, name)) return 1; }
	return 0;
}
/* Pull archive members on demand: activate any lazy member that defines a needed symbol, repeating to a
 * fixpoint (a pulled member's own undefined refs may pull further members). This is the classic
 * static-archive rule; we resolve globally rather than by command-line position (like --start-group). */
static void pull_archive_members(void) {
	int changed = 1;
	while (changed) { changed = 0;
		for (int i = 0; i < nobj; i++) { if (objs[i].active) continue;
			for (int k = 0; k < objs[i].nsym; k++) { Elf32_Sym *s = &objs[i].sym[k];
				if (ELF32_ST_BIND(s->st_info) != STB_GLOBAL || s->st_shndx == SHN_UNDEF || !s->st_name) continue;
				if (needed_globally(objs[i].strtab + s->st_name)) { objs[i].active = 1; changed = 1; break; }
			}
		}
	}
}

/* Resolve one object-local symbol index to a final virtual address. */
static u32 resolve(Obj *o, int symidx) {
	Elf32_Sym *s = &o->sym[symidx];
	if (s->st_shndx == SHN_UNDEF) {                     /* external — must be defined elsewhere */
		GSym *g = gsym_find(o->strtab + s->st_name);
		if (!g || !g->defined) die("undefined symbol '%s' (referenced in %s)", o->strtab + s->st_name, o->path);
		return g->vaddr;
	}
	if (s->st_shndx == SHN_ABS) return s->st_value;     /* absolute value, not relocated */
	return o->sec_vaddr[s->st_shndx] + s->st_value;     /* defined here (incl. STT_SECTION: value 0) */
}

/* ---- phases -------------------------------------------------------------------------------------- */
/* Assign every allocatable input section a virtual address, grouped into two page-aligned segments so
 * the kernel can map them with different permissions (W^X). Both segments keep vaddr == LOAD_BASE +
 * file-offset (the RW segment is bumped to a page boundary in file AND memory together, preserving that
 * identity), so the writer places each PROGBITS section at file offset vaddr - LOAD_BASE.
 *   seg 0 (R-X): headers + read-only sections   — SHF_ALLOC && !SHF_WRITE   (.text, .rodata)
 *   seg 1 (R-W): writable data then .bss         — SHF_ALLOC &&  SHF_WRITE   (.data [PROGBITS], .bss [NOBITS])
 * Three placement passes so sections of like kind are contiguous regardless of input order. */
enum { RO = 0, RW = 1 };            /* writability axis: SHF_WRITE clear / set   */
enum { PROGBITS = 0, NOBITS = 1 };  /* storage axis: file-backed / zero-filled   */
static void place(int want_write, int nobits, u32 *cur) {
	for (int i = 0; i < nobj; i++) { if (!objs[i].active) continue; for (int j = 0; j < objs[i].nsh; j++) {
		Elf32_Shdr *s = &objs[i].sh[j];
		if (!(s->sh_flags & SHF_ALLOC) || !s->sh_size) continue;
		if (!!(s->sh_flags & SHF_WRITE) != want_write) continue;
		if ((s->sh_type == SHT_NOBITS) != nobits) continue;
		*cur = alignup(*cur, s->sh_addralign); objs[i].sec_vaddr[j] = *cur; *cur += s->sh_size;
	} }
}
/* PIE: how many R_ARM_RELATIVE entries .rel.dyn will hold — one per absolute reference to a relocatable
 * address. Counts exactly what relocate() will later collect (same predicate, same iteration order), so
 * the space reserved here matches the entries filled there. */
static int count_pie_relocs(void) {
	int n = 0;
	for (int i = 0; i < nobj; i++) { if (!objs[i].active) continue; for (int j = 0; j < objs[i].nsh; j++) {
		Elf32_Shdr *rs = &objs[i].sh[j];
		if (rs->sh_type != SHT_REL || !(objs[i].sh[rs->sh_info].sh_flags & SHF_ALLOC)) continue;
		Elf32_Rel *rel = (Elf32_Rel *)(objs[i].data + rs->sh_offset);
		for (int r = 0, m = rs->sh_size / sizeof(Elf32_Rel); r < m; r++) {
			Elf32_Sym *sym = &objs[i].sym[ELF32_R_SYM(rel[r].r_info)];
			/* RELATIVE only for a DEFINED relocatable address; UNDEF (imports) go via PLT/COPY, ABS is fixed. */
			if (md_needs_dynamic_reloc(ELF32_R_TYPE(rel[r].r_info))
			    && sym->st_shndx != SHN_UNDEF && sym->st_shndx != SHN_ABS) n++;
		}
	} }
	return n;
}
static void layout(Layout *L) {
	int need_dynamic = pie || shared || nimport || ngotent;              /* anything carrying a .dynamic */
	int need_dynsym  = shared || nimport;                                /* a .dynsym (exports and/or imports) */
	int has_interp   = nimport && !shared;                               /* a program names its loader; a .so must NOT */
	/* Over-reserve phdr slots (unused ones become harmless padding before .text): 2 PT_LOAD + optional
	 * PT_DYNAMIC + optional PT_INTERP (a consumer program only). */
	int nphdr = 2 + (need_dynamic ? 1 : 0) + (has_interp ? 1 : 0);
	u32 hdrsz = sizeof(Elf32_Ehdr) + nphdr * sizeof(Elf32_Phdr);
	u32 cur = load_base + hdrsz;
	place(RO, PROGBITS, &cur);               /* seg 0: read-only PROGBITS (.text, .rodata)          */
	L->text_size = cur - (load_base + hdrsz);
	if (has_interp) {                        /* seg 0 tail: PT_INTERP string naming the runtime loader (program only) */
		L->interp_vaddr = cur; L->interp_off = cur - load_base;
		L->interp_sz = (u32)sizeof(INTERP_PATH); cur += L->interp_sz;
	}
	if (nplt) {                              /* seg 0 tail: the PLT stubs (executable) */
		cur = alignup(cur, 4); L->plt_vaddr = cur; L->plt_off = cur - load_base;
		L->plt_sz = (u32)nplt * PLTENT; cur += L->plt_sz;
	}
	if (need_dynsym) {                       /* seg 0 tail: dynamic symbol tables .hash / .dynsym / .dynstr */
		u32 nchain = 1u + (u32)nimport + (u32)nexport, nbucket = pick_nbucket(nchain);  /* idx 0 = null sym */
		cur = alignup(cur, 4); L->hash_vaddr = cur; L->hash_off = cur - load_base;
		L->hash_sz = (2 + nbucket + nchain) * 4; cur += L->hash_sz;      /* [nbucket, nchain, bucket[], chain[]] */
		cur = alignup(cur, 4); L->dynsym_vaddr = cur; L->dynsym_off = cur - load_base;
		L->dynsym_sz = nchain * sizeof(Elf32_Sym); cur += L->dynsym_sz;
		L->dynstr_vaddr = cur; L->dynstr_off = cur - load_base;
		L->dynstr_sz = dynstr_bytes(); cur += L->dynstr_sz;
	}
	if (nplt) {                              /* seg 0 tail: the PLT relocation table (JUMP_SLOTs -> DT_JMPREL) */
		cur = alignup(cur, 4); L->relplt_vaddr = cur; L->relplt_off = cur - load_base;
		L->relplt_sz = (u32)nplt * sizeof(Elf32_Rel); cur += L->relplt_sz;
	}
	int ndata = 0;                           /* DATA imports -> one copy slot + one R_ARM_COPY each */
	for (int i = 0; i < nimport; i++) if (imports[i].is_data) ndata++;
	if (pie || shared || ndata || ngotent) { /* seg 0 tail: .rel.dyn — RELATIVE (ABS32 + GOT-local) + GLOB_DAT (GOT-import) + COPY -> DT_REL */
		int nrelative = (pie || shared) ? count_pie_relocs() : 0;   /* a plain consumer emits no ABS32 RELATIVEs */
		cur = alignup(cur, 4); L->reldyn_vaddr = cur; L->reldyn_off = cur - load_base;
		L->reldyn_sz = (u32)(nrelative + ngotent + ndata) * sizeof(Elf32_Rel); cur += L->reldyn_sz;
	}
	if (need_dynamic) {                      /* seg 0 tail: the .dynamic array */
		cur = alignup(cur, 4); L->dynamic_vaddr = cur; L->dynamic_off = cur - load_base;
		L->dynamic_count = (u32)count_dyn_entries();
		L->dynamic_sz = L->dynamic_count * sizeof(Elf32_Dyn); cur += L->dynamic_sz;
	}
	L->rx_filesz = cur - load_base;
	cur = load_base + alignup(cur - load_base, PAGE);   /* page-align the R-W segment (file + mem)   */
	L->rw_vaddr = cur; L->rw_off = cur - load_base;
	if (nplt) {                              /* seg 1 head: the PLT's GOT — WRITABLE (loader stores here) */
		L->gotplt_vaddr = cur; L->gotplt_off = cur - load_base;
		L->gotplt_sz = (u32)nplt * 4; cur += L->gotplt_sz;
	}
	if (ngotent) {                           /* seg 1: the PIC GOT — writable slots (RELATIVE local / GLOB_DAT import) */
		L->got_vaddr = cur; L->got_off = cur - load_base;
		for (int i = 0; i < ngotent; i++) {
			gotents[i].vaddr = cur; cur += 4;
			if (gotents[i].is_import) {
				if (nglobdat >= MAXGLOBDAT) die("too many GLOB_DAT relocations");
				globdat[nglobdat++] = (GlobDat){ gotents[i].vaddr, (u32)gotents[i].dynsym_index };
			} else {
				if (ndynrel >= MAXDYNREL) die("too many dynamic relocations");
				dynrel[ndynrel++] = gotents[i].vaddr;   /* RELATIVE: slot holds the link addr, loader adds bias */
			}
		}
		L->got_sz = cur - L->got_vaddr;
	}
	place(RW, PROGBITS, &cur);               /* seg 1: writable PROGBITS (.got.plt, .got, then .data) — on disk + memory */
	L->rw_filesz = cur - L->rw_vaddr;
	place(RW, NOBITS, &cur);                 /* seg 1 tail: .bss (NOBITS) — memory only, no file bytes    */
	if (ndata) {                             /* .dynbss: one NOBITS slot per DATA import + its R_ARM_COPY reloc */
		cur = alignup(cur, 4); L->dynbss_vaddr = cur;
		for (int i = 0; i < nimport; i++) if (imports[i].is_data) {
			cur = alignup(cur, 4); imports[i].copy_vaddr = cur; cur += imports[i].copy_size;
			if (ncopyrel >= MAXCOPYREL) die("too many copy relocations");
			copyrel[ncopyrel++] = (CopyRel){ imports[i].copy_vaddr, (u32)imports[i].dynsym_index };
		}
		L->dynbss_sz = cur - L->dynbss_vaddr;
	}
	L->rw_memsz = cur - L->rw_vaddr;
}

/* Record every defined global symbol's final address. */
static void build_globals(void) {
	for (int i = 0; i < nobj; i++) { if (!objs[i].active) continue; for (int k = 0; k < objs[i].nsym; k++) {
		Elf32_Sym *s = &objs[i].sym[k];
		if (ELF32_ST_BIND(s->st_info) == STB_GLOBAL && s->st_shndx != SHN_UNDEF && s->st_name)
			gsym_define(objs[i].strtab + s->st_name, objs[i].sec_vaddr[s->st_shndx] + s->st_value);
	} }
}

/* -shared: collect the global/weak DEFINED (section-relative) symbols to export into .dynsym/.hash.
 * Run before layout — the names size the tables; the final vaddr is read from the obj at write time. */
static void build_exports(void) {
	for (int i = 0; i < nobj; i++) { if (!objs[i].active) continue; for (int k = 0; k < objs[i].nsym; k++) {
		Elf32_Sym *s = &objs[i].sym[k]; int b = ELF32_ST_BIND(s->st_info);
		if ((b == STB_GLOBAL || b == STB_WEAK) && s->st_shndx != SHN_UNDEF && s->st_shndx != SHN_ABS && s->st_name) {
			if (nexport >= MAXEXPORT) die("too many exported symbols");
			exports[nexport++] = (Export){ objs[i].strtab + s->st_name, &objs[i], k };
		}
	} }
}

/* Which provider (-l library) exports `name`, or -1 if none. */
static int shexport_lib(const char *name) {
	for (int i = 0; i < nshexport; i++) if (!strcmp(shexports[i].name, name)) return shexports[i].lib;
	return -1;
}
/* The size a provider records for exported `name` (for a data import's copy slot); 0 if unknown. */
static u32 shexport_size(const char *name) {
	for (int i = 0; i < nshexport; i++) if (!strcmp(shexports[i].name, name)) return shexports[i].size;
	return 0;
}
/* Is `name` defined by one of our own (active) objects? Then it's a local definition, not an import. */
static int defined_by_active(const char *name) {
	for (int i = 0; i < nobj; i++) if (objs[i].active && defines_global(&objs[i], name)) return 1;
	return 0;
}
static int find_import(const char *name) {
	for (int i = 0; i < nimport; i++) if (!strcmp(imports[i].name, name)) return i;
	return -1;
}
/* Scan active objects' relocations for undefined refs that a provider exports -> record them as IMPORTS.
 * A CALL-type reference additionally gets a PLT slot (a stub + GOT word + JUMP_SLOT reloc). Run before
 * layout — the import count sizes .dynsym/.plt/.got.plt/.rel.plt. dynsym_index = the slot after the
 * null symbol and any earlier imports (exports, if this is also -shared, follow the imports). */
static void build_imports(void) {
	for (int i = 0; i < nobj; i++) { if (!objs[i].active) continue; for (int j = 0; j < objs[i].nsh; j++) {
		Elf32_Shdr *rs = &objs[i].sh[j];
		if (rs->sh_type != SHT_REL || !(objs[i].sh[rs->sh_info].sh_flags & SHF_ALLOC)) continue;
		Elf32_Rel *rel = (Elf32_Rel *)(objs[i].data + rs->sh_offset);
		for (int r = 0, m = rs->sh_size / sizeof(Elf32_Rel); r < m; r++) {
			if (md_is_got_reloc(ELF32_R_TYPE(rel[r].r_info))) continue;   /* GOT imports handled by build_got (GLOB_DAT) */
			Elf32_Sym *s = &objs[i].sym[ELF32_R_SYM(rel[r].r_info)];
			if (s->st_shndx != SHN_UNDEF || !s->st_name) continue;
			const char *nm = objs[i].strtab + s->st_name;
			if (!strcmp(nm, "_DYNAMIC")) continue;           /* linker-defined (set after layout); resolve() handles it */
			if (defined_by_active(nm)) continue;             /* resolves locally, not an import */
			int lib = shexport_lib(nm);
			/* A .so may reference symbols with no known provider (e.g. libc's __libc_start_main -> main):
			 * they stay UNDEF and the runtime loader resolves them (against the program / other libs). A
			 * plain exe has no such luxury — an undefined non-import is an error (resolve() reports it). */
			if (lib < 0 && !shared) continue;
			int imp = find_import(nm);
			if (imp < 0) {
				if (nimport >= MAXIMPORT) die("too many imports");
				imp = nimport; imports[nimport++] = (Import){ .name = nm, .lib = lib, .plt_index = -1, .dynsym_index = 1 + imp };
				if (lib >= 0) shlibs[lib].used = 1;          /* provider known -> emit a DT_NEEDED for it */
			}
			if (md_is_call_reloc(ELF32_R_TYPE(rel[r].r_info))) {     /* CALL -> route through a PLT stub */
				if (imports[imp].plt_index < 0) imports[imp].plt_index = nplt++;
			} else if (!imports[imp].is_data) {                      /* ABS32 -> DATA import: needs a copy + R_ARM_COPY */
				imports[imp].is_data = 1;
				imports[imp].copy_size = shexport_size(nm);
				if (!imports[imp].copy_size)
					die("data import '%s': provider records no size (st_size=0), cannot size its copy relocation", nm);
			}
		}
	} }
}

/* PIC: find (or create) the GOT slot for the symbol objs-ref (obj, symidx). A LOCAL definition is keyed
 * by (obj, symidx) — two same-named statics are distinct; a GLOBAL/IMPORT by name (unique program-wide).
 * An IMPORT (undefined + a provider exports it) also gets a .dynsym entry so its slot's GLOB_DAT can name
 * it. Returns the GOT-entry index. Idempotent, so relocate() can call it again to read the slot vaddr. */
static int got_find_or_add(Obj *obj, int symidx) {
	Elf32_Sym *s = &obj->sym[symidx];
	const char *nm = obj->strtab + s->st_name;
	int local = (s->st_shndx != SHN_UNDEF) && (ELF32_ST_BIND(s->st_info) == STB_LOCAL);
	for (int i = 0; i < ngotent; i++) {
		if (local) { if (gotents[i].def_symidx >= 0 && gotents[i].def_obj == obj && gotents[i].def_symidx == symidx) return i; }
		else       { if (gotents[i].def_symidx < 0 && !strcmp(gotents[i].name, nm)) return i; }
	}
	if (ngotent >= MAXGOT) die("too many GOT entries");
	GotEnt *g = &gotents[ngotent];
	*g = (GotEnt){ .name = nm, .def_symidx = -1 };
	if (local) {                                             /* a file-local def (static / string literal) */
		g->def_obj = obj; g->def_symidx = symidx;
	} else if (defined_by_active(nm)) {                      /* a global defined by some active object     */
		/* symval resolved via the global table after build_globals(); RELATIVE slot. */
	} else {                                                 /* an import -> GLOB_DAT slot (runtime-resolved) */
		int lib = shexport_lib(nm);
		if (lib < 0 && !shared)                              /* a .so may import with no known provider; an exe may not */
			die("undefined symbol '%s' via GOT (not defined and no provider exports it)", nm);
		int imp = find_import(nm);
		if (imp < 0) {
			if (nimport >= MAXIMPORT) die("too many imports");
			imp = nimport; imports[nimport++] = (Import){ .name = nm, .lib = lib, .plt_index = -1, .dynsym_index = 1 + imp };
			if (lib >= 0) shlibs[lib].used = 1;
		}
		g->is_import = 1; g->dynsym_index = imports[imp].dynsym_index;
	}
	return ngotent++;
}
/* Collect a GOT slot for every R_ARM_GOT_PREL reference (run before layout — the count sizes .got). */
static void build_got(void) {
	for (int i = 0; i < nobj; i++) { if (!objs[i].active) continue; for (int j = 0; j < objs[i].nsh; j++) {
		Elf32_Shdr *rs = &objs[i].sh[j];
		if (rs->sh_type != SHT_REL || !(objs[i].sh[rs->sh_info].sh_flags & SHF_ALLOC)) continue;
		Elf32_Rel *rel = (Elf32_Rel *)(objs[i].data + rs->sh_offset);
		for (int r = 0, m = rs->sh_size / sizeof(Elf32_Rel); r < m; r++)
			if (md_is_got_reloc(ELF32_R_TYPE(rel[r].r_info)))
				got_find_or_add(&objs[i], ELF32_R_SYM(rel[r].r_info));
	} }
}
/* After build_globals(): fill each LOCAL/GLOBAL GOT slot's initial value (its link-time address). The
 * writer stores this in the .got slot; the loader's R_ARM_RELATIVE then adds the load bias. Imports keep
 * symval 0 (their slot is filled by GLOB_DAT at runtime). */
static void finalize_got(void) {
	for (int i = 0; i < ngotent; i++) {
		GotEnt *g = &gotents[i];
		if (g->is_import) continue;
		if (g->def_symidx >= 0) {                            /* file-local def: address from its own object */
			Elf32_Sym *s = &g->def_obj->sym[g->def_symidx];
			g->symval = g->def_obj->sec_vaddr[s->st_shndx] + s->st_value;
		} else {                                             /* global def: address from the global table  */
			GSym *gs = gsym_find(g->name);
			if (!gs || !gs->defined) die("GOT symbol '%s' undefined at finalize", g->name);
			g->symval = gs->vaddr;
		}
	}
}
/* Number of Elf32_Dyn entries the output carries (kept in lockstep with write_dynamic's emission). */
static int count_dyn_entries(void) {
	int n = 1;                                           /* DT_NULL terminator */
	for (int i = 0; i < nshlib; i++) if (shlibs[i].used) n++;   /* DT_NEEDED per used provider */
	if (shared) n++;                                     /* DT_SONAME */
	if (shared || nimport) n += 5;                       /* DT_HASH/STRTAB/SYMTAB/STRSZ/SYMENT */
	if (nplt) n += 4;                                    /* DT_PLTGOT/PLTRELSZ/PLTREL/JMPREL */
	int ndata = 0; for (int i = 0; i < nimport; i++) if (imports[i].is_data) ndata++;
	if (pie || shared || ndata || ngotent) n += 4;       /* DT_REL/RELSZ/RELENT/RELCOUNT (RELATIVE/GLOB_DAT/COPY) */
	return n;
}
/* .dynstr size: '\0' + import names + export names + used-provider sonames + our own soname (if -shared). */
static u32 dynstr_bytes(void) {
	u32 n = 1;
	for (int i = 0; i < nimport; i++) n += (u32)strlen(imports[i].name) + 1;
	for (int i = 0; i < nexport; i++) n += (u32)strlen(exports[i].name) + 1;
	for (int i = 0; i < nshlib; i++) if (shlibs[i].used) n += (u32)strlen(shlibs[i].soname) + 1;
	if (shared) n += (u32)strlen(soname) + 1;
	return n;
}

/* For each REL section, patch its target section's bytes now that addresses are known. */
static void relocate(const Layout *L) {
	for (int i = 0; i < nobj; i++) { if (!objs[i].active) continue; for (int j = 0; j < objs[i].nsh; j++) {
		Elf32_Shdr *rs = &objs[i].sh[j];
		if (rs->sh_type != SHT_REL) continue;
		Elf32_Shdr *ts = &objs[i].sh[rs->sh_info];              /* the section being patched */
		if (!(ts->sh_flags & SHF_ALLOC)) continue;
		Elf32_Rel *rel = (Elf32_Rel *)(objs[i].data + rs->sh_offset);
		int n = rs->sh_size / sizeof(Elf32_Rel);
		for (int r = 0; r < n; r++) {
			u32 type = ELF32_R_TYPE(rel[r].r_info);
			u32 sidx = ELF32_R_SYM(rel[r].r_info);
			Elf32_Sym *sym = &objs[i].sym[sidx];
			u32 S;
			int imp = -1;
			if (md_is_got_reloc(type)) {                                /* PIC: resolve to the symbol's GOT slot */
				S = gotents[got_find_or_add(&objs[i], sidx)].vaddr;
			} else if ((imp = (sym->st_shndx == SHN_UNDEF && sym->st_name)   /* an import from a shared library? */
			          ? find_import(objs[i].strtab + sym->st_name) : -1) >= 0) {
				if (md_is_call_reloc(type))                         /* call -> its PLT stub (JUMP_SLOT fills the GOT) */
					S = L->plt_vaddr + (u32)PLTENT * imports[imp].plt_index;
				else                                                /* data -> the exe's own copy in .dynbss (R_ARM_COPY) */
					S = imports[imp].copy_vaddr;
			} else {
				S = resolve(&objs[i], sidx);                        /* target symbol address (or local) */
			}
			u32 P = objs[i].sec_vaddr[rs->sh_info] + rel[r].r_offset;    /* address being patched   */
			u8 *loc = objs[i].data + ts->sh_offset + rel[r].r_offset;    /* bytes to patch          */
			md_apply_reloc(&objs[i], type, loc, S, P);
			/* PIE/shared: the static patch above wrote the LINK-TIME value (base 0). Record an
			 * R_ARM_RELATIVE so the loader adds the load bias to it. Absolute symbols carry no address. */
			if ((pie || shared) && md_needs_dynamic_reloc(type)
			    && sym->st_shndx != SHN_UNDEF && sym->st_shndx != SHN_ABS) {
				if (ndynrel >= MAXDYNREL) die("too many dynamic relocations");
				dynrel[ndynrel++] = P;
			}
		}
	} }
}

/* An input file is an archive if it opens with the ar magic; otherwise treat it as a relocatable object. */
static int is_archive(const char *path) {
	FILE *f = fopen(path, "rb"); if (!f) die("cannot open %s", path);
	char m[8]; size_t n = fread(m, 1, 8, f); fclose(f);
	return n == 8 && !memcmp(m, "!<arch>\n", 8);
}

/* Does a file exist and open? (probe before load_shared, which die()s on a missing path.) */
static int file_exists(const char *p) { FILE *f = fopen(p, "rb"); if (f) { fclose(f); return 1; } return 0; }

int main(int argc, char **argv) {
	const char *out = "a.out";
	const char *libnames[MAXSHLIB]; int nlibname = 0;    /* -l names, resolved to files after the parse loop */
	const char *libdirs[32];        int nlibdir  = 0;    /* -L search directories */
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
		else if (!strcmp(argv[i], "-Ttext") && i + 1 < argc) load_base = strtoul(argv[++i], NULL, 0);   /* text base */
		else if (!strncmp(argv[i], "-Ttext=", 7)) load_base = strtoul(argv[i] + 7, NULL, 0);
		else if ((!strcmp(argv[i], "-e") || !strcmp(argv[i], "--entry")) && i + 1 < argc) entry_sym = argv[++i];
		else if (!strcmp(argv[i], "-pie") || !strcmp(argv[i], "--pie")) { pie = 1; load_base = 0; }   /* PIE: link at 0, self-relocate */
		else if (!strcmp(argv[i], "-shared") || !strcmp(argv[i], "--shared")) { shared = 1; load_base = 0; }   /* .so: ET_DYN, no entry */
		else if (!strcmp(argv[i], "-soname") && i + 1 < argc) soname = argv[++i];
		else if (!strncmp(argv[i], "-soname=", 8)) soname = argv[i] + 8;
		else if (!strncmp(argv[i], "-l", 2) && argv[i][2]) { if (nlibname >= MAXSHLIB) die("too many -l"); libnames[nlibname++] = argv[i] + 2; }
		else if (!strcmp(argv[i], "-l") && i + 1 < argc)    { if (nlibname >= MAXSHLIB) die("too many -l"); libnames[nlibname++] = argv[++i]; }
		else if (!strncmp(argv[i], "-L", 2) && argv[i][2]) { if (nlibdir >= 32) die("too many -L"); libdirs[nlibdir++] = argv[i] + 2; }
		else if (!strcmp(argv[i], "-L") && i + 1 < argc)    { if (nlibdir >= 32) die("too many -L"); libdirs[nlibdir++] = argv[++i]; }
		else if (argv[i][0] == '-') die("unknown option '%s'", argv[i]);
		else if (is_archive(argv[i])) ar_load(argv[i]);   /* lazy members, pulled on demand below */
		else elf_load(argv[i]);                           /* always-linked object */
	}
	if (!nobj) die("usage: ld [-o out] [-Ttext addr] [-e sym] [-shared] [-L dir] [-l name] obj.o|lib.a ...");

	/* Resolve each -l<name> to lib<name>.so under a -L dir and read its exports (a provider). */
	for (int i = 0; i < nlibname; i++) {
		char path[512]; int loaded = 0;
		for (int d = 0; d < nlibdir && !loaded; d++) {
			snprintf(path, sizeof path, "%s/lib%s.so", libdirs[d], libnames[i]);
			if (file_exists(path)) { load_shared(path); loaded = 1; }
		}
		if (!loaded) die("cannot find -l%s (searched %d -L dir(s) for lib%s.so)", libnames[i], nlibdir, libnames[i]);
	}
	pull_archive_members();

	Layout L = {0};
	if (shared && !soname) { const char *b = strrchr(out, '/'); soname = b ? b + 1 : out; }
	build_imports();                                     /* undefined refs -> providers; sizes .plt/.dynsym (sets nimport) */
	/* Publish this object's defined globals in .dynsym: for a .so, its export set; for a dynamic PROGRAM,
	 * so a provider can resolve back into it (e.g. libc.so's `main`/`errno` -> the program) — like
	 * --export-dynamic. After build_imports (needs nimport); sizes .dynsym -> before layout. */
	if (shared || nimport) build_exports();
	build_got();                                         /* PIC: R_ARM_GOT_PREL refs -> GOT slots; sizes .got */
	layout(&L);
	build_globals();
	finalize_got();                                      /* fill each GOT slot's link-time value (post-addresses) */
	if (pie || shared) gsym_define("_DYNAMIC", L.dynamic_vaddr);   /* so `.word _DYNAMIC` finds the array */
	u32 entry = 0;                                       /* a plain .so has none; ld.so IS a .so WITH an entry */
	GSym *start = gsym_find(entry_sym);
	if (start && start->defined) entry = start->vaddr;
	else if (!shared) die("no '%s' symbol (entry point)", entry_sym);
	relocate(&L);
	elf_write_exec(out, entry, &L);
	return 0;
}
