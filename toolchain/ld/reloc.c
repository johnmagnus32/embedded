/*
 * reloc.c — a RELOCATABLE link (ld -r): many objects in, ONE object out (ET_REL), which a later link finishes. The
 * kernel builds vmlinux.o this way (one object for modpost and the final link); BusyBox each directory's built-in.o.
 *
 * Nothing gets an address. What it does instead:
 *   sections     same-named input sections concatenate into one output section, each piece at its alignment
 *                (code/data in object order — the order a final link would have placed them);
 *   symbols      a section symbol per output section; each object's locals (its statics stay apart), moved by
 *                where their section landed; one entry per global — the definition that won, or one undefined
 *                reference for all users (weak if none of them is strong);
 *   relocations  carried, not applied: offsets moved with their section, symbols renumbered; one against an input
 *                SECTION symbol becomes one against the output section's, its addend — kept in the patched field
 *                on ARM (REL) — grown by the input section's offset (md_rel_adjust).
 *   files        GNU's: an input with local symbols but no STT_FILE of its own gets one, its file's basename, before
 *                its first local (so the locals that follow aren't taken for the previous file's);
 *   mapping      GNU's: an allocated data piece with no mapping symbols at all gets `$d` at its start (a merged
 *                section would otherwise say nothing about what that stretch holds);
 *   unwinding    GNU's EXIDX coverage fix: a text piece without unwind entries, after one with them, must not be
 *                covered by that one's last entry — an EXIDX_CANTUNWIND entry is appended to the earlier piece's
 *                table (exidx_coverage). Done here because after merging, the gap can't be seen any more.
 *   order        GNU's: its default -r script's sections in script order, each "orphan" (a name the script doesn't
 *                list) after the script section of its kind, in first-seen order (sec_order) — so a final link that
 *                takes several sections of this one object in a wildcard (`*(.ARM.exidx*)`) lays them out as from GNU's.
 * Out of scope (an error, never silently mishandled): COMDAT groups, RELA, a linker script, COMMON symbols.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "ld.h"
#include "armattrs.h"

#define SHF_INFO_LINK_ 0x40u
#define SHF_LINK_ORDER_ 0x80u
#define SHT_ARM_EXIDX 0x70000001u

typedef struct {
	const char *name; u32 type, flags, entsize, align, size;
	int link;                     /* SHF_LINK_ORDER (.ARM.exidx*): the output section its pieces describe (-1: none) */
	int index;                    /* its section-header index */
	Elf32_Rel *rel; int nrel, relcap;   /* the relocations against it, renumbered */
} RSec;
static RSec *rsec; static int nrsec, rseccap;
static StrMap rsec_by_name;
static int **rmap;                /* [obj][input shndx] -> rsec index (-1: not carried) */
static int **smap;                /* [obj][input symidx] -> output symbol index (-1: none) */

/* The output section an input section's name goes to: GNU's -r script folds a few names into another. */
static const char *out_name(const char *name) {
	if (!strcmp(name, ".stub") || !strcmp(name, ".gnu.warning")) return ".text";
	if (!strncmp(name, ".gnu_extab", 10)) return ".gnu_extab";
	return name;
}
static int rsec_get(const char *name) {
	name = out_name(name);
	void *v = strmap_get(&rsec_by_name, name);
	if (v) return (int)(long)v - 1;
	rsec = grow(rsec, nrsec, &rseccap, sizeof *rsec);
	rsec[nrsec] = (RSec){ .name = name, .align = 1, .link = -1 };
	strmap_put(&rsec_by_name, name, (void *)(long)(nrsec + 1));
	return nrsec++;
}

/* Which input sections are carried: the content ones — not the tables we rebuild (symtab, strings, REL), and the
 * build attributes once (a merged object has one set; the inputs of one build agree). */
static int carried(const Obj *o, int j, int *attrs_seen) {
	u32 t = o->sh[j].sh_type;
	if (t == 0 /* SHT_NULL */ || t == SHT_SYMTAB || t == SHT_STRTAB || t == SHT_REL) return 0;
	if (t == 4 /* SHT_RELA */) die("%s: RELA section %s (ARM objects use REL)", o->path, sec_name(o, j));
	if (t == 17 /* SHT_GROUP */) die("%s: section group %s (COMDAT) in a relocatable link: unsupported", o->path, sec_name(o, j));
	if (t == 18 /* SHT_SYMTAB_SHNDX */) die("%s: extended section indices: unsupported", o->path);
	if (t == SHT_ARM_ATTRIBUTES) { if (*attrs_seen) return 0; *attrs_seen = 1; }
	return 1;
}

static Elf32_Sym *osym; static int nosym, osymcap; static Strtab ostr;
static int add_osym(const char *name, u32 value, u32 size, u8 info, u8 other, u16 shndx) {
	osym = grow(osym, nosym, &osymcap, sizeof *osym);
	osym[nosym] = (Elf32_Sym){ .st_name = name && *name ? str_add(&ostr, name) : 0, .st_value = value, .st_size = size,
	                            .st_info = info, .st_other = other, .st_shndx = shndx };
	return nosym++;
}
/* A symbol's place in the output: its section's output index + the moved value (absolute ones stay). */
static int sym_place(const Obj *o, int oi, const Elf32_Sym *s, u32 *value, u16 *shndx) {
	if (s->st_shndx == SHN_ABS) { *value = s->st_value; *shndx = SHN_ABS; return 1; }
	if (s->st_shndx == SHN_UNDEF || s->st_shndx >= 0xff00 || s->st_shndx >= o->nsh) return 0;
	int r = rmap[oi][s->st_shndx]; if (r < 0) return 0;
	*value = s->st_value + o->sec_vaddr[s->st_shndx]; *shndx = (u16)rsec[r].index; return 1;
}

/* GNU ld's EXIDX coverage fix (bfd elf32_arm_fix_exidx_coverage, as ld runs it for -r): every executable PROGBITS
 * input section, stably sorted by output OFFSET — in a relocatable link every output section is at 0, so the order
 * interleaves .text/.init.text/... pieces; GNU's, reproduced — walked with the last unwind entry's kind
 * (0 = CANTUNWIND, 1 = inline codes, 2 = table). A piece with no table after one whose last entry unwinds, or a
 * table whose first entry isn't at its piece's start, gets one EXIDX_CANTUNWIND appended to the PREVIOUS piece's
 * table, covering from that piece's end. (No terminating entry, no merging: those are for a final link.) */
typedef struct { int obj, sec; u32 off; int seq; } TextPiece;
static int by_offset(const void *a, const void *b) {
	const TextPiece *x = a, *y = b;
	return x->off != y->off ? (x->off < y->off ? -1 : 1) : x->seq - y->seq;   /* (stable: ties keep input order) */
}
static int **cantunwind;          /* [obj][exidx shndx] -> EXIDX_CANTUNWIND entries appended to it (0 or 1) */
static int exidx_of(const Obj *o, int text) {
	for (int j = 1; j < o->nsh; j++)
		if (o->sh[j].sh_type == SHT_ARM_EXIDX && (o->sh[j].sh_flags & SHF_LINK_ORDER_) && (int)o->sh[j].sh_link == text) return j;
	return 0;
}
static int exidx_coverage(void) {
	TextPiece *tp = NULL; int ntp = 0, tpcap = 0, added = 0;
	for (int i = 0; i < nobj; i++) for (int j = 1; j < objs[i]->nsh; j++) {
		Elf32_Shdr *sh = &objs[i]->sh[j];
		if (rmap[i][j] < 0 || sh->sh_type != SHT_PROGBITS || !(sh->sh_flags & SHF_EXECINSTR)) continue;
		tp = grow(tp, ntp, &tpcap, sizeof *tp); tp[ntp] = (TextPiece){ i, j, objs[i]->sec_vaddr[j], ntp }; ntp++;
	}
	qsort(tp, ntp, sizeof *tp, by_offset);
	int last_type = 0, last_obj = -1, last_text = 0, last_exidx = 0;
	#define INSERT() do { if (!cantunwind[last_obj][last_exidx]++) added++; else die("internal: two EXIDX_CANTUNWIND insertions in one table"); last_type = 0; } while (0)
	for (int k = 0; k < ntp; k++) {
		Obj *o = objs[tp[k].obj]; int x = exidx_of(o, tp[k].sec);
		if (!x || rmap[tp[k].obj][x] < 0) {   /* no unwind entries here */
			if (last_type == 0 || last_obj < 0 || o->sh[tp[k].sec].sh_size == 0) continue;
			INSERT(); continue;
		}
		Elf32_Shdr *xs = &o->sh[x]; const u8 *c = o->data + xs->sh_offset;
		if (last_type > 0 && xs->sh_size >= 4 && rd32(c) != 0) INSERT();   /* (the input section's own address is 0) */
		for (u32 e = 0; e + 8 <= xs->sh_size; e += 8) {
			u32 w = rd32(c + e + 4);
			last_type = w == 1 ? 0 : (w & 0x80000000u) ? 1 : 2;
		}
		last_obj = tp[k].obj; last_text = tp[k].sec; last_exidx = x;
	}
	#undef INSERT
	(void)last_text; free(tp);
	return added;
}
/* Same-named carried input sections -> output sections, each piece's offset (kept in sec_vaddr); extra bytes after
 * a piece: the CANTUNWIND entries appended to it. */
static void layout_pieces(u32 *e_flags) {
	int attrs_seen = 0, have_flags = 0;
	for (int r = 0; r < nrsec; r++) { rsec[r].size = 0; rsec[r].type = 0; rsec[r].flags = 0; rsec[r].align = 1; }
	for (int i = 0; i < nobj; i++) {
		Obj *o = objs[i];
		if (!o->active) continue;
		if (!have_flags) { *e_flags = o->eh->e_flags; have_flags = 1; }
		for (int j = 1; j < o->nsh; j++) {
			if (!carried(o, j, &attrs_seen)) continue;
			Elf32_Shdr *sh = &o->sh[j];
			int r = rsec_get(sec_name(o, j)); RSec *R = &rsec[r];
			u32 al = sh->sh_addralign ? sh->sh_addralign : 1;
			if (R->size == 0 && R->type == 0) { R->type = sh->sh_type; R->entsize = sh->sh_entsize; }
			else {
				if (R->type == SHT_NOBITS && sh->sh_type != SHT_NOBITS) R->type = sh->sh_type;   /* (zeros in a PROGBITS: fine) */
				if (R->entsize != sh->sh_entsize) { R->entsize = 0; R->flags &= ~(SHF_MERGE | SHF_STRINGS); sh->sh_flags &= ~(SHF_MERGE | SHF_STRINGS); }
			}
			R->flags |= sh->sh_flags;
			if (al > R->align) R->align = al;
			R->size = alignup(R->size, al); o->sec_vaddr[j] = R->size; R->size += sh->sh_size + 8u * (u32)cantunwind[i][j];
			rmap[i][j] = r;
		}
	}
}

static const char *file_basename(const char *path) {   /* "dir/x.o" -> "x.o"; "lib.a(x.o)" -> "x.o" */
	size_t n = strlen(path);
	if (n && path[n - 1] == ')') { const char *l = strrchr(path, '('); if (l) { char *m = strndup(l + 1, (size_t)(path + n - 1 - (l + 1))); const char *b = strrchr(m, '/'); return b ? b + 1 : m; } }
	const char *b = strrchr(path, '/'); return b ? b + 1 : path;
}
/* GNU ld's default relocatable script (ld --verbose -r), its output sections in order — the ones with no input vanish. */
static const char *const r_script[] = { ".interp", ".note.gnu.build-id", ".hash", ".gnu.hash", ".dynsym", ".dynstr", ".gnu.version",
	".gnu.version_d", ".gnu.version_r", ".init", ".plt", ".iplt", ".text", ".fini", ".rodata", ".rodata1", ".ARM.extab", ".ARM.exidx",
	".eh_frame_hdr", ".eh_frame", ".sframe", ".gcc_except_table", ".gnu_extab", ".exception_ranges", ".tdata", ".tbss", ".jcr", ".dynamic",
	".got", ".data", ".data1", ".bss", ".stab", ".stabstr", ".stab.excl", ".stab.exclstr", ".stab.index", ".stab.indexstr", ".comment",
	".gnu.build.attributes", ".debug", ".line", ".debug_srcinfo", ".debug_sfnames", ".debug_aranges", ".debug_pubnames", ".debug_info",
	".debug_abbrev", ".debug_line", ".debug_frame", ".debug_str", ".debug_loc", ".debug_macinfo", ".debug_weaknames", ".debug_funcnames",
	".debug_typenames", ".debug_varnames", ".debug_pubtypes", ".debug_ranges", ".debug_addr", ".debug_line_str", ".debug_loclists",
	".debug_macro", ".debug_names", ".debug_rnglists", ".debug_str_offsets", ".debug_sup", ".gnu.attributes", ".note.gnu.arm.ident", NULL };
static int script_pos(const char *name) { for (int i = 0; r_script[i]; i++) if (!strcmp(r_script[i], name)) return i; return -1; }
/* An orphan's place, by its first input section's kind (GNU ldelf_place_orphan): the script section it follows. */
static const char *orphan_anchor(const Elf32_Shdr *sh) {
	u32 f = sh->sh_flags;
	if (!(f & SHF_ALLOC)) return ".comment";
	if (sh->sh_type == 7 /* SHT_NOTE */) return ".interp";
	if (f & SHF_TLS) return ".tdata";
	if (sh->sh_type == SHT_NOBITS) return ".bss";
	if (f & SHF_WRITE) return ".data";
	if (!(f & SHF_EXECINSTR)) return ".rodata";
	return ".text";
}
/* Section-header order: each script section that has input, followed by the orphans anchored to it. */
static void sec_order(void) {
	int nscript = 0; while (r_script[nscript]) nscript++;
	int **after = calloc(nscript, sizeof *after), *nafter = calloc(nscript, sizeof *nafter);
	char *placed = calloc(nrsec ? nrsec : 1, 1);
	for (int i = 0; i < nobj; i++) for (int j = 1; j < objs[i]->nsh; j++) {   /* orphans: first seen, first placed */
		int r = rmap[i][j]; if (r < 0 || placed[r]) continue;
		placed[r] = 1;
		if (script_pos(rsec[r].name) >= 0) continue;
		int a = script_pos(orphan_anchor(&objs[i]->sh[j]));
		after[a] = realloc(after[a], (nafter[a] + 1) * sizeof **after); after[a][nafter[a]++] = r;
	}
	int idx = 1, attrs = -1;
	for (int p = 0; p < nscript; p++) {
		void *v = strmap_get(&rsec_by_name, r_script[p]);
		if (v) rsec[(int)(long)v - 1].index = idx++;
		for (int k = 0; k < nafter[p]; k++) {
			if (rsec[after[p][k]].type == SHT_ARM_ATTRIBUTES) { attrs = after[p][k]; continue; }   /* (GNU's backend writes it: last) */
			rsec[after[p][k]].index = idx++;
		}
	}
	if (attrs >= 0) rsec[attrs].index = idx++;
	if (idx != 1 + nrsec) die("internal: section order covers %d of %d sections", idx - 1, nrsec);
	for (int p = 0; p < nscript; p++) free(after[p]);
	free(after); free(nafter); free(placed);
}

void reloc_link(const char *out) {
	/* 1. sections: the output sections, each piece's offset in its output section (kept in sec_vaddr) */
	rmap = calloc(nobj, sizeof *rmap); smap = calloc(nobj, sizeof *smap); cantunwind = calloc(nobj, sizeof *cantunwind);
	u32 e_flags = 0;
	for (int i = 0; i < nobj; i++) {
		Obj *o = objs[i];
		rmap[i] = malloc(o->nsh * sizeof **rmap); for (int j = 0; j < o->nsh; j++) rmap[i][j] = -1;
		cantunwind[i] = calloc(o->nsh, sizeof **cantunwind);
	}
	layout_pieces(&e_flags);
	if (exidx_coverage()) layout_pieces(&e_flags);   /* (the tables grew: their pieces move; text pieces don't) */
	sec_order();   /* content sections first: [1, nrsec], in GNU's order */
	/* SHF_LINK_ORDER: every piece's linked section in ONE output section (the exidx of .text describes .text) */
	for (int i = 0; i < nobj; i++) for (int j = 1; j < objs[i]->nsh; j++) {
		int r = rmap[i][j]; Elf32_Shdr *sh = &objs[i]->sh[j];
		if (r < 0 || !(sh->sh_flags & SHF_LINK_ORDER_)) continue;
		int l = sh->sh_link && sh->sh_link < (u32)objs[i]->nsh ? rmap[i][sh->sh_link] : -1;
		if (l < 0) die("%s: %s is ordered by a section that isn't carried", objs[i]->path, sec_name(objs[i], j));
		if (rsec[r].link >= 0 && rsec[r].link != l) die("%s: %s describes %s, but another input's %s describes %s", objs[i]->path,
		    rsec[r].name, rsec[l].name, rsec[r].name, rsec[rsec[r].link].name);
		rsec[r].link = l;
	}

	/* A relocation against a piece's section symbol takes the piece's offset into its addend; where that field can't
	 * hold it (a branch's 24 bits, past 32MB into a huge merged .text), the relocation goes against an ANCHOR instead —
	 * a local symbol at the piece's start — with its addend untouched: the same final address. (GNU's arm_add_to_rel
	 * truncates the addend there, unchecked.) */
	int **anchor = calloc(nobj, sizeof *anchor), nanchor = 0;
	for (int i = 0; i < nobj; i++) {
		Obj *o = objs[i]; if (!o->active) continue;
		for (int j = 1; j < o->nsh; j++) {
			Elf32_Shdr *rs = &o->sh[j];
			if (rs->sh_type != SHT_REL || !rs->sh_info || rs->sh_info >= (u32)o->nsh || rmap[i][rs->sh_info] < 0) continue;
			Elf32_Shdr *ts = &o->sh[rs->sh_info]; Elf32_Rel *rel = (Elf32_Rel *)(o->data + rs->sh_offset);
			for (u32 n = 0; n < rs->sh_size / sizeof *rel; n++) {
				int sidx = (int)ELF32_R_SYM(rel[n].r_info); if (!sidx || sidx >= o->nsym) continue;
				Elf32_Sym *s = &o->sym[sidx]; u8 w[4];
				if (ELF32_ST_TYPE(s->st_info) != STT_SECTION || s->st_shndx >= o->nsh || !o->sec_vaddr[s->st_shndx]) continue;
				if (ts->sh_type == SHT_NOBITS || rel[n].r_offset + 4 > ts->sh_size) continue;   /* (step 3 reports it) */
				memcpy(w, o->data + ts->sh_offset + rel[n].r_offset, 4);
				if (md_rel_adjust(ELF32_R_TYPE(rel[n].r_info), w, o->sec_vaddr[s->st_shndx])) continue;
				if (!anchor[i]) anchor[i] = calloc(o->nsh, sizeof **anchor);
				anchor[i][s->st_shndx] = -1;   /* wanted (its symbol index comes with the locals) */
			}
		}
	}

	/* 2. symbols: null, section symbols, each object's locals, then the globals */
	str_add(&ostr, ""); add_osym(NULL, 0, 0, 0, 0, SHN_UNDEF);
	int *secsym = malloc((nrsec ? nrsec : 1) * sizeof *secsym);
	for (int r = 0; r < nrsec; r++) secsym[r] = add_osym(NULL, 0, 0, ELF32_ST_INFO(STB_LOCAL, STT_SECTION), 0, (u16)rsec[r].index);
	for (int i = 0; i < nobj; i++) {
		Obj *o = objs[i];
		smap[i] = malloc((o->nsym ? o->nsym : 1) * sizeof **smap); for (int k = 0; k < o->nsym; k++) smap[i][k] = -1;
		if (!o->active) continue;
		int have_file = 0;
		for (int k = 1; k < o->nsym; k++) if (ELF32_ST_TYPE(o->sym[k].st_info) == STT_FILE) have_file = 1;
		for (int k = 1; k < o->nsym; k++) {
			Elf32_Sym *s = &o->sym[k];
			if (ELF32_ST_BIND(s->st_info) != STB_LOCAL) continue;
			int t = ELF32_ST_TYPE(s->st_info);
			if (!have_file && t != STT_SECTION) { add_osym(file_basename(o->path), 0, 0, ELF32_ST_INFO(STB_LOCAL, STT_FILE), 0, SHN_ABS); have_file = 1; }
			if (t == STT_SECTION) { int r = s->st_shndx < o->nsh ? rmap[i][s->st_shndx] : -1; if (r >= 0) smap[i][k] = secsym[r]; continue; }
			if (t == STT_FILE) { smap[i][k] = add_osym(o->strtab + s->st_name, 0, 0, s->st_info, 0, SHN_ABS); continue; }
			u32 v; u16 sx;
			if (sym_place(o, i, s, &v, &sx)) smap[i][k] = add_osym(s->st_name ? o->strtab + s->st_name : NULL, v, s->st_size, s->st_info, s->st_other, sx);
		}
		for (int j = 1; anchor[i] && j < o->nsh; j++) if (anchor[i][j]) {   /* this object's anchors (.L names: kallsyms skips them) */
			char nm[48]; snprintf(nm, sizeof nm, ".L.ld-r.anchor.%d", nanchor++);
			anchor[i][j] = add_osym(strdup(nm), o->sec_vaddr[j], 0, ELF32_ST_INFO(STB_LOCAL, STT_NOTYPE), 0, (u16)rsec[rmap[i][j]].index);
		}
		char *mapped = calloc(o->nsh, 1);   /* the pieces that have mapping symbols ($a $t $d, or $x.name) */
		for (int k = 1; k < o->nsym; k++) {
			const char *n = o->sym[k].st_name ? o->strtab + o->sym[k].st_name : "";
			if (n[0] == '$' && strchr("atd", n[1]) && (!n[2] || n[2] == '.') && o->sym[k].st_shndx < o->nsh) mapped[o->sym[k].st_shndx] = 1;
		}
		for (int j = 1; j < o->nsh; j++) {
			Elf32_Shdr *ps = &o->sh[j];
			if (rmap[i][j] < 0 || mapped[j] || !ps->sh_size || !(ps->sh_flags & SHF_ALLOC) || (ps->sh_flags & SHF_EXECINSTR)) continue;
			add_osym("$d", o->sec_vaddr[j], 0, ELF32_ST_INFO(STB_LOCAL, STT_NOTYPE), 0, (u16)rsec[rmap[i][j]].index);
		}
		free(mapped);
	}
	int first_global = nosym;
	StrMap gidx = {0};
	for (int i = 0; i < nobj; i++) {   /* the definitions that won, in command-line order */
		Obj *o = objs[i]; if (!o->active) continue;
		for (int k = 1; k < o->nsym; k++) {
			Elf32_Sym *s = &o->sym[k]; int b = ELF32_ST_BIND(s->st_info);
			if (b == STB_LOCAL || s->st_shndx == SHN_UNDEF || !s->st_name) continue;
			const char *nm = o->strtab + s->st_name; GSym *g = gsym_find(nm);
			if (!g || g->obj != o || g->symidx != k) continue;
			u32 v; u16 sx;
			if (!sym_place(o, i, s, &v, &sx)) die("%s: '%s' is defined in a section that isn't carried", o->path, nm);
			strmap_put(&gidx, nm, (void *)(long)(add_osym(nm, v, s->st_size, s->st_info, s->st_other, sx) + 1));
		}
	}
	for (int i = 0; i < nobj; i++) {   /* every other mention of a global: the winner, else one undefined entry */
		Obj *o = objs[i]; if (!o->active) continue;
		for (int k = 1; k < o->nsym; k++) {
			Elf32_Sym *s = &o->sym[k]; int b = ELF32_ST_BIND(s->st_info);
			if (b == STB_LOCAL || !s->st_name) continue;
			const char *nm = o->strtab + s->st_name;
			long x = (long)strmap_get(&gidx, nm);
			if (!x) {   /* referenced, defined nowhere: undefined in the output too (strong if any reference is) */
				GSym *g = gsym_find(nm);
				x = add_osym(nm, 0, 0, ELF32_ST_INFO(g && g->strong_ref ? STB_GLOBAL : STB_WEAK, ELF32_ST_TYPE(s->st_info)), s->st_other, SHN_UNDEF) + 1;
				strmap_put(&gidx, nm, (void *)x);
			}
			smap[i][k] = (int)x - 1;
		}
	}

	/* 3. relocations: carried into the output section's REL table, offsets and symbols renumbered */
	for (int i = 0; i < nobj; i++) {
		Obj *o = objs[i]; if (!o->active) continue;
		for (int j = 1; j < o->nsh; j++) {
			Elf32_Shdr *rs = &o->sh[j];
			if (rs->sh_type != SHT_REL) continue;
			int t = (int)rs->sh_info, r = t > 0 && t < o->nsh ? rmap[i][t] : -1;
			if (r < 0) die("%s: relocations for %s, which isn't carried", o->path, rs->sh_info < (u32)o->nsh ? sec_name(o, t) : "?");
			Elf32_Shdr *ts = &o->sh[t];
			Elf32_Rel *rel = (Elf32_Rel *)(o->data + rs->sh_offset);
			for (u32 n = 0; n < rs->sh_size / sizeof *rel; n++) {
				u32 type = ELF32_R_TYPE(rel[n].r_info); int sidx = (int)ELF32_R_SYM(rel[n].r_info);
				if (sidx >= o->nsym || (sidx && smap[i][sidx] < 0)) die("%s: a relocation in %s refers to a symbol that isn't carried", o->path, sec_name(o, t));
				Elf32_Sym *s = &o->sym[sidx]; int osym = sidx ? smap[i][sidx] : 0;   /* (0: none — R_ARM_V4BX) */
				u8 w[4]; int anchored = 0;
				if (sidx && ELF32_ST_TYPE(s->st_info) == STT_SECTION && anchor[i] && s->st_shndx < o->nsh && anchor[i][s->st_shndx] > 0
				    && rel[n].r_offset + 4 <= ts->sh_size && ts->sh_type != SHT_NOBITS) {   /* this one's addend can't move: its anchor */
					memcpy(w, o->data + ts->sh_offset + rel[n].r_offset, 4);
					if (!md_rel_adjust(type, w, o->sec_vaddr[s->st_shndx])) { osym = anchor[i][s->st_shndx]; anchored = 1; }
				}
				if (!anchored && sidx && ELF32_ST_TYPE(s->st_info) == STT_SECTION && o->sec_vaddr[s->st_shndx]) {   /* move the addend */
					if (ts->sh_type == SHT_NOBITS || rel[n].r_offset + 4 > ts->sh_size) die("%s: relocation offset %#x outside %s", o->path, rel[n].r_offset, sec_name(o, t));
					if (!md_rel_adjust(type, o->data + ts->sh_offset + rel[n].r_offset, o->sec_vaddr[s->st_shndx]))
						die("%s: relocation type %u against section %s in %s: its addend can't be moved in a relocatable link",
						    o->path, type, sec_name(o, s->st_shndx), sec_name(o, t));
				}
				RSec *R = &rsec[r];
				R->rel = grow(R->rel, R->nrel, &R->relcap, sizeof *R->rel);
				R->rel[R->nrel++] = (Elf32_Rel){ rel[n].r_offset + o->sec_vaddr[t], ELF32_R_INFO((u32)osym, type) };
			}
		}
	}

	/* the inserted EXIDX_CANTUNWIND entries: {prel31 -> the end of the table's text piece, 1} */
	for (int i = 0; i < nobj; i++) {
		Obj *o = objs[i];
		for (int j = 1; j < o->nsh; j++) {
			if (!cantunwind[i][j]) continue;
			int t = (int)o->sh[j].sh_link, r = rmap[i][j];
			RSec *R = &rsec[r];
			R->rel = grow(R->rel, R->nrel, &R->relcap, sizeof *R->rel);
			R->rel[R->nrel++] = (Elf32_Rel){ o->sec_vaddr[j] + o->sh[j].sh_size, ELF32_R_INFO((u32)secsym[rmap[i][t]], 42 /* R_ARM_PREL31 */) };
		}
	}
	/* 4. the file: header | contents | REL tables | .symtab .strtab .shstrtab | section headers */
	int nrelsec = 0; for (int r = 0; r < nrsec; r++) if (rsec[r].nrel) nrelsec++;
	int isym = 1 + nrsec + nrelsec, nsh = isym + 3;
	Elf32_Shdr *sh = calloc(nsh, sizeof *sh);
	Strtab shs = {0}; str_add(&shs, "");
	u32 off = sizeof(Elf32_Ehdr);
	int *byidx = malloc((nrsec + 1) * sizeof *byidx);
	for (int r = 0; r < nrsec; r++) byidx[rsec[r].index] = r;
	for (int q = 1; q <= nrsec; q++) {
		RSec *R = &rsec[byidx[q]];
		if (R->type != SHT_NOBITS) off = alignup(off, R->align);
		sh[R->index] = (Elf32_Shdr){ .sh_name = str_add(&shs, R->name), .sh_type = R->type, .sh_flags = R->flags, .sh_offset = off,
		    .sh_size = R->size, .sh_link = R->link >= 0 ? (u32)rsec[R->link].index : 0, .sh_addralign = R->align, .sh_entsize = R->entsize };
		if (R->type != SHT_NOBITS) off += R->size;
	}
	for (int q = 1, x = 1 + nrsec; q <= nrsec; q++) {
		RSec *R = &rsec[byidx[q]]; if (!R->nrel) continue;
		char *nm = malloc(strlen(R->name) + 5); sprintf(nm, ".rel%s", R->name);
		off = alignup(off, 4);
		sh[x++] = (Elf32_Shdr){ .sh_name = str_add(&shs, nm), .sh_type = SHT_REL, .sh_flags = SHF_INFO_LINK_, .sh_offset = off,
		    .sh_size = (u32)R->nrel * sizeof(Elf32_Rel), .sh_link = (u32)isym, .sh_info = (u32)R->index, .sh_addralign = 4, .sh_entsize = sizeof(Elf32_Rel) };
		off += sh[x - 1].sh_size;
	}
	off = alignup(off, 4);
	sh[isym] = (Elf32_Shdr){ .sh_name = str_add(&shs, ".symtab"), .sh_type = SHT_SYMTAB, .sh_offset = off, .sh_size = (u32)nosym * sizeof(Elf32_Sym),
	    .sh_link = (u32)isym + 1, .sh_info = (u32)first_global, .sh_addralign = 4, .sh_entsize = sizeof(Elf32_Sym) };
	off += sh[isym].sh_size;
	sh[isym + 1] = (Elf32_Shdr){ .sh_name = str_add(&shs, ".strtab"), .sh_type = SHT_STRTAB, .sh_offset = off, .sh_size = (u32)ostr.len, .sh_addralign = 1 };
	off += (u32)ostr.len;
	sh[isym + 2] = (Elf32_Shdr){ .sh_name = str_add(&shs, ".shstrtab"), .sh_type = SHT_STRTAB, .sh_offset = off, .sh_addralign = 1 };
	sh[isym + 2].sh_size = (u32)shs.len; off += (u32)shs.len;
	u32 shoff = alignup(off, 4), total = shoff + (u32)nsh * sizeof(Elf32_Shdr);

	u8 *buf = calloc(1, total);
	Elf32_Ehdr eh = { .e_type = ET_REL, .e_machine = md_e_machine, .e_version = 1, .e_shoff = shoff, .e_flags = e_flags,
	    .e_ehsize = sizeof(Elf32_Ehdr), .e_shentsize = sizeof(Elf32_Shdr), .e_shnum = (u16)nsh, .e_shstrndx = (u16)(isym + 2) };
	memcpy(eh.e_ident, "\177ELF\1\1\1", 7);
	memcpy(buf, &eh, sizeof eh);
	for (int i = 0; i < nobj; i++) {   /* each piece's bytes (with moved addends) at its offset; gaps stay zero */
		Obj *o = objs[i]; if (!o->active) continue;
		for (int j = 1; j < o->nsh; j++) {
			int r = rmap[i][j]; if (r < 0 || rsec[r].type == SHT_NOBITS || o->sh[j].sh_type == SHT_NOBITS) continue;
			memcpy(buf + sh[rsec[r].index].sh_offset + o->sec_vaddr[j], o->data + o->sh[j].sh_offset, o->sh[j].sh_size);
			if (cantunwind[i][j]) {   /* the appended entry: the addend (the text piece's end), EXIDX_CANTUNWIND */
				u8 *e = buf + sh[rsec[r].index].sh_offset + o->sec_vaddr[j] + o->sh[j].sh_size; int t = (int)o->sh[j].sh_link;
				wr32(e, o->sec_vaddr[t] + o->sh[t].sh_size); wr32(e + 4, 1);
			}
		}
	}
	for (int q = 1, x = 1 + nrsec; q <= nrsec; q++) if (rsec[byidx[q]].nrel) { memcpy(buf + sh[x].sh_offset, rsec[byidx[q]].rel, sh[x].sh_size); x++; }
	memcpy(buf + sh[isym].sh_offset, osym, sh[isym].sh_size);
	memcpy(buf + sh[isym + 1].sh_offset, ostr.b, ostr.len);
	memcpy(buf + sh[isym + 2].sh_offset, shs.b, shs.len);
	memcpy(buf + shoff, sh, (size_t)nsh * sizeof *sh);
	FILE *f = fopen(out, "wb"); if (!f) die("cannot open %s", out);
	if (fwrite(buf, 1, total, f) != total || fclose(f)) die("%s: write failed", out);
}
