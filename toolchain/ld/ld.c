/*
 * ld.c — the linker FRONT-END (architecture- and format-generic linking algorithm) + driver.
 *
 * Produces an ELF executable (static, PIE, or dynamically linked) or a shared object from relocatable objects,
 * archives and shared libraries — the inverse of the assembler: `as` emitted section-relative bytes +
 * relocations; `ld` gives them real addresses and patches them in. The phases, in order:
 *
 *   load       objects, archive indexes, shared-library exports                       (elf.c)
 *   resolve    the global symbol table; archive members pulled for strong references  (here)
 *   script     read the linker script (-T, or the built-in default)                     (script.c)
 *   match      input sections -> output sections                                        (script.c)
 *   scan       one relocation pass: imports, PLT, GOT, dynamic relocations              (dynamic.c)
 *   layout     orphans, addresses, segments                                             (script.c)
 *   fill       the linker-made tables' contents                                         (dynamic.c)
 *   relocate   patch every relocation (encodings in arm.c)                              (here)
 *   write      the output file                                                          (elf.c)
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "ld.h"

Obj **objs; int nobj; static int objcap;
Obj *obj_new(void) {
	objs = grow(objs, nobj, &objcap, sizeof *objs);
	return objs[nobj++] = calloc(1, sizeof(Obj));
}
u32 load_base = 0x00010000u;               /* default layout: image base; -Ttext <addr> overrides (bare-metal 0x40000000) */
int pie = 0;                               /* -pie: ET_DYN, base 0, absolute refs become load-bias fixups */
int shared = 0;                            /* -shared: emit a .so — exported .dynsym/.hash, no required entry */
const char *soname = NULL;                 /* -soname NAME -> DT_SONAME (default: the output basename) */
const char *entry_sym = NULL;              /* -e; else the script's ENTRY(); else _start */
int bsymbolic = 0;                         /* -Bsymbolic */
const char *interp_path = "/lib/ld.so.1";  /* --dynamic-linker: the PT_INTERP a dynamic program names */
int stack_override;                        /* -z noexecstack / execstack: PT_GNU_STACK's flags, else from the inputs */
int norelro;                               /* -z norelro */
int warn_rwx = 1;                          /* --no-warn-rwx-segments turns the RWX-segment warning off */
int orphan_mode;                           /* --orphan-handling=place|discard|warn|error */
int no_undefined;                          /* --no-undefined, -z defs */
const char **defsyms; int ndefsym;         /* --defsym */
static const char *version_files[8]; static int nversion_file;   /* --version-script */

/* die() is tool-specific (its own "ld:" prefix); rd32/wr32/alignup/Strtab are shared (common/elfutil). */
void die(const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	fputs("ld: ", stderr); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap); exit(1);
}
void *grow(void *v, int n, int *cap, size_t esz) {
	if (n < *cap) return v;
	*cap = *cap ? *cap * 2 : 64;
	if (!(v = realloc(v, (size_t)*cap * esz))) die("out of memory");
	return v;
}

/* ---- global symbol table ------------------------------------------------------------------------- */
/* ELF symbol binding: a STRONG (STB_GLOBAL) definition overrides a WEAK one; two strong ones are an error; of
 * two weak ones the first stays. An undefined WEAK reference resolves to 0 (see resolve()); only a STRONG
 * reference pulls an archive member (a member whose definition is weak still satisfies it, as in GNU ld). */
static StrMap gsyms;
GSym *gsym_find(const char *name) { return strmap_get(&gsyms, name); }
GSym *gsym_get(const char *name) {
	GSym *g = strmap_get(&gsyms, name);
	if (!g) { g = calloc(1, sizeof *g); g->name = name; strmap_put(&gsyms, name, g); }
	return g;
}
static const char **pending; static int npending, pendcap;   /* strongly referenced, not yet defined: archive candidates */
/* Enter one object's global/weak symbols: its definitions (binding rules) and its undefined references. */
static void add_symbols(Obj *o) {
	for (int k = 0; k < o->nsym; k++) {
		Elf32_Sym *s = &o->sym[k]; int b = ELF32_ST_BIND(s->st_info);
		if ((b != STB_GLOBAL && b != STB_WEAK) || !s->st_name) continue;
		const char *name = o->strtab + s->st_name;
		GSym *g = gsym_get(name);
		if (s->st_shndx == SHN_COMMON) die("%s: COMMON symbol '%s' (compile with -fno-common)", o->path, name);
		if (s->st_shndx == SHN_UNDEF) {
			if (b == STB_GLOBAL && !g->strong_ref) {
				g->strong_ref = 1;
				if (!g->defined) { pending = grow(pending, npending, &pendcap, sizeof *pending); pending[npending++] = name; }
			}
			continue;
		}
		int weak = b == STB_WEAK;
		if (g->defined) {
			if (!weak && !g->weak) die("duplicate definition of '%s' (in %s and %s)", name, g->obj->path, o->path);
			if (weak || !g->weak) continue;              /* the existing (strong, or first weak) one stays */
		}
		g->defined = 1; g->weak = weak; g->obj = o; g->symidx = k;
		g->hidden = ELF32_ST_VISIBILITY(s->st_other) != STV_DEFAULT;
	}
}
/* Symbol resolution: enter every command-line object, then pull archive members for strongly referenced,
 * still-undefined symbols until nothing new is needed (resolution across all archives at once, like
 * --start-group). Unresolved references are reported when the relocation scan meets them. */
static void resolve_symbols(void) {
	for (int i = 0; i < nobj; i++) add_symbols(objs[i]);
	for (int i = 0; i < npending; i++) {                 /* the list grows as pulled members add references */
		if (gsym_find(pending[i])->defined) continue;
		Obj *m = ar_pull(pending[i]);
		if (m) add_symbols(m);
	}
}
static int discarded(const Obj *o, int shndx) {
	return shndx != SHN_ABS && shndx < 0xff00 && (!o->sec_out[shndx] || o->sec_out[shndx] == &os_discard);
}
/* After layout: every object-defined global's final address (one in a discarded section no longer defines it). */
static void build_globals(void) {
	for (size_t i = 0; i < gsyms.cap; i++) {
		GSym *g = gsyms.vals[i];
		if (!g || !g->defined || !g->obj) continue;
		if (discarded(g->obj, g->obj->sym[g->symidx].st_shndx)) { g->defined = 0; continue; }
		g->vaddr = sym_addr(g->obj, g->symidx);
	}
}
int defined_locally(const char *name) { GSym *g = gsym_find(name); return g && (g->defined || g->linker); }
u32 sym_addr(const Obj *o, int symidx) {
	const Elf32_Sym *s = &o->sym[symidx];
	return s->st_shndx == SHN_ABS ? s->st_value : o->sec_vaddr[s->st_shndx] + s->st_value;
}
int sym_is_abs(const Obj *o, int symidx) {
	const Elf32_Sym *s = &o->sym[symidx];
	int b = ELF32_ST_BIND(s->st_info);
	if (s->st_shndx != SHN_UNDEF && (b == STB_LOCAL || !s->st_name)) return s->st_shndx == SHN_ABS;   /* global: the winner's */
	GSym *g = gsym_find(o->strtab + s->st_name);
	if (!g || !(g->defined || g->linker)) return 1;      /* undefined weak: 0 at any load address */
	return g->obj ? g->obj->sym[g->symidx].st_shndx == SHN_ABS : g->abs;
}

/* A relocation's symbol, resolved to its final address. A GLOBAL or WEAK name — even one this object defines —
 * means the link's winning definition (a weak one here may have lost to a strong one elsewhere: musl's
 * weak_alias(dummy, __stdout_used) is overridden by stdout.o's), so it's looked up by name; only a LOCAL symbol is
 * this object's own. */
static u32 resolve(Obj *o, int symidx) {
	Elf32_Sym *s = &o->sym[symidx];
	int b = ELF32_ST_BIND(s->st_info);
	if (s->st_shndx == SHN_UNDEF || (s->st_name && (b == STB_GLOBAL || b == STB_WEAK))) {
		const char *nm = o->strtab + s->st_name;
		GSym *g = gsym_find(nm);
		if (g && g->defined) return g->vaddr;
		if (s->st_shndx == SHN_UNDEF && b == STB_WEAK) return 0;   /* an undefined weak reference is 0 */
		if (g && g->obj) die("'%s' (referenced in %s) is defined in a discarded section of %s", nm, o->path, g->obj->path);
		die("undefined symbol '%s' (referenced in %s)", nm, o->path);
	}
	if (discarded(o, s->st_shndx))
		die("%s: reference to %s in discarded section %s", o->path,
		    s->st_name ? o->strtab + s->st_name : "a symbol", sec_name(o, s->st_shndx));
	return sym_addr(o, symidx);
}

/* ---- long-branch veneers ------------------------------------------------------------------------------- *
 * An ARM b/bl reaches +-32MB. One farther goes through a VENEER (GNU's long-branch stub): in a stub section after its
 * caller's STUB GROUP — consecutive input sections of one code output section spanning at most GROUP bytes (GNU's
 * default 4170000), so every caller in the group reaches it — 12 bytes `ldr ip, [pc]; add pc, pc, ip; .word dest-.-12`
 * (--pic-veneer, -pie, -shared: position-independent) or 8 `ldr pc, [pc, #-4]; .word dest`, named __<target>_veneer.
 * Stubs move what follows them: scan, lay out again, until none is new. */
#define VENEER_GROUP 4170000u
int pic_veneer;                                          /* --pic-veneer */
int strip_all;                                           /* -s: no .symtab/.strtab */
typedef struct { Obj *stub; int n; } VGroup;             /* the group's stub section: a linker object of one section */
typedef struct { int g; u32 off; Obj *o; int sidx; s32 A; u32 type; const char *name; } Veneer;   /* its target: o's symbol sidx, + A */
static VGroup *vgrp; static int nvgrp, vgrpcap;
static Veneer *ven; static int nven, vencap;
static StrMap ven_map;                                   /* "group:target" -> veneer + 1 (the target by identity: addresses move) */
static int **vgrp_of;                                    /* [obj][section] -> its group + 1 (0: none yet) */
static u32 veneer_size(void) { return pic_veneer || pie || shared ? 12 : 8; }
static int branch_reloc(u32 type) { return type == 28 /* CALL */ || type == 29 /* JUMP24 */ || type == 1 /* PC24 */ || type == 27 /* PLT32 */; }
static u32 resolve(Obj *o, int symidx);
static int branch_target(Obj *o, int t, Elf32_Rel *r, u32 *dest, u32 *P, s32 *A) {   /* 0: not a branch to resolve here */
	u32 type = ELF32_R_TYPE(r->r_info); int sidx = (int)ELF32_R_SYM(r->r_info);
	if (!branch_reloc(type) || !sidx) return 0;
	u32 w = rd32(o->data + o->sh[t].sh_offset + r->r_offset), S;
	if (!dyn_target(o, sidx, type, &S)) S = resolve(o, sidx);
	*A = (s32)(w << 8) >> 6; *P = o->sec_vaddr[t] + r->r_offset; *dest = S + (u32)*A + 8;
	return 1;
}
static int in_reach(u32 dest, u32 P) { s32 d = (s32)(dest - (P + 8)); return d >= -0x2000000 && d <= 0x1fffffc; }
static void target_key(char *key, size_t n, int g, Obj *o, int sidx, s32 A) {   /* a global by name, else this object's symbol */
	Elf32_Sym *s = &o->sym[sidx];
	if (s->st_name && ELF32_ST_BIND(s->st_info) != STB_LOCAL) snprintf(key, n, "%d:%s%+d", g, o->strtab + s->st_name, A);
	else snprintf(key, n, "%d:%p:%d%+d", g, (void *)o, sidx, A);
}
static const char *branch_name(Obj *o, int sidx) {
	Elf32_Sym *s = &o->sym[sidx];
	return s->st_name ? o->strtab + s->st_name : ELF32_ST_TYPE(s->st_info) == STT_SECTION ? sec_name(o, s->st_shndx) : "local";
}
static int vgroup_of(Obj *o, int t) { int i = 0; while (objs[i] != o) i++; return vgrp_of[i] ? vgrp_of[i][t] - 1 : -1; }
static void form_groups(void) {   /* every code output section's inputs, in address order, cut into groups of <= GROUP bytes */
	vgrp_of = calloc(nobj, sizeof *vgrp_of);
	typedef struct { Obj *o; int j, oi; } Piece;
	for (int s = 0; s < noutsec; s++) {
		OutSec *os = outsecs[s];
		if (!(os->flags & SHF_EXECINSTR) || os->type == SHT_NOBITS) continue;
		Piece *pc = NULL; int np = 0, cap = 0;
		for (int i = 0; i < nobj; i++) for (int j = 1; j < objs[i]->nsh; j++)
			if (objs[i]->sec_out && objs[i]->sec_out[j] == os && objs[i]->sh[j].sh_size) { pc = grow(pc, np, &cap, sizeof *pc); pc[np++] = (Piece){ objs[i], j, i }; }
		for (int a = 1; a < np; a++) for (int b = a; b > 0 && pc[b - 1].o->sec_vaddr[pc[b - 1].j] > pc[b].o->sec_vaddr[pc[b].j]; b--) { Piece x = pc[b]; pc[b] = pc[b - 1]; pc[b - 1] = x; }
		for (int a = 0; a < np; ) {
			u32 start = pc[a].o->sec_vaddr[pc[a].j]; int b = a;
			while (b + 1 < np && pc[b + 1].o->sec_vaddr[pc[b + 1].j] + pc[b + 1].o->sh[pc[b + 1].j].sh_size - start <= VENEER_GROUP) b++;
			Obj *st = obj_new();                     /* the group's stub section, after its last input */
			st->path = "<veneers>"; st->active = 1; st->nsh = 2;
			st->sh = calloc(2, sizeof *st->sh); st->sec_vaddr = calloc(2, sizeof(u32)); st->sec_lma = calloc(2, sizeof(u32)); st->sec_out = calloc(2, sizeof *st->sec_out);
			st->sh[1] = (Elf32_Shdr){ .sh_name = 1, .sh_type = SHT_PROGBITS, .sh_flags = SHF_ALLOC | SHF_EXECINSTR, .sh_addralign = 4 };
			st->shstr = "\0.stub";
			vgrp = grow(vgrp, nvgrp, &vgrpcap, sizeof *vgrp); vgrp[nvgrp++] = (VGroup){ st, 0 };
			for (int k = a; k <= b; k++) { if (!vgrp_of[pc[k].oi]) vgrp_of[pc[k].oi] = calloc(pc[k].o->nsh, sizeof **vgrp_of); vgrp_of[pc[k].oi][pc[k].j] = nvgrp; }
			layout_insert_after(pc[b].o, pc[b].j, st, 1);
			a = b + 1;
		}
		free(pc);
	}
}
static int veneer_scan(int add) {   /* the out-of-reach branches: count them (add: give each its group's veneer) */
	int fresh = 0;
	for (int i = 0; i < nobj; i++) {
		Obj *o = objs[i];
		if (!o->active || o == linker_obj || !o->nsym) continue;
		for (int j = 1; j < o->nsh; j++) {
			Elf32_Shdr *rs = &o->sh[j]; int t = (int)rs->sh_info;
			if (rs->sh_type != SHT_REL || !(o->sh[t].sh_flags & SHF_EXECINSTR) || !o->sec_out[t] || o->sec_out[t] == &os_discard) continue;
			Elf32_Rel *rel = (Elf32_Rel *)(o->data + rs->sh_offset);
			for (u32 r = 0; r < rs->sh_size / sizeof *rel; r++) {
				u32 dest, P; s32 A;
				if (!branch_target(o, t, &rel[r], &dest, &P, &A) || in_reach(dest, P)) continue;
				if (dest & 3) die("%s: a branch in %s to Thumb code out of reach: an interworking veneer is unsupported", o->path, sec_name(o, t));
				if (!add) { fresh++; continue; }
				int g = vgroup_of(o, t);
				if (g < 0) die("internal: %s(%s) is in no stub group", o->path, sec_name(o, t));
				char key[600]; int sidx = (int)ELF32_R_SYM(rel[r].r_info); target_key(key, sizeof key, g, o, sidx, A);
				if (strmap_get(&ven_map, key)) continue;
				ven = grow(ven, nven, &vencap, sizeof *ven);
				ven[nven] = (Veneer){ g, (u32)vgrp[g].n * veneer_size(), o, sidx, A, ELF32_R_TYPE(rel[r].r_info), branch_name(o, sidx) };
				strmap_put(&ven_map, strdup(key), (void *)(long)(++nven));
				vgrp[g].n++; vgrp[g].stub->sh[1].sh_size = (u32)vgrp[g].n * veneer_size(); fresh++;
			}
		}
	}
	return fresh;
}
static void build_globals(void);
static void startstop_define(void);
static void veneers(void) {
	if (!veneer_scan(0)) return;
	form_groups();
	for (int pass = 0; veneer_scan(1); pass++) {
		if (pass == 16) die("internal: veneers do not settle");
		layout_addresses(); build_globals(); startstop_define();
	}
	for (int g = 0; g < nvgrp; g++) {   /* the stubs' bytes and symbols: __X_veneer, $a, $d (as GNU's) */
		Obj *st = vgrp[g].stub; u32 size = st->sh[1].sh_size;
		st->data = calloc(1, size + 1);
		int nsym = 1 + 3 * vgrp[g].n; st->sym = calloc(nsym, sizeof *st->sym); st->nsym = nsym;
		Strtab names = {0}; str_add(&names, "");
		u32 a = str_add(&names, "$a"), d = str_add(&names, "$d"); int k = 1;
		for (int v = 0; v < nven; v++) {
			if (ven[v].g != g) continue;
			u8 *p = st->data + ven[v].off; u32 at = st->sec_vaddr[1] + ven[v].off, S;
			if (!dyn_target(ven[v].o, ven[v].sidx, ven[v].type, &S)) S = resolve(ven[v].o, ven[v].sidx);
			u32 dest = S + (u32)ven[v].A + 8;
			if (veneer_size() == 12) { wr32(p, 0xe59fc000); wr32(p + 4, 0xe08ff00c); wr32(p + 8, dest - at - 12); }   /* ldr ip, [pc]; add pc, pc, ip */
			else { wr32(p, 0xe51ff004); wr32(p + 4, dest); }                                                              /* ldr pc, [pc, #-4] */
			char *nm = malloc(strlen(ven[v].name) + 12); sprintf(nm, "__%s_veneer", ven[v].name);
			st->sym[k++] = (Elf32_Sym){ str_add(&names, nm), ven[v].off, veneer_size(), ELF32_ST_INFO(STB_LOCAL, STT_FUNC), 0, 1 };
			st->sym[k++] = (Elf32_Sym){ a, ven[v].off, 0, ELF32_ST_INFO(STB_LOCAL, STT_NOTYPE), 0, 1 };
			st->sym[k++] = (Elf32_Sym){ d, ven[v].off + veneer_size() - 4, 0, ELF32_ST_INFO(STB_LOCAL, STT_NOTYPE), 0, 1 };
			free(nm);
		}
		st->strtab = names.b;
	}
}
static u32 veneer_for(Obj *o, int t, int sidx, s32 A) {   /* a branch's veneer address (0: none) */
	int g = vgroup_of(o, t); if (g < 0) return 0;
	char key[600]; target_key(key, sizeof key, g, o, sidx, A);
	long v = (long)strmap_get(&ven_map, key);
	return v ? vgrp[g].stub->sec_vaddr[1] + ven[v - 1].off : 0;
}

/* GNU ld's __start_SEC / __stop_SEC: referenced, undefined, and SEC an output section named like a C identifier — its
 * start and end (protected visibility, as GNU's -z start-stop-visibility default; not exported). Declared before the
 * relocation scan (they resolve here, never imports), given their values after each layout. */
GSym **startstops; int nstartstop;
static const char *startstop_section(const char *n) {
	const char *s = !strncmp(n, "__start_", 8) ? n + 8 : !strncmp(n, "__stop_", 7) ? n + 7 : NULL;
	if (!s || !*s || (*s >= '0' && *s <= '9')) return NULL;
	for (const char *c = s; *c; c++) if (!(*c == '_' || (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9'))) return NULL;
	return s;
}
static void startstop_declare(void) {
	for (size_t i = 0; i < gsyms.cap; i++) {
		GSym *g = gsyms.vals[i]; const char *s;
		if (!g || g->defined || g->linker || !(s = startstop_section(g->name))) continue;
		int found = 0;
		for (int k = 0; k < nobj && !found; k++) for (int j = 1; j < objs[k]->nsh && !found; j++)
			found = objs[k]->active && objs[k]->sec_out && (objs[k]->sec_out[j] ? objs[k]->sec_out[j] != &os_discard && !strcmp(objs[k]->sec_out[j]->name, s)
			        : (objs[k]->sh[j].sh_flags & SHF_ALLOC) && !strcmp(sec_name(objs[k], j), s));   /* (an orphan: an output section of its name) */
		if (!found) continue;
		g->linker = 1; g->startstop = g->name[2] == 's' && g->name[3] == 't' && g->name[4] == 'a' ? 1 : 2;
		startstops = realloc(startstops, (nstartstop + 1) * sizeof *startstops); startstops[nstartstop++] = g;
	}
}
static void startstop_define(void) {
	for (int i = 0; i < nstartstop; i++) {
		GSym *g = startstops[i]; OutSec *os = outsec_named(startstop_section(g->name));
		if (!os) { g->linker = 0; g->defined = 0; continue; }
		g->defined = 1; g->os = os; g->vaddr = os->vaddr + (g->startstop == 2 ? os->size : 0);
	}
}

/* For each REL section, patch its target section's bytes now that addresses are known. */
static void relocate(void) {
	for (int i = 0; i < nobj; i++) {
		Obj *o = objs[i];
		if (!o->active || o == linker_obj) continue;
		for (int j = 0; j < o->nsh; j++) {
			Elf32_Shdr *rs = &o->sh[j];
			if (rs->sh_type != SHT_REL) continue;
			int t = (int)rs->sh_info;
			Elf32_Shdr *ts = &o->sh[t];                          /* the section being patched */
			if (!(ts->sh_flags & SHF_ALLOC) || o->sec_out[t] == &os_discard) continue;
			if (ts->sh_type == SHT_NOBITS) die("%s: relocations against NOBITS section %s", o->path, sec_name(o, t));
			Elf32_Rel *rel = (Elf32_Rel *)(o->data + rs->sh_offset);
			for (u32 r = 0; r < rs->sh_size / sizeof *rel; r++) {
				u32 type = ELF32_R_TYPE(rel[r].r_info); int sidx = (int)ELF32_R_SYM(rel[r].r_info);
				if (!md_is_marker(type) && (rel[r].r_offset > ts->sh_size || ts->sh_size - rel[r].r_offset < 4))   /* a 4-byte field (no underflow) */
					die("%s: relocation offset %#x outside %s", o->path, rel[r].r_offset, sec_name(o, t));
				if (md_is_marker(type) && !sidx) continue;   /* (R_ARM_V4BX: against no symbol at all) */
				u32 S; int tk = md_tls_kind(type);
				if (tk == TLS_LE) S = tls_tpoff(resolve(o, sidx));                 /* from the thread pointer */
				else if (tk == TLS_LDO) S = resolve(o, sidx) - tls_vaddr;          /* inside this module's block */
				else if (!dyn_target(o, sidx, type, &S)) S = resolve(o, sidx);   /* GOT slot / PLT stub / copy, else the symbol */
				if (nven && branch_reloc(type) && sidx) {   /* out of reach: through its group's veneer */
					u32 dest, Pb; s32 A;
					branch_target(o, t, &rel[r], &dest, &Pb, &A);
					if (!in_reach(dest, Pb)) { u32 v = veneer_for(o, t, sidx, A); if (v) S = v - (u32)A - 8; }   /* the branch lands on v */
				}
				md_apply_reloc(o, type, o->data + ts->sh_offset + rel[r].r_offset, S, o->sec_vaddr[t] + rel[r].r_offset);
			}
		}
	}
}

/* An input file's kind, from its first bytes: an archive (ar magic), a shared object (ELF ET_DYN, used as a
 * provider), else a relocatable object (elf_load validates it). */
enum { IN_OBJECT, IN_ARCHIVE, IN_SHARED };
static int input_kind(const char *path) {
	FILE *f = fopen(path, "rb"); if (!f) die("cannot open %s", path);
	u8 h[18]; size_t n = fread(h, 1, sizeof h, f); fclose(f);
	if (n >= 8 && (!memcmp(h, "!<arch>\n", 8) || !memcmp(h, "!<thin>\n", 8))) return IN_ARCHIVE;   /* (thin: GNU ar T) */
	if (n >= 18 && !memcmp(h, "\177ELF", 4) && (h[16] | h[17] << 8) == ET_DYN) return IN_SHARED;
	return IN_OBJECT;
}
/* -Map FILE: where everything went — each output section, the input sections in it (by address), and the global
 * symbols each defines; then the linker-script symbols. GNU's memory-map layout, without its script echo. */
typedef struct { u32 vaddr; int os, obj, j, seq; const char *name; } MapEnt;
static int map_cmp(const void *x, const void *y) {
	const MapEnt *a = x, *b = y;
	if (a->os != b->os) return a->os < b->os ? -1 : 1;
	if (a->vaddr != b->vaddr) return a->vaddr < b->vaddr ? -1 : 1;
	return a->seq - b->seq;
}
static void write_map(const char *path) {
	FILE *f = fopen(path, "w"); if (!f) die("cannot write %s", path);
	MapEnt *e = NULL; int n = 0, cap = 0;
	for (int i = 0; i < nobj; i++) {
		Obj *o = objs[i]; if (!o->active) continue;
		for (int j = 1; j < o->nsh; j++) {   /* pieces (name == NULL), then the symbols placed in them */
			OutSec *os = o->sec_out[j]; if (!os || os == &os_discard || !os->index) continue;
			e = grow(e, n, &cap, sizeof *e); e[n] = (MapEnt){ o->sec_vaddr[j], os->index, i, j, n, NULL }; n++;
		}
		for (int k = 1; k < o->nsym; k++) {
			Elf32_Sym *s = &o->sym[k]; int b = ELF32_ST_BIND(s->st_info);
			if ((b != STB_GLOBAL && b != STB_WEAK) || !s->st_shndx || s->st_shndx >= o->nsh) continue;   /* ABS, COMMON, …: no input section */
			OutSec *os = o->sec_out[s->st_shndx]; if (!os || os == &os_discard || !os->index) continue;
			GSym *g = gsym_find(o->strtab + s->st_name);
			if (!g || g->obj != o || g->symidx != k) continue;   /* not the definition that won */
			e = grow(e, n, &cap, sizeof *e); e[n] = (MapEnt){ g->vaddr, os->index, i, s->st_shndx, n, g->name }; n++;
		}
	}
	qsort(e, n, sizeof *e, map_cmp);
	fprintf(f, "Memory map\n\n");
	for (int s = 0, k = 0; s < noutsec; s++) {
		OutSec *os = outsecs[s];
		fprintf(f, "%-15s 0x%08x %#10x", os->name, os->vaddr, os->size);
		if (os->lma != os->vaddr) fprintf(f, " load address 0x%08x", os->lma);
		fputc('\n', f);
		for (; k < n && e[k].os == os->index; k++) {
			Obj *o = objs[e[k].obj];
			if (e[k].name) fprintf(f, "                0x%08x                %s\n", e[k].vaddr, e[k].name);
			else fprintf(f, " %-14s 0x%08x %#10x %s\n", sec_name(o, e[k].j), e[k].vaddr, o->sh[e[k].j].sh_size, o->path);
		}
		fputc('\n', f);
	}
	if (ndeclared) fprintf(f, "Linker script symbols\n\n");
	for (int d = 0; d < ndeclared; d++)
		if (declared[d]->defined) fprintf(f, "                0x%08x                %s\n", declared[d]->vaddr, declared[d]->name);
	if (fclose(f)) die("%s: write failed", path);
	free(e);
}

static int file_exists(const char *p) { FILE *f = fopen(p, "rb"); if (f) { fclose(f); return 1; } return 0; }
static int link_static;                                   /* -static: no shared objects at all */
static int verbose;                                       /* --verbose: name each input as it is opened */
static void load_input(const char *path, int kind, int whole) {
	if (verbose) printf("ld: opened %s\n", path);
	if (kind == IN_ARCHIVE && whole) ar_load_whole(path);   /* --whole-archive: every member */
	else if (kind == IN_ARCHIVE) ar_load(path);          /* lazy members, pulled on demand */
	else if (kind == IN_SHARED) {
		if (link_static) die("%s: a shared object in a -static link", path);
		load_shared(path);                               /* a provider: its exports, not its sections */
	} else elf_load(path);                               /* always-linked object */
}
/* -z KEYWORD: the ones with a meaning here; any other is an error (never silently ignored). */
static void z_option(const char *k) {
	if (!strcmp(k, "relro")) norelro = 0;
	else if (!strcmp(k, "norelro")) norelro = 1;
	else if (!strcmp(k, "now")) ;
	else if (!strcmp(k, "defs")) no_undefined = 1;                        /* our loader always binds at startup (eager) */
	else if (!strcmp(k, "combreloc")) combreloc = 1;
	else if (!strcmp(k, "nocombreloc")) combreloc = 0;
	else if (!strcmp(k, "text")) ztext = 1;
	else if (!strcmp(k, "notext") || !strcmp(k, "textoff")) ztext = 0;
	else if (!strcmp(k, "noexecstack")) stack_override = PF_R | PF_W;
	else if (!strcmp(k, "execstack")) stack_override = PF_R | PF_W | PF_X;
	else if (!strncmp(k, "max-page-size=", 14) || !strncmp(k, "common-page-size=", 17)) {
		if (strtoul(strchr(k, '=') + 1, NULL, 0) != PAGE) die("-z %s: this linker lays out %#x-byte pages only", k, PAGE);
	} else die("unsupported -z %s", k);
}

const char **libdirs; int nlibdir;                     /* -L, in order */
int script_ndirs;                                       /* the -L dirs before the -T (a script searches those) */
int main(int argc, char **argv) {
	const char *out = "a.out", *script_path = NULL, *map_path = NULL;
	/* Inputs are processed in command-line order once every option is read (-L applies to every -l, wherever it
	 * appears, as in GNU ld): a file, or -l<name> searched per -L dir as lib<name>.so then lib<name>.a — .a only
	 * under -Bstatic/-static. */
	struct { const char *name; int lib, bstatic, whole; } *in = NULL; int nin = 0, incap = 0;
	int dircap = 0;
	int ttext = 0, bstatic = 0, relocatable = 0, whole = 0, version = 0;
	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		if (!strcmp(a, "-o") && i + 1 < argc) out = argv[++i];
		else if ((!strcmp(a, "-T") || !strcmp(a, "--script")) && i + 1 < argc) script_path = argv[++i], script_ndirs = nlibdir;
		else if (!strncmp(a, "--script=", 9)) script_path = a + 9, script_ndirs = nlibdir;
		else if (!strncmp(a, "-T", 2) && a[2] && strncmp(a, "-Ttext", 6) && strncmp(a, "-Tdata", 6) && strncmp(a, "-Tbss", 5)) script_path = a + 2, script_ndirs = nlibdir;
		else if (!strcmp(a, "-S") || !strcmp(a, "--strip-debug")) ;   /* debug sections never reach our output */
		else if (!strcmp(a, "-s") || !strcmp(a, "--strip-all")) strip_all = 1;
		else if (!strcmp(a, "-Ttext") && i + 1 < argc) { load_base = strtoul(argv[++i], NULL, 0); ttext = 1; }
		else if (!strncmp(a, "-Ttext=", 7)) { load_base = strtoul(a + 7, NULL, 0); ttext = 1; }
		else if ((!strcmp(a, "-e") || !strcmp(a, "--entry")) && i + 1 < argc) entry_sym = argv[++i];
		else if (!strcmp(a, "-pie") || !strcmp(a, "--pie")) { pie = 1; load_base = 0; }
		else if (!strcmp(a, "-shared") || !strcmp(a, "--shared")) { shared = 1; load_base = 0; }
		else if (!strcmp(a, "-Bsymbolic")) bsymbolic = 1;
		else if (!strcmp(a, "-r") || !strcmp(a, "-i") || !strcmp(a, "--relocatable")) relocatable = 1;   /* one object out (reloc.c) */
		else if (!strcmp(a, "--whole-archive")) whole = 1;
		else if (!strcmp(a, "--no-whole-archive")) whole = 0;
		else if (!strcmp(a, "-EL") || !strcmp(a, "-nostdlib")) ;   /* little-endian: the only kind; no default library dirs: we have none */
		else if (!strcmp(a, "-X") || !strcmp(a, "--discard-locals")) ;   /* .L temporaries: our objects' are the ones relocations name */
		else if (!strcmp(a, "-v") || !strcmp(a, "-V") || !strcmp(a, "--version")) version = 1;   /* after the other options parse (ld-option) */
		else if (!strncmp(a, "--orphan-handling=", 18)) {
			const char *m = a + 18;
			orphan_mode = !strcmp(m, "place") ? 0 : !strcmp(m, "discard") ? 1 : !strcmp(m, "warn") ? 2 : !strcmp(m, "error") ? 3
			            : (die("--orphan-handling=%s: place, discard, warn or error", m), 0);
		}
		else if (!strcmp(a, "--no-undefined")) no_undefined = 1;
		else if (!strcmp(a, "--pic-veneer")) pic_veneer = 1;
		else if (!strncmp(a, "--version-script=", 17) || (!strcmp(a, "--version-script") && i + 1 < argc)) version_files[nversion_file++ % 8] = a[16] ? a + 17 : argv[++i];
		else if (!strcmp(a, "--build-id")) build_id_style = "sha1";
		else if (!strncmp(a, "--build-id=", 11)) build_id_style = a + 11;
		else if (!strcmp(a, "--hash-style=sysv")) ;   /* the only one we emit (.hash) */
		else if ((!strcmp(a, "--defsym") && i + 1 < argc) || !strncmp(a, "--defsym=", 9)) {
			const char *d = a[8] ? a + 9 : argv[++i];
			if (!strchr(d, '=')) die("--defsym %s: SYMBOL=EXPRESSION", d);
			defsyms = realloc(defsyms, (ndefsym + 1) * sizeof *defsyms); defsyms[ndefsym++] = d;
		}
		else if (!strcmp(a, "--warn-common")) ;   /* nothing to warn of: a COMMON symbol is an error here (see resolve) */
		else if ((!strcmp(a, "-Map") || !strcmp(a, "--Map")) && i + 1 < argc) map_path = argv[++i];
		else if (!strncmp(a, "-Map=", 5) || !strncmp(a, "--Map=", 6)) map_path = strchr(a, '=') + 1;
		else if (!strcmp(a, "--verbose")) { verbose = 1; printf("ld: the os toolchain's linker (ELF32 ARM, built-in default script)\n"); }
		else if (!strcmp(a, "--no-warn-rwx-segments")) warn_rwx = 0;
		else if (!strcmp(a, "--warn-rwx-segments")) warn_rwx = 1;
		else if (!strcmp(a, "--build-id=none") || !strcmp(a, "--start-group") || !strcmp(a, "--end-group")) ;   /* we emit no build-id;
		                                                     * archives already resolve as one group (see resolve_symbols) */
		else if (!strncmp(a, "--dynamic-linker=", 17)) interp_path = a + 17;
		else if (!strcmp(a, "--dynamic-linker") && i + 1 < argc) interp_path = argv[++i];
		else if (!strcmp(a, "-z") && i + 1 < argc) z_option(argv[++i]);
		else if (!strncmp(a, "-z", 2) && a[2]) z_option(a + 2);
		else if (!strcmp(a, "-soname") && i + 1 < argc) soname = argv[++i];
		else if (!strncmp(a, "-soname=", 8)) soname = a + 8;
		else if (!strcmp(a, "-static")) link_static = bstatic = 1;
		else if (!strcmp(a, "-Bstatic") || !strcmp(a, "-dn") || !strcmp(a, "-non_shared")) bstatic = 1;
		else if (!strcmp(a, "-Bdynamic") || !strcmp(a, "-dy") || !strcmp(a, "-call_shared")) { if (link_static) die("-Bdynamic after -static"); bstatic = 0; }
		else if (!strncmp(a, "-l", 2)) {
			if (!a[2] && i + 1 == argc) die("-l needs a name");
			in = grow(in, nin, &incap, sizeof *in); in[nin].name = a[2] ? a + 2 : argv[++i]; in[nin].lib = 1; in[nin].whole = whole; in[nin++].bstatic = bstatic;
		} else if (!strncmp(a, "-L", 2)) {
			if (!a[2] && i + 1 == argc) die("-L needs a directory");
			libdirs = grow(libdirs, nlibdir, &dircap, sizeof *libdirs); libdirs[nlibdir++] = a[2] ? a + 2 : argv[++i];
		}
		else if (a[0] == '-') die("unknown option '%s'", a);
		else { in = grow(in, nin, &incap, sizeof *in); in[nin].name = a; in[nin].lib = 0; in[nin].whole = whole; in[nin++].bstatic = bstatic; }
	}
	/* -v: GNU ld's first line, as build systems parse it ("GNU ld ... VERSION": the kernel's ld-version.sh); then the
	 * link, if there is anything to link. Any option we don't take failed above (the kernel's ld-option probes). */
	if (version) {
		printf("GNU ld (os toolchain: our ld, GNU ld compatible) 2.42\n");
		if (!nin) return 0;
	}
	for (int k = 0; k < nin; k++) {
		if (!in[k].lib) { load_input(in[k].name, input_kind(in[k].name), in[k].whole); continue; }
		char path[512]; int found = 0;
		for (int d = 0; d < nlibdir && !found; d++)
			for (int t = in[k].bstatic; t < 2 && !found; t++) {   /* lib<name>.so (unless static), then lib<name>.a */
				snprintf(path, sizeof path, "%s/lib%s.%s", libdirs[d], in[k].name, t ? "a" : "so");
				if (file_exists(path)) { load_input(strdup(path), t ? IN_ARCHIVE : IN_SHARED, in[k].whole); found = 1; }
			}
		if (!found) die("cannot find -l%s (searched %d -L dir(s) for lib%s%s.a)", in[k].name, nlibdir, in[k].name, in[k].bstatic ? "" : ".so / lib");
	}
	if (!nobj) die("usage: ld [-o out] [-T script | -Ttext addr] [-e sym] [-pie | -shared [-soname name] [-Bsymbolic]] [-z kw] [--dynamic-linker path] [-L dir] [-l name] obj.o|lib.a ...");
	if (script_path && ttext) die("-Ttext and -T both place the image: use one");
	if (relocatable && (script_path || ttext || pie || shared || entry_sym)) die("-r makes an object: no -T, -Ttext, -pie, -shared or -e");
	if (relocatable && nshlib) die("-r with a shared library: unsupported");
	if (relocatable && (build_id_size() || ndefsym)) die("-r with --build-id or --defsym: unsupported");
	if (relocatable && map_path) die("-Map with -r: unsupported (a relocatable object has no addresses yet)");

	resolve_symbols();
	if (relocatable) { reloc_link(out); return 0; }
	if (shared && !soname) { const char *b = strrchr(out, '/'); soname = b ? b + 1 : out; }

	if (nversion_file > 8) die("more than 8 --version-script files");
	for (int k = 0; k < nversion_file; k++) {             /* (read before the script: a VERSION block adds to them) */
		FILE *vf = fopen(version_files[k], "rb"); if (!vf) die("cannot open version script %s", version_files[k]);
		fseek(vf, 0, SEEK_END); long n = ftell(vf); fseek(vf, 0, SEEK_SET);
		char *t = malloc(n + 1); if (fread(t, 1, n, vf) != (size_t)n) die("%s: read failed", version_files[k]); t[n] = 0; fclose(vf);
		version_script(t, version_files[k]);
	}
	script_read(script_path);                            /* declares the script's symbols */
	if (!entry_sym) entry_sym = "_start";
	dyn_init();
	for (int i = 0; i < nobj; i++) if (objs[i]->active && objs[i] != linker_obj) layout_match(objs[i]);
	startstop_declare();
	dyn_scan();
	dyn_size();
	layout_match(linker_obj);
	layout_run();
	build_globals(); startstop_define();
	veneers();
	startstop_define();
	dyn_fill();
	relocate();

	u32 entry = 0;                                       /* a plain .so has none; ld.so IS a .so WITH an entry */
	GSym *start = gsym_find(entry_sym);
	if (start && start->defined) entry = start->vaddr;
	else if (!shared) die("no '%s' symbol (entry point)", entry_sym);
	elf_write(out, entry);
	if (linker_obj->sh[L_BUILDID].sh_size) {   /* the note's file offset: its output section's, plus its place in it */
		OutSec *os = linker_obj->sec_out[L_BUILDID];
		if (!os || os == &os_discard) die("--build-id: the .note.gnu.build-id section was discarded");
		build_id_write(out, os->off + (linker_obj->sec_vaddr[L_BUILDID] - os->vaddr));
	}
	if (map_path) write_map(map_path);
	return 0;
}
