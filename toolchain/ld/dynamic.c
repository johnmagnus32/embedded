/*
 * dynamic.c — the dynamic-linking tables, built ONCE.
 *
 * One scan over the relocations (dyn_scan) decides everything that depends on them: which undefined symbols are
 * IMPORTS resolved at run time (and which of those need a PLT stub or, for an executable's data reference, a COPY
 * slot), which symbols need a GOT slot, and which words need a runtime fixup. Each decision is a list entry. The
 * linker-made sections (.interp .hash .dynsym .dynstr .rel.dyn .rel.plt .plt .dynamic .got.plt .got .dynbss) are
 * sized from those lists (dyn_size) and, after layout, filled from the SAME lists (dyn_fill) — so no separate
 * counting pass has to agree with a writing pass.
 *
 * The sections belong to linker_obj, a pseudo input object that the layout engine places like any other: the
 * default script names them, a -T script receives them as orphans. A dynamic relocation names its target as
 * (object, section, offset), so it is right wherever layout puts that section.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ld.h"

#define INTERP_PATH "/lib/ld.so.1"   /* the runtime loader a dynamic program names in PT_INTERP */

Obj *linker_obj;
static const struct { const char *name; u32 type, flags, align, entsize; int link, info; } L_def[L_NSEC] = {
	[L_INTERP]  = { ".interp",  SHT_PROGBITS, SHF_ALLOC,               1, 0,                 0,        0 },
	[L_HASH]    = { ".hash",    SHT_HASH,     SHF_ALLOC,               4, 4,                 L_DYNSYM, 0 },
	[L_DYNSYM]  = { ".dynsym",  SHT_DYNSYM,   SHF_ALLOC,               4, sizeof(Elf32_Sym), L_DYNSTR, 1 },   /* info: 1st global */
	[L_DYNSTR]  = { ".dynstr",  SHT_STRTAB,   SHF_ALLOC,               1, 0,                 0,        0 },
	[L_RELDYN]  = { ".rel.dyn", SHT_REL,      SHF_ALLOC,               4, sizeof(Elf32_Rel), L_DYNSYM, 0 },
	[L_RELPLT]  = { ".rel.plt", SHT_REL,      SHF_ALLOC,               4, sizeof(Elf32_Rel), L_DYNSYM, L_GOTPLT },
	[L_PLT]     = { ".plt",     SHT_PROGBITS, SHF_ALLOC|SHF_EXECINSTR, 4, 0,                 0,        0 },
	[L_DYNAMIC] = { ".dynamic", SHT_DYNAMIC,  SHF_ALLOC,               4, sizeof(Elf32_Dyn), L_DYNSTR, 0 },
	[L_GOTPLT]  = { ".got.plt", SHT_PROGBITS, SHF_ALLOC|SHF_WRITE,     4, 4,                 0,        0 },
	[L_GOT]     = { ".got",     SHT_PROGBITS, SHF_ALLOC|SHF_WRITE,     4, 4,                 0,        0 },
	[L_DYNBSS]  = { ".dynbss",  SHT_NOBITS,   SHF_ALLOC|SHF_WRITE,     4, 0,                 0,        0 },
};

/* An IMPORT: an undefined symbol a provider (or, for a .so, the run time) resolves. plt >= 0: called, gets a PLT
 * stub + GOT slot + JUMP_SLOT. is_data: an executable addresses it absolutely — it gets a .dynbss copy + COPY. */
typedef struct { const char *name; int lib, plt, dynsym, is_data; u32 copy_off, copy_size, stroff; } Import;
/* A GOT slot: a local definition (obj/symidx), a global one (by name), or an import (its GLOB_DAT fills it). */
typedef struct { const char *name; Obj *obj; int symidx, import; } Got;
/* A dynamic relocation at (obj's section shndx + off); sym = its .dynsym index (0 for RELATIVE). */
typedef struct { Obj *obj; int shndx; u32 off, type; int sym; } DynRel;
typedef struct { Obj *obj; int symidx; u32 stroff; } Export;
typedef struct { u32 tag; int what, sec; u32 val; } DynEnt;   /* what: 0 = val, 1 = sec's address, 2 = sec's size */

static Import *imports; static int nimport, impcap; static StrMap import_map;
static Got *got;        static int ngot, gotcap;      static StrMap got_map;
static DynRel *relative, *symrel, *relplt; static int nrelative, relcap, nsymrel, symcap, nrelplt, pltcap;
static Export *exports; static int nexport, expcap;
static DynEnt *dyn;     static int ndyn, dyncap;
static Strtab dynstr; static u32 *needed_off, soname_off;
static int nplt;
static u32 dynbss_size;

static int dynamic_image(void) { return pie || shared; }   /* loaded at a bias: its absolute words need fixups */

void dyn_init(void) {
	Obj *o = linker_obj = obj_new();
	o->path = "<linker>"; o->active = 1; o->nsh = L_NSEC;
	o->sh = calloc(L_NSEC, sizeof *o->sh);
	o->sec_vaddr = calloc(L_NSEC, sizeof(u32)); o->sec_lma = calloc(L_NSEC, sizeof(u32));
	o->sec_out = calloc(L_NSEC, sizeof *o->sec_out);
	Strtab names = {0}; str_add(&names, "");
	for (int i = 1; i < L_NSEC; i++)
		o->sh[i] = (Elf32_Shdr){ .sh_name = str_add(&names, L_def[i].name), .sh_type = L_def[i].type, .sh_flags = L_def[i].flags,
		    .sh_addralign = L_def[i].align, .sh_entsize = L_def[i].entsize, .sh_link = L_def[i].link, .sh_info = L_def[i].info };
	o->shstr = names.b;
	GSym *d = gsym_find("_DYNAMIC");                     /* linker-provided: the .dynamic array, if there is one */
	if (d && !d->defined) d->linker = 1;
}

/* ---- provider exports ------------------------------------------------------------------------------ */
static StrMap shexport_map;   /* name -> its first provider's ShExport */
static ShExport *shexport_of(const char *name) {
	if (!shexport_map.n) for (int i = nshexport - 1; i >= 0; i--) strmap_put(&shexport_map, shexports[i].name, &shexports[i]);
	return strmap_get(&shexport_map, name);
}

/* ---- the relocation scan ---------------------------------------------------------------------------- */
static int import_of(const char *name, int lib) {
	long hit = (long)strmap_get(&import_map, name);
	if (hit) return (int)hit - 1;
	imports = grow(imports, nimport, &impcap, sizeof *imports);
	imports[nimport] = (Import){ .name = name, .lib = lib, .plt = -1, .dynsym = 1 + nimport };
	strmap_put(&import_map, name, (void *)(long)(nimport + 1));
	if (lib >= 0) shlibs[lib].used = 1;                  /* a provider we use -> DT_NEEDED */
	return nimport++;
}
static void add_rel(DynRel **v, int *n, int *cap, Obj *o, int shndx, u32 off, u32 type, int sym) {
	*v = grow(*v, *n, cap, sizeof **v);
	(*v)[(*n)++] = (DynRel){ o, shndx, off, type, sym };
}
/* A runtime fixup of a word inside an input section: that section must be writable. We emit no DT_TEXTREL (and
 * our loader maps code read-only), so a fixup in code is a link error, not a crash at load time — as in lld. */
static void runtime_fixup(DynRel **v, int *n, int *cap, Obj *o, int t, u32 off, u32 type, int sym, const char *what) {
	if (!(o->sh[t].sh_flags & SHF_WRITE))
		die("%s: absolute reference to '%s' in read-only section %s needs a runtime fixup (a text relocation) — "
		    "make it position-independent (-fPIC / PC-relative)", o->path, what, sec_name(o, t));
	add_rel(v, n, cap, o, t, off, type, sym);
}
static int import_if_external(const char *name) {        /* -1: not an import (defined here / undefined) */
	if (defined_locally(name)) return -1;
	ShExport *e = shexport_of(name);
	if (!e && !shared) return -1;                        /* a .so may import with no known provider; a program may not */
	return import_of(name, e ? e->lib : -1);
}

/* A GOT slot for (o, symidx): a LOCAL symbol keyed by object+index (same-named statics differ), others by name. */
static void got_slot(Obj *o, int symidx) {
	Elf32_Sym *s = &o->sym[symidx];
	const char *nm = o->strtab + s->st_name;
	int local = s->st_shndx != SHN_UNDEF && ELF32_ST_BIND(s->st_info) == STB_LOCAL;
	char key[48]; if (local) snprintf(key, sizeof key, "@%p:%d", (void *)o, symidx);
	if (strmap_get(&got_map, local ? key : nm)) return;
	got = grow(got, ngot, &gotcap, sizeof *got);
	Got *g = &got[ngot]; *g = (Got){ .name = nm, .obj = local ? o : NULL, .symidx = symidx, .import = -1 };
	strmap_put(&got_map, local ? strdup(key) : nm, (void *)(long)(ngot + 1));
	u32 off = 4u * (u32)ngot++;
	if (!local && (g->import = import_if_external(nm)) >= 0)          /* the loader writes the import's address */
		add_rel(&symrel, &nsymrel, &symcap, linker_obj, L_GOT, off, md_r_glob_dat, imports[g->import].dynsym);
	else if (!local && !defined_locally(nm)) {
		if (ELF32_ST_BIND(s->st_info) != STB_WEAK) die("undefined symbol '%s' (GOT reference in %s)", nm, o->path);
	} else if (dynamic_image() && !sym_is_abs(o, symidx))            /* holds a link-time address: add the bias */
		add_rel(&relative, &nrelative, &relcap, linker_obj, L_GOT, off, md_r_relative, 0);
}

static void scan_one(Obj *o, int t, u32 off, u32 type, int symidx) {
	if (md_is_got_reloc(type)) { got_slot(o, symidx); return; }
	Elf32_Sym *s = &o->sym[symidx];
	if (s->st_shndx == SHN_UNDEF && s->st_name) {
		const char *nm = o->strtab + s->st_name;
		int imp = import_if_external(nm);
		if (imp >= 0) {
			Import *im = &imports[imp];
			if (md_is_call_reloc(type)) {                /* a call -> through a PLT stub */
				if (im->plt < 0) {
					im->plt = nplt++;
					add_rel(&relplt, &nrelplt, &pltcap, linker_obj, L_GOTPLT, 4u * (u32)im->plt, md_r_jump_slot, im->dynsym);
				}
			} else if (shared) {                         /* a .so: the loader stores S + A into the word itself */
				if (!md_needs_dynamic_reloc(type))
					die("%s: relocation type %u against imported '%s' can't be resolved at run time", o->path, type, nm);
				runtime_fixup(&symrel, &nsymrel, &symcap, o, t, off, md_r_abs32, im->dynsym, nm);
			} else if (!im->is_data) {                   /* a program: a copy of the variable in its own .dynbss */
				ShExport *e = shexport_of(nm);
				if (!e || !e->size) die("data import '%s': provider records no size (st_size=0), cannot size its copy relocation", nm);
				im->is_data = 1; im->copy_size = e->size;
				dynbss_size = alignup(dynbss_size, 4); im->copy_off = dynbss_size; dynbss_size += e->size;
				add_rel(&symrel, &nsymrel, &symcap, linker_obj, L_DYNBSS, im->copy_off, md_r_copy, im->dynsym);
			}
			return;
		}
		if (!defined_locally(nm)) {
			if (ELF32_ST_BIND(s->st_info) != STB_WEAK) die("undefined symbol '%s' (referenced in %s)", nm, o->path);
			return;                                      /* an undefined weak reference is 0 — at any load address */
		}
	}
	if (!dynamic_image() || sym_is_abs(o, symidx)) return;
	if (md_is_abs_nonword(type))                         /* GNU ld: the same error */
		die("%s: absolute movw/movt reference to '%s' can't be position-independent — recompile with -fPIC",
		    o->path, s->st_name ? o->strtab + s->st_name : "(section)");
	if (md_needs_dynamic_reloc(type))
		runtime_fixup(&relative, &nrelative, &relcap, o, t, off, md_r_relative, 0, s->st_name ? o->strtab + s->st_name : sec_name(o, s->st_shndx));
}

void dyn_scan(void) {
	for (int i = 0; i < nobj; i++) {
		Obj *o = objs[i];
		if (!o->active || o == linker_obj) continue;
		for (int j = 0; j < o->nsh; j++) {
			Elf32_Shdr *rs = &o->sh[j];
			if (rs->sh_type != SHT_REL) continue;
			int t = (int)rs->sh_info;
			if (!(o->sh[t].sh_flags & SHF_ALLOC) || o->sec_out[t] == &os_discard) continue;   /* patches nothing we emit */
			Elf32_Rel *rel = (Elf32_Rel *)(o->data + rs->sh_offset);
			for (u32 r = 0; r < rs->sh_size / sizeof *rel; r++)
				scan_one(o, t, rel[r].r_offset, ELF32_R_TYPE(rel[r].r_info), (int)ELF32_R_SYM(rel[r].r_info));
		}
	}
}

/* A relocation whose target is a linker-made table entry: a GOT reference -> the symbol's GOT slot; a call to an
 * import -> its PLT stub; an executable's absolute reference to a data import -> its .dynbss copy; a .so's absolute
 * reference to an import -> 0 (the word keeps its addend; the loader adds S). Returns 0 for an ordinary target. */
int dyn_target(Obj *o, int symidx, u32 type, u32 *S) {
	Elf32_Sym *s = &o->sym[symidx];
	if (md_is_got_reloc(type)) {
		int local = s->st_shndx != SHN_UNDEF && ELF32_ST_BIND(s->st_info) == STB_LOCAL;
		char key[48]; if (local) snprintf(key, sizeof key, "@%p:%d", (void *)o, symidx);
		long i = (long)strmap_get(&got_map, local ? key : o->strtab + s->st_name);
		if (!i) die("internal: no GOT slot for '%s'", o->strtab + s->st_name);
		*S = linker_obj->sec_vaddr[L_GOT] + 4u * (u32)(i - 1);
		return 1;
	}
	if (s->st_shndx != SHN_UNDEF || !s->st_name) return 0;
	long i = (long)strmap_get(&import_map, o->strtab + s->st_name);
	if (!i) return 0;
	Import *im = &imports[i - 1];
	if (md_is_call_reloc(type)) *S = linker_obj->sec_vaddr[L_PLT] + md_plt_entsize * (u32)im->plt;
	else if (im->is_data)       *S = linker_obj->sec_vaddr[L_DYNBSS] + im->copy_off;
	else                        *S = 0;
	return 1;
}

/* ---- sizing ------------------------------------------------------------------------------------------ */
static void set_size(int sec, u32 size) { linker_obj->sh[sec].sh_size = size; }
static void dyn_add(u32 tag, int what, int sec, u32 val) {
	dyn = grow(dyn, ndyn, &dyncap, sizeof *dyn);
	dyn[ndyn++] = (DynEnt){ tag, what, sec, val };
}
static u32 dynsym_count(void) { return 1u + (u32)nimport + (u32)nexport; }
static u32 hash_nbucket(void) {   /* the largest prime (bfd's table) not above the symbol count: h % nbucket spreads */
	static const u32 primes[] = { 1,3,17,37,67,97,131,197,263,521,1031,2053,4099,8209,16411,0 };
	u32 best = 1;
	for (int i = 0; primes[i] && primes[i] <= dynsym_count(); i++) best = primes[i];
	return best;
}

/* Exports: the defined, default-visibility globals — for a .so its interface; for a dynamic program what its
 * providers may bind back to (libc's `main`/`environ`), like --export-dynamic. Object order, winning definition only. */
static void collect_exports(void) {
	for (int i = 0; i < nobj; i++) {
		Obj *o = objs[i];
		if (!o->active || o == linker_obj) continue;
		for (int k = 0; k < o->nsym; k++) {
			Elf32_Sym *s = &o->sym[k]; int b = ELF32_ST_BIND(s->st_info);
			if ((b != STB_GLOBAL && b != STB_WEAK) || s->st_shndx == SHN_UNDEF || s->st_shndx == SHN_ABS || !s->st_name) continue;
			if (ELF32_ST_VISIBILITY(s->st_other) != STV_DEFAULT) continue;
			GSym *g = gsym_find(o->strtab + s->st_name);
			if (!g || g->obj != o || g->symidx != k) continue;       /* overridden (a weak def lost to a strong one) */
			exports = grow(exports, nexport, &expcap, sizeof *exports);
			exports[nexport++] = (Export){ o, k, 0 };
		}
	}
}

void dyn_size(void) {
	int has_dynsym = shared || nimport;
	int has_dynamic = dynamic_image() || nimport;
	if (!has_dynamic) {                                  /* a static image: only the GOT (filled with final addresses) */
		set_size(L_GOT, 4u * (u32)ngot);
		linker_obj->data = calloc(1, 4u * (u32)ngot + 1);
		linker_obj->sh[L_GOT].sh_offset = 0;
		return;
	}
	if (shared && !soname) die("internal: -shared without a soname");
	if (has_dynsym) collect_exports();

	str_add(&dynstr, "");
	for (int i = 0; i < nimport; i++) imports[i].stroff = str_add(&dynstr, imports[i].name);
	for (int i = 0; i < nexport; i++) exports[i].stroff = str_add(&dynstr, exports[i].obj->strtab + exports[i].obj->sym[exports[i].symidx].st_name);
	needed_off = calloc(nshlib + 1, sizeof *needed_off);
	for (int i = 0; i < nshlib; i++) if (shlibs[i].used) needed_off[i] = str_add(&dynstr, shlibs[i].soname);
	if (shared) soname_off = str_add(&dynstr, soname);

	for (int i = 0; i < nshlib; i++) if (shlibs[i].used) dyn_add(DT_NEEDED, 0, 0, needed_off[i]);
	if (shared) dyn_add(DT_SONAME, 0, 0, soname_off);
	if (has_dynsym) {
		dyn_add(DT_HASH, 1, L_HASH, 0);     dyn_add(DT_STRTAB, 1, L_DYNSTR, 0); dyn_add(DT_SYMTAB, 1, L_DYNSYM, 0);
		dyn_add(DT_STRSZ, 2, L_DYNSTR, 0);  dyn_add(DT_SYMENT, 0, 0, sizeof(Elf32_Sym));
	}
	if (nplt) {
		dyn_add(DT_PLTGOT, 1, L_GOTPLT, 0); dyn_add(DT_PLTRELSZ, 2, L_RELPLT, 0);
		dyn_add(DT_PLTREL, 0, 0, DT_REL);   dyn_add(DT_JMPREL, 1, L_RELPLT, 0);
	}
	if (nrelative + nsymrel) {
		dyn_add(DT_REL, 1, L_RELDYN, 0);    dyn_add(DT_RELSZ, 2, L_RELDYN, 0);
		dyn_add(DT_RELENT, 0, 0, sizeof(Elf32_Rel)); dyn_add(DT_RELCOUNT, 0, 0, (u32)nrelative);   /* RELATIVE entries lead */
	}
	dyn_add(DT_NULL, 0, 0, 0);

	if (nimport && !shared) set_size(L_INTERP, sizeof INTERP_PATH);
	if (has_dynsym) {
		set_size(L_HASH, (2 + hash_nbucket() + dynsym_count()) * 4);   /* [nbucket, nchain, bucket[], chain[]] */
		set_size(L_DYNSYM, dynsym_count() * sizeof(Elf32_Sym));
		set_size(L_DYNSTR, (u32)dynstr.len);
	}
	set_size(L_RELDYN, (u32)(nrelative + nsymrel) * sizeof(Elf32_Rel));
	set_size(L_RELPLT, (u32)nrelplt * sizeof(Elf32_Rel));
	set_size(L_PLT, (u32)nplt * md_plt_entsize);
	set_size(L_DYNAMIC, (u32)ndyn * sizeof(Elf32_Dyn));
	set_size(L_GOTPLT, 4u * (u32)nplt);
	set_size(L_GOT, 4u * (u32)ngot);
	set_size(L_DYNBSS, dynbss_size);
	u32 total = 0;
	for (int i = 1; i < L_NSEC; i++) {
		Elf32_Shdr *s = &linker_obj->sh[i];
		if (s->sh_type == SHT_NOBITS) continue;
		total = alignup(total, s->sh_addralign); s->sh_offset = total; total += s->sh_size;
	}
	linker_obj->data = calloc(1, total + 1);
}

/* ---- contents (after layout) ------------------------------------------------------------------------- */
static u8 *sec_bytes(int sec) { return linker_obj->data + linker_obj->sh[sec].sh_offset; }
static u32 at(const DynRel *r) { return r->obj->sec_vaddr[r->shndx] + r->off; }
static void put_rels(u8 *p, const DynRel *v, int n) {
	for (int i = 0; i < n; i++, p += 8) { wr32(p, at(&v[i])); wr32(p + 4, ELF32_R_INFO((u32)v[i].sym, v[i].type)); }
}
/* SysV ELF hash (System V gABI) — MUST match the runtime loader's elf_hash (libc/ld/src/reloc.h). */
static u32 elf_hash(const char *s) {
	u32 h = 0, g;
	for (const u8 *p = (const u8 *)s; *p; p++) { h = (h << 4) + *p; g = h & 0xf0000000u; if (g) h ^= g >> 24; h &= ~g; }
	return h;
}
static u32 out_index(Obj *o, int shndx) { OutSec *os = o->sec_out[shndx]; return os && os != &os_discard ? (u32)os->index : 0; }

void dyn_fill(void) {
	GSym *d = gsym_find("_DYNAMIC");
	if (d && d->linker) {
		if (linker_obj->sh[L_DYNAMIC].sh_size) { d->defined = 1; d->vaddr = linker_obj->sec_vaddr[L_DYNAMIC]; d->os = linker_obj->sec_out[L_DYNAMIC]; }
		else d->linker = 0;                              /* no .dynamic: a weak reference is 0, a strong one undefined */
	}
	if (linker_obj->sh[L_INTERP].sh_size) memcpy(sec_bytes(L_INTERP), INTERP_PATH, sizeof INTERP_PATH);
	for (int k = 0; k < nplt; k++)
		md_plt_entry(sec_bytes(L_PLT) + md_plt_entsize * k, linker_obj->sec_vaddr[L_PLT] + md_plt_entsize * (u32)k,
		             linker_obj->sec_vaddr[L_GOTPLT] + 4u * (u32)k);
	for (int i = 0; i < ngot; i++) {                     /* a definition's address (loader adds the bias); an import 0 */
		Got *g = &got[i]; u32 v = 0;
		if (g->obj) v = sym_addr(g->obj, g->symidx);
		else if (g->import < 0) { GSym *s = gsym_find(g->name); v = s && s->defined ? s->vaddr : 0; }
		wr32(sec_bytes(L_GOT) + 4 * i, v);
	}
	put_rels(sec_bytes(L_RELDYN), relative, nrelative);
	put_rels(sec_bytes(L_RELDYN) + 8 * nrelative, symrel, nsymrel);
	put_rels(sec_bytes(L_RELPLT), relplt, nrelplt);
	if (linker_obj->sh[L_DYNSYM].sh_size) {
		memcpy(sec_bytes(L_DYNSTR), dynstr.b, dynstr.len);
		u8 *p = sec_bytes(L_DYNSYM) + sizeof(Elf32_Sym);   /* [0] = STN_UNDEF */
		for (int i = 0; i < nimport; i++, p += sizeof(Elf32_Sym)) {   /* a data import carries its size (the COPY length) */
			Elf32_Sym d = { .st_name = imports[i].stroff, .st_size = imports[i].is_data ? imports[i].copy_size : 0,
			    .st_info = ELF32_ST_INFO(STB_GLOBAL, imports[i].is_data ? STT_OBJECT : STT_FUNC), .st_shndx = SHN_UNDEF };
			memcpy(p, &d, sizeof d);
		}
		for (int i = 0; i < nexport; i++, p += sizeof(Elf32_Sym)) {
			Elf32_Sym *s = &exports[i].obj->sym[exports[i].symidx];
			Elf32_Sym d = { .st_name = exports[i].stroff, .st_value = sym_addr(exports[i].obj, exports[i].symidx),
			    .st_size = s->st_size, .st_info = s->st_info, .st_other = s->st_other,
			    .st_shndx = (u16)out_index(exports[i].obj, s->st_shndx) };
			memcpy(p, &d, sizeof d);
		}
		u32 nbucket = hash_nbucket(), nchain = dynsym_count();
		u8 *h = sec_bytes(L_HASH), *bucket = h + 8, *chain = h + 8 + 4 * nbucket;
		wr32(h, nbucket); wr32(h + 4, nchain);
		for (u32 y = 1; y < nchain; y++) {               /* prepend: newest at the bucket head */
			const char *nm = y <= (u32)nimport ? imports[y - 1].name : dynstr.b + exports[y - 1 - nimport].stroff;
			u32 b = elf_hash(nm) % nbucket;
			wr32(chain + 4 * y, rd32(bucket + 4 * b)); wr32(bucket + 4 * b, y);
		}
	}
	for (int i = 0; i < ndyn; i++) {
		u32 v = dyn[i].what == 1 ? linker_obj->sec_vaddr[dyn[i].sec] : dyn[i].what == 2 ? linker_obj->sh[dyn[i].sec].sh_size : dyn[i].val;
		wr32(sec_bytes(L_DYNAMIC) + 8 * i, dyn[i].tag); wr32(sec_bytes(L_DYNAMIC) + 8 * i + 4, v);
	}
}
