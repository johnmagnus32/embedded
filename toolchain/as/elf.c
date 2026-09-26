/*
 * elf.c — the OBJECT backend: serialize the front-end's section/symbol/relocation tables into an ELF32
 * relocatable object. The gas `config/obj-elf.c` equivalent. Architecture-agnostic: it writes whatever
 * e_machine/e_flags the MD backend declares (md_e_machine/md_e_flags) and whatever reloc type codes the
 * MD backend stored in each Reloc — so this file is unchanged when a new architecture is added.
 *
 * obj_write() runs five phases, one helper each, over a small writer context (Obj):
 *   build_symtab  — .symtab image + .strtab (locals before globals; sh_info = first global)
 *   plan_headers  — decide the section-header order/indices (null + secs + .rel + symtab/str/shstr)
 *   build_shdrs   — fill each section header + the .shstrtab section-name table
 *   layout        — assign file offsets + section sizes; compute the aligned section-header offset
 *   write_out     — emit the ELF header, section contents, relocs, symbols, strings, headers
 * REL (not RELA): the addend lives in the instruction the MD backend already emitted.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "as.h"
#include "elfutil.h"   /* shared: Strtab + str_add (the ELF32 structs + constants come via as.h -> elf.h) */

/* Writer context: the intermediate state shared across the phases below. */
typedef struct {
	Elf32_Sym *esym; int ne; int first_global; int *symmap;   /* .symtab image + our->ELF index map */
	Strtab str, shstr;                                        /* .strtab (symbol names) / .shstrtab (section names) */
	Elf32_Shdr *sh; int total;                                /* section headers + their count */
	int relof[MAXSEC];                                        /* user section i -> its .rel header index (-1 = none) */
	int symtab_ndx, strtab_ndx, shstrtab_ndx;                 /* trailing header indices */
	u32 shoff;                                                /* file offset of the section header table */
} Obj;

static int sec_has_rel(int i) { for (int r = 0; r < nrel; r++) if (rels[r].sec == i) return 1; return 0; }

/* Phase 1 — .symtab image + .strtab. ELF requires all locals before all globals; sh_info records the
 * first global's index. symmap[] translates our symbol indices to ELF ones (for the .rel entries). */
static void build_symtab(Obj *o) {
	str_add(&o->str, "");                                     /* strtab[0] = "" */
	o->esym = calloc(nsym + 1, sizeof *o->esym); o->ne = 1;   /* [0] = the null symbol */
	o->symmap = calloc(nsym, sizeof(int));
	/* `.L*` names are assembler-local labels: GAS never puts them in the symbol table unless a relocation
	 * still refers to one (a reloc it couldn't reduce to section-symbol + offset, e.g. movw/movt vs .LSTR). */
	char *used = calloc(nsym ? nsym : 1, 1);
	for (int r = 0; r < nrel; r++) used[rels[r].symidx] = 1;
	for (int pass = 0; pass < 2; pass++) {                    /* pass 0 = locals, pass 1 = globals */
		if (pass == 1) o->first_global = o->ne;
		for (int i = 0; i < nsym; i++) {
			int is_local = syms[i].defined && !syms[i].global && !syms[i].weak;   /* weak binds like global (comes after locals) */
			if (!syms[i].name || is_local != (pass == 0)) continue;
			if (is_local && !used[i] && syms[i].type != STT_SECTION && !strncmp(syms[i].name, ".L", 2)) continue;
			if (!syms[i].defined && syms[i].weak && !used[i]) continue;   /* GAS drops an unreferenced undefined .weak (an unreferenced .global stays) */
			int e = o->ne++; o->symmap[i] = e;
			o->esym[e].st_name  = (syms[i].type == STT_SECTION) ? 0 : str_add(&o->str, syms[i].name);   /* section syms are nameless (GNU) */
			o->esym[e].st_value = syms[i].value;
			o->esym[e].st_size  = syms[i].size;
			o->esym[e].st_info  = ELF32_ST_INFO(syms[i].weak ? STB_WEAK : (is_local ? STB_LOCAL : STB_GLOBAL), syms[i].type);
			o->esym[e].st_shndx = !syms[i].defined ? SHN_UNDEF : syms[i].sec == SEC_ABS ? SHN_ABS : secs[syms[i].sec].shndx;
		}
	}
	free(used);
}

/* Phase 2 — decide the section-header layout: [0]=null, user secs (1..nsec), a .rel.<sec> per section
 * with relocations, then .symtab, .strtab, .shstrtab. Records each user section's .rel header in relof[]. */
static void plan_headers(Obj *o) {
	for (int i = 0; i < nsec; i++) o->relof[i] = -1;
	o->total = 1 + nsec;
	for (int i = 0; i < nsec; i++) if (sec_has_rel(i)) o->relof[i] = o->total++;
	o->symtab_ndx = o->total++; o->strtab_ndx = o->total++; o->shstrtab_ndx = o->total++;
	o->sh = calloc(o->total, sizeof *o->sh);
}

/* Phase 3 — fill every section header + intern its name into .shstrtab. */
static void build_shdrs(Obj *o) {
	str_add(&o->shstr, "");
	for (int i = 0; i < nsec; i++) { Elf32_Shdr *h = &o->sh[secs[i].shndx];
		h->sh_name = str_add(&o->shstr, secs[i].name); h->sh_type = secs[i].type; h->sh_flags = secs[i].flags; h->sh_addralign = secs[i].align; h->sh_entsize = secs[i].entsize; }   /* GAS: the largest alignment requested in the section (default 1) */
	for (int i = 0; i < nsec; i++) if (o->relof[i] >= 0) { char nm[128]; snprintf(nm, sizeof nm, ".rel%s", secs[i].name);
		Elf32_Shdr *h = &o->sh[o->relof[i]];
		h->sh_type = SHT_REL; h->sh_name = str_add(&o->shstr, nm); h->sh_flags = 0x40;   /* SHF_INFO_LINK: sh_info names a section (GAS) */
		h->sh_link = o->symtab_ndx; h->sh_info = secs[i].shndx;   /* link=symtab it indexes, info=section it patches */
		h->sh_addralign = 4; h->sh_entsize = sizeof(Elf32_Rel); }
	Elf32_Shdr *st = &o->sh[o->symtab_ndx];
	st->sh_type = SHT_SYMTAB; st->sh_name = str_add(&o->shstr, ".symtab");
	st->sh_link = o->strtab_ndx; st->sh_info = o->first_global;   /* link=its strtab, info=first global index */
	st->sh_addralign = 4; st->sh_entsize = sizeof(Elf32_Sym);
	o->sh[o->strtab_ndx].sh_type = SHT_STRTAB;   o->sh[o->strtab_ndx].sh_name   = str_add(&o->shstr, ".strtab");   o->sh[o->strtab_ndx].sh_addralign = 1;
	o->sh[o->shstrtab_ndx].sh_type = SHT_STRTAB; o->sh[o->shstrtab_ndx].sh_name = str_add(&o->shstr, ".shstrtab"); o->sh[o->shstrtab_ndx].sh_addralign = 1;
}

/* Phase 4 — assign file offsets + sizes (sections 4-aligned), then the 4-aligned section-header offset. */
static void layout(Obj *o) {
	u32 off = sizeof(Elf32_Ehdr);
	for (int i = 0; i < nsec; i++) { Elf32_Shdr *h = &o->sh[secs[i].shndx];
		h->sh_offset = off; h->sh_size = secs[i].len;
		if (secs[i].type != SHT_NOBITS) off = (off + secs[i].len + 3) & ~3u; }   /* .bss occupies no file space */
	for (int i = 0; i < nsec; i++) if (o->relof[i] >= 0) { int cnt = 0; for (int r = 0; r < nrel; r++) if (rels[r].sec == i) cnt++;
		Elf32_Shdr *h = &o->sh[o->relof[i]]; h->sh_offset = off; h->sh_size = cnt * sizeof(Elf32_Rel); off += h->sh_size; }
	o->sh[o->symtab_ndx].sh_offset = off; o->sh[o->symtab_ndx].sh_size = o->ne * sizeof(Elf32_Sym); off += o->sh[o->symtab_ndx].sh_size;
	o->sh[o->strtab_ndx].sh_offset = off; o->sh[o->strtab_ndx].sh_size = o->str.len; off += o->str.len;
	o->sh[o->shstrtab_ndx].sh_offset = off; o->sh[o->shstrtab_ndx].sh_size = o->shstr.len; off += o->shstr.len;
	o->shoff = (off + 3) & ~3u;
}

/* Phase 5 — emit the file in offset order: header, section contents, relocs, symbols, strings, headers. */
static void write_out(Obj *o, FILE *f) {
	Elf32_Ehdr eh = {0};
	memcpy(eh.e_ident, "\177ELF\1\1\1", 7);   /* MAG + ELFCLASS32 + ELFDATA2LSB + EV_CURRENT */
	eh.e_type = ET_REL; eh.e_machine = md_e_machine; eh.e_version = 1; eh.e_flags = md_e_flags;
	eh.e_ehsize = sizeof eh; eh.e_shentsize = sizeof(Elf32_Shdr); eh.e_shnum = o->total; eh.e_shstrndx = o->shstrtab_ndx; eh.e_shoff = o->shoff;

	u8 pad[4] = {0};
	fwrite(&eh, sizeof eh, 1, f);
	for (int i = 0; i < nsec; i++) { if (secs[i].type == SHT_NOBITS) continue;   /* .bss: no bytes on disk */
		fwrite(secs[i].data, 1, secs[i].len, f); u32 p = (4 - (secs[i].len & 3)) & 3; if (p) fwrite(pad, 1, p, f); }
	/* each .rel section in ADDRESS order (as GAS writes them; ours creates branch relocs at end of pass) — stable */
	int *ord = malloc((nrel ? nrel : 1) * sizeof *ord);
	for (int i = 0; i < nsec; i++) if (o->relof[i] >= 0) {
		int n = 0; for (int r = 0; r < nrel; r++) if (rels[r].sec == i) ord[n++] = r;
		for (int a = 1; a < n; a++) { int v = ord[a], b = a - 1; while (b >= 0 && rels[ord[b]].off > rels[v].off) { ord[b + 1] = ord[b]; b--; } ord[b + 1] = v; }
		for (int k = 0; k < n; k++) { int r = ord[k]; Elf32_Rel er = { rels[r].off, ELF32_R_INFO(o->symmap[rels[r].symidx], rels[r].type) }; fwrite(&er, sizeof er, 1, f); }
	}
	free(ord);
	fwrite(o->esym, sizeof(Elf32_Sym), o->ne, f);
	fwrite(o->str.b, 1, o->str.len, f);
	fwrite(o->shstr.b, 1, o->shstr.len, f);
	for (long pos = ftell(f); pos < (long)o->shoff; pos++) fputc(0, f);   /* pad to the aligned shoff */
	fwrite(o->sh, sizeof(Elf32_Shdr), o->total, f);
}

void obj_write(const char *path) {
	FILE *f = fopen(path, "wb"); if (!f) die("cannot open %s", path);
	Obj o = {0};
	for (int i = 0; i < nsec; i++) secs[i].shndx = 1 + i;   /* ELF section indices 1..nsec (build_symtab needs them) */
	build_symtab(&o);
	plan_headers(&o);
	build_shdrs(&o);
	layout(&o);
	write_out(&o, f);
	fclose(f);
}
