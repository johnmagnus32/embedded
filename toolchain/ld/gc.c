/*
 * gc.c — --gc-sections: drop the allocated input sections nothing reaches (GNU bfd_elf_gc_sections). It runs before
 * any section is placed (GNU: before map_input_to_output_sections), so /DISCARD/ hasn't applied yet; a swept section
 * simply becomes discarded.
 *
 * ROOTS: the entry symbol's section (-e / ENTRY, else _start), _init's and _fini's (GNU's -init/-fini defaults), -u
 * symbols', every section a KEEP() pattern anywhere in the script names, notes (SHT_NOTE), SHF_GNU_RETAIN sections,
 * and for a .so — or a program linked against one: this linker exports every default-visibility global, like
 * --export-dynamic — the sections defining what it exports. MARKING follows relocations: a marked section keeps the
 * section of every symbol its relocations name (a local's own; a global's winning definition; __start_SEC /
 * __stop_SEC keep every section named SEC), and an SHF_LINK_ORDER section (.ARM.exidx) is marked once what it's
 * linked to is — repeated until nothing changes (unwind entries name personality routines). Unmarked allocated
 * sections are swept. (Non-allocated sections aren't output by this linker, so GNU's rules for them don't arise.)
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ld.h"

int gc_sections;                                         /* --gc-sections */
int print_gc;                                            /* --print-gc-sections */
const char **gc_undef; int ngc_undef;                    /* -u SYM: roots too */
static char **mark;                                      /* [obj][section] */
static struct { int obj, sec; } *wl; static int nwl, wlcap;

static void mark_sec(int oi, int j) {
	if (j <= 0 || j >= objs[oi]->nsh || mark[oi][j]) return;
	mark[oi][j] = 1;
	wl = grow(wl, nwl, &wlcap, sizeof *wl); wl[nwl].obj = oi; wl[nwl].sec = j; nwl++;
}
static int obj_index(const Obj *o) { for (int i = 0; i < nobj; i++) if (objs[i] == o) return i; die("internal: gc: unknown object %s", o->path); }
static void mark_def(const GSym *g) {                    /* a global's defining section */
	if (!g || !g->defined || !g->obj) return;
	const Elf32_Sym *s = &g->obj->sym[g->symidx];
	if (s->st_shndx != SHN_UNDEF && s->st_shndx < 0xff00 /* SHN_LORESERVE */) mark_sec(obj_index(g->obj), s->st_shndx);
}
static void mark_named(const char *name) {               /* __start_/__stop_: every allocated section of that name */
	for (int i = 0; i < nobj; i++) {
		Obj *o = objs[i];
		if (!o->active || o == linker_obj) continue;
		for (int j = 1; j < o->nsh; j++) if ((o->sh[j].sh_flags & SHF_ALLOC) && !strcmp(sec_name(o, j), name)) mark_sec(i, j);
	}
}
static void follow(int oi, int t) {                      /* what (objs[oi], t)'s relocations name */
	Obj *o = objs[oi];
	for (int r = 1; r < o->nsh; r++) {
		const Elf32_Shdr *rs = &o->sh[r];
		if (rs->sh_type != SHT_REL || (int)rs->sh_info != t) continue;
		const Elf32_Rel *rel = (const Elf32_Rel *)(o->data + rs->sh_offset);
		for (u32 k = 0; k < rs->sh_size / sizeof *rel; k++) {
			u32 si = ELF32_R_SYM(rel[k].r_info);
			if (!si || si >= (u32)o->nsym) continue;
			const Elf32_Sym *s = &o->sym[si];
			if (ELF32_ST_BIND(s->st_info) == STB_LOCAL) {
				if (s->st_shndx != SHN_UNDEF && s->st_shndx < 0xff00 /* SHN_LORESERVE */) mark_sec(oi, s->st_shndx);
				continue;
			}
			const char *nm = o->strtab + s->st_name; GSym *g = gsym_find(nm);
			if (g && g->defined && g->obj) mark_def(g);
			else if (startstop_section(nm)) mark_named(startstop_section(nm));
		}
	}
}

void gc_run(void) {
	mark = calloc(nobj, sizeof *mark);
	for (int i = 0; i < nobj; i++) mark[i] = calloc(objs[i]->nsh + 1, 1);
	mark_def(gsym_find(entry_sym ? entry_sym : "_start"));
	mark_def(gsym_find("_init")); mark_def(gsym_find("_fini"));
	for (int k = 0; k < ngc_undef; k++) mark_def(gsym_find(gc_undef[k]));
	int exports = shared || export_dynamic || nshlib;
	for (int i = 0; i < nobj; i++) {
		Obj *o = objs[i];
		if (!o->active) continue;
		for (int j = 1; j < o->nsh; j++) {
			const Elf32_Shdr *h = &o->sh[j];
			if (h->sh_type == 17 /* SHT_GROUP */) die("%s: section group %s (COMDAT) with --gc-sections: unsupported", o->path, sec_name(o, j));
			if (!strcmp(sec_name(o, j), ".eh_frame") && (h->sh_flags & SHF_ALLOC))
				die("%s: .eh_frame with --gc-sections: unsupported (GNU keeps its FDEs per function)", o->path);
			if (o == linker_obj || h->sh_type == 7 /* SHT_NOTE */ || (h->sh_flags & 0x200000u /* SHF_GNU_RETAIN */) || script_keeps(o, j))
				mark_sec(i, j);
		}
		if (!exports || o == linker_obj) continue;
		for (int k = 1; k < o->nsym; k++) {               /* the exported interface (dynamic.c collect_exports' rule) */
			const Elf32_Sym *s = &o->sym[k]; int b = ELF32_ST_BIND(s->st_info);
			if ((b != STB_GLOBAL && b != STB_WEAK) || s->st_shndx == SHN_UNDEF || s->st_shndx >= 0xff00 /* SHN_LORESERVE */ || !s->st_name) continue;
			if (ELF32_ST_VISIBILITY(s->st_other) != STV_DEFAULT || version_hidden(o->strtab + s->st_name) || !dyn_exports(o->strtab + s->st_name)) continue;
			const GSym *g = gsym_find(o->strtab + s->st_name);
			if (g && g->obj == o && g->symidx == k) mark_sec(i, s->st_shndx);
		}
	}
	for (;;) {
		while (nwl) { nwl--; follow(wl[nwl].obj, wl[nwl].sec); }
		int more = 0;                                    /* linked-to sections: .ARM.exidx follows its code */
		for (int i = 0; i < nobj; i++) {
			Obj *o = objs[i];
			if (!o->active) continue;
			for (int j = 1; j < o->nsh; j++) {
				u32 l = o->sh[j].sh_link;
				if (!mark[i][j] && (o->sh[j].sh_flags & SHF_LINK_ORDER) && l && l < (u32)o->nsh && mark[i][l]) { mark_sec(i, j); more = 1; }
			}
		}
		if (!more) break;
	}
	for (int i = 0; i < nobj; i++) {                     /* sweep */
		Obj *o = objs[i];
		if (!o->active || o == linker_obj) continue;
		for (int j = 1; j < o->nsh; j++) {
			if (mark[i][j] || !(o->sh[j].sh_flags & SHF_ALLOC) || o->sec_out[j]) continue;
			if (print_gc && o->sh[j].sh_size) fprintf(stderr, "ld: removing unused section '%s' in file '%s'\n", sec_name(o, j), o->path);
			o->sec_out[j] = &os_discard;
		}
	}
}
