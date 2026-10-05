/*
 * objcopy.c — our from-scratch object copier, the GNU `objcopy` and `strip` equivalent (one program: run as `strip`
 * — argv[0] ending in "strip" — it strips files in place, as GNU strip does).
 *
 * In:  an ELF32 little-endian object, executable or shared object.
 * Out: the same ELF, minus what the options remove (-j / -R sections, -S / -g / --strip-unneeded symbols and debug
 *      sections), laid out the way GNU's BFD writes an ELF so the result is byte-identical to GNU objcopy/strip:
 *        - an executable's / shared object's allocated sections keep their file offsets (its segments are unchanged);
 *          a relocatable's sections are placed in order from the ELF header, each at its alignment;
 *        - then the other sections, in order; then .symtab and .strtab; then a relocatable's relocation sections;
 *          then .shstrtab; then the section headers (4-aligned);
 *        - section order: the input's, each relocation section right after the section it relocates, then .symtab,
 *          .strtab, .shstrtab;
 *        - symbols: the kept ones, locals first, in input order (section and file symbols count as debugging ones);
 *        - .strtab / .shstrtab are GNU's suffix-merged string tables (".text" shares ".rel.text"'s bytes).
 *   or -O binary: the loadable sections' bytes laid out by load address from the lowest one, gaps --gap-fill'ed.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <utime.h>
#include <unistd.h>
#include "elfread.h"
#include "arread.h"
#include "strmap.h"

static const char *tool = "objcopy";
void die(const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	fprintf(stderr, "%s: ", tool); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap); exit(1);
}
static void *xcalloc(size_t n, size_t s) { void *p = calloc(n ? n : 1, s); if (!p) die("out of memory"); return p; }

/* ---- options ---------------------------------------------------------------------------------------------- */
enum { STRIP_NONE, STRIP_DEBUG, STRIP_UNNEEDED, STRIP_ALL };
enum { LOCALS_UNDEF, LOCALS_START_L, LOCALS_ALL };
static int strip_symbols = STRIP_NONE, discard_locals = LOCALS_UNDEF, out_binary, preserve_dates;
static int gap_fill_set, pad_to_set; static u8 gap_fill; static u32 pad_to;
typedef struct { const char *pat; int kind; } Pat;        /* kind: 'R' remove, 'j' only, 'r' remove relocations */
static Pat *pats; static int npat, patcap;
static const char **keep_syms, **strip_syms; static int nkeep, nstripsym;

static void add_pat(const char *p, int kind) {
	if (npat == patcap) { patcap = patcap ? 2 * patcap : 16; pats = realloc(pats, patcap * sizeof *pats); }
	pats[npat].pat = p; pats[npat++].kind = kind;
}
/* GNU's find_section_list: the LATEST matching option of that kind decides; "!pat" excludes. */
static int pat_match(const char *name, int kind) {
	for (int i = npat - 1; i >= 0; i--) {
		if (pats[i].kind != kind) continue;
		const char *p = pats[i].pat;
		if (p[0] == '!') { if (!fnmatch(p + 1, name, 0)) return 0; }
		else if (!fnmatch(p, name, 0)) return 1;
	}
	return 0;
}
/* --rename-section OLD=NEW[,FLAGS] / --set-section-flags PAT=FLAGS: GNU's section flags (BFD's SEC_*), which a flag
 * list REPLACES (--set-section-flags keeps contents and relocations); a section keeps its bytes only with `contents`. */
enum { F_ALLOC = 1, F_LOAD = 2, F_NOLOAD = 4, F_READONLY = 8, F_DEBUG = 16, F_CODE = 32, F_DATA = 64, F_ROM = 128,
       F_EXCLUDE = 256, F_SHARE = 512, F_CONTENTS = 1024, F_MERGE = 2048, F_STRINGS = 4096, F_RELOC = 8192, F_TLS = 16384 };
typedef struct { const char *from, *to; int flags; } Rename;   /* flags -1: unchanged */
static Rename *renames; static int nrename;
typedef struct { const char *pat; int flags; } SetFlags;
static SetFlags *setflags; static int nsetflags;
static int parse_flags(const char *s) {
	static const struct { const char *n; int f; } t[] = { {"alloc", F_ALLOC}, {"load", F_LOAD}, {"noload", F_NOLOAD}, {"readonly", F_READONLY},
		{"debug", F_DEBUG}, {"code", F_CODE}, {"data", F_DATA}, {"rom", F_ROM}, {"exclude", F_EXCLUDE}, {"share", F_SHARE},
		{"contents", F_CONTENTS}, {"merge", F_MERGE}, {"strings", F_STRINGS} };
	int f = 0;
	while (*s) {
		const char *e = strchr(s, ','); size_t n = e ? (size_t)(e - s) : strlen(s); int k;
		for (k = 0; k < (int)(sizeof t / sizeof *t); k++) if (strlen(t[k].n) == n && !strncmp(t[k].n, s, n)) break;
		if (k == (int)(sizeof t / sizeof *t)) die("unrecognized section flag `%.*s'", (int)n, s);
		f |= t[k].f; s += n + (e != NULL);
	}
	return f;
}
static int any_pat(int kind) { for (int i = 0; i < npat; i++) if (pats[i].kind == kind) return 1; return 0; }
static int named(const char *n, const char **list, int k) { for (int i = 0; i < k; i++) if (!strcmp(list[i], n)) return 1; return 0; }

/* ---- GNU's ELF string table: insertion order, then every string that is a suffix of another shares its bytes --- */
typedef struct { const char *s; int len, ref, keep; u32 off; int suffix_of; } StrEnt;
typedef struct { StrEnt *e; int n, cap; StrMap at; } StrTab;   /* at: string -> entry + 1 */
static int st_add(StrTab *t, const char *s) {   /* the entry (deduplicated); "" is entry -1 (offset 0) */
	if (!*s) return -1;
	long have = (long)(size_t)strmap_get(&t->at, s);
	if (have) { t->e[have - 1].ref++; return (int)have - 1; }
	if (t->n == t->cap) { t->cap = t->cap ? 2 * t->cap : 64; t->e = realloc(t->e, t->cap * sizeof *t->e); }
	t->e[t->n] = (StrEnt){ s, (int)strlen(s), 1, 0, 0, -1 };
	strmap_put(&t->at, s, (void *)(size_t)(t->n + 1));
	return t->n++;
}
static StrTab *sort_tab;
static int strrev_cmp(const void *x, const void *y) {   /* _bfd_elf_strtab_finalize's strrevcmp: compared from the last byte */
	const StrEnt *a = &sort_tab->e[*(const int *)x], *b = &sort_tab->e[*(const int *)y];
	const unsigned char *s = (const unsigned char *)a->s + a->len - 1, *u = (const unsigned char *)b->s + b->len - 1;
	for (int l = a->len < b->len ? a->len : b->len; l; l--, s--, u--) if (*s != *u) return (int)*s - (int)*u;
	return a->len - b->len;
}
static u8 *st_finish(StrTab *t, u32 *size) {
	int *ord = xcalloc(t->n, sizeof *ord), m = 0;
	for (int i = 0; i < t->n; i++) if (t->e[i].ref) ord[m++] = i;
	sort_tab = t; qsort(ord, m, sizeof *ord, strrev_cmp);
	if (m) {   /* walk from the end: a string that ends the current one merges into it */
		int cur = ord[m - 1];
		for (int k = m - 2; k >= 0; k--) {
			StrEnt *c = &t->e[ord[k]], *e = &t->e[cur];
			if (c->len < e->len && !memcmp(e->s + e->len - c->len, c->s, c->len)) c->suffix_of = cur;
			else cur = ord[k];
		}
	}
	u32 off = 1;
	for (int i = 0; i < t->n; i++) if (t->e[i].ref && t->e[i].suffix_of < 0) { t->e[i].off = off; off += t->e[i].len + 1; }
	for (int i = 0; i < t->n; i++) if (t->e[i].ref && t->e[i].suffix_of >= 0) {
		StrEnt *e = &t->e[t->e[i].suffix_of];
		t->e[i].off = e->off + e->len - t->e[i].len;
	}
	u8 *b = xcalloc(off, 1);
	for (int i = 0; i < t->n; i++) if (t->e[i].ref && t->e[i].suffix_of < 0) memcpy(b + t->e[i].off, t->e[i].s, t->e[i].len);
	*size = off; free(ord);
	return b;
}
static u32 st_off(const StrTab *t, int i) { return i < 0 ? 0 : t->e[i].off; }

/* ---- BFD's view of a section ----------------------------------------------------------------------------- */
static int starts(const char *s, const char *p) { return !strncmp(s, p, strlen(p)); }
static int is_debug(const ElfFile *f, int j) {   /* SEC_DEBUGGING: by name, for a non-allocated section */
	const char *nm = f->shstr + f->sh[j].sh_name;
	if (f->sh[j].sh_flags & SHF_ALLOC || nm[0] != '.') return 0;
	return starts(nm, ".debug") || starts(nm, ".gnu.debuglto_.debug_") || starts(nm, ".gnu.linkonce.wi.") || starts(nm, ".zdebug")
	    || starts(nm, ".line") || starts(nm, ".stab") || !strcmp(nm, ".gdb_index");
}
#define PT_TLS_ 7
static int bfd_flags(const ElfFile *f, int j, int has_relocs) {   /* _bfd_elf_make_section_from_shdr's SEC_* for section j */
	const Elf32_Shdr *s = &f->sh[j]; int fl = 0;
	if (s->sh_type != SHT_NOBITS) fl |= F_CONTENTS;
	if (s->sh_flags & SHF_ALLOC) { fl |= F_ALLOC; if (s->sh_type != SHT_NOBITS) fl |= F_LOAD; }
	if (!(s->sh_flags & SHF_WRITE)) fl |= F_READONLY;
	if (s->sh_flags & SHF_EXECINSTR) fl |= F_CODE; else if (fl & F_LOAD) fl |= F_DATA;
	if (s->sh_flags & SHF_MERGE) fl |= F_MERGE;
	if (s->sh_flags & SHF_STRINGS) fl |= F_STRINGS;
	if (s->sh_flags & SHF_TLS) fl |= F_TLS;
	if (s->sh_flags & 0x80000000u) fl |= F_EXCLUDE;
	if (is_debug(f, j)) fl |= F_DEBUG;
	if (has_relocs) fl |= F_RELOC;
	return fl;
}
/* ELF_SECTION_IN_SEGMENT (binutils include/elf/internal.h), check_vma = 1, strict = 0. */
static int in_segment(const Elf32_Shdr *s, const Elf32_Phdr *p) {
	int tls = (s->sh_flags & SHF_TLS) != 0;
	u32 size = !tls || s->sh_type != SHT_NOBITS || p->p_type == PT_TLS_ ? s->sh_size : 0;
	if (!(tls ? (p->p_type == PT_TLS_ || p->p_type == PT_GNU_RELRO || p->p_type == PT_LOAD) : (p->p_type != PT_TLS_ && p->p_type != PT_PHDR))) return 0;
	if (!(s->sh_flags & SHF_ALLOC) && (p->p_type == PT_LOAD || p->p_type == PT_DYNAMIC || p->p_type == 0x6474e550u /* EH_FRAME */
	    || p->p_type == PT_GNU_STACK || p->p_type == PT_GNU_RELRO || p->p_type == 0x6474e554u /* SFRAME */)) return 0;
	if (s->sh_type != SHT_NOBITS && !(s->sh_offset >= p->p_offset && s->sh_offset - p->p_offset + size <= p->p_filesz)) return 0;
	if (s->sh_flags & SHF_ALLOC && !(s->sh_addr >= p->p_vaddr && s->sh_addr - p->p_vaddr + size <= p->p_memsz)) return 0;
	if ((p->p_type == PT_DYNAMIC || p->p_type == 4 /* NOTE */) && !s->sh_size && p->p_memsz
	    && !((s->sh_type == SHT_NOBITS || (s->sh_offset > p->p_offset && s->sh_offset - p->p_offset < p->p_filesz))
	         && (!(s->sh_flags & SHF_ALLOC) || (s->sh_addr > p->p_vaddr && s->sh_addr - p->p_vaddr < p->p_memsz)))) return 0;
	return 1;
}
/* A section's load address, as BFD computes it from the program headers (an object's: its address). */
static u32 section_lma(const ElfFile *f, int j) {
	const Elf32_Shdr *s = &f->sh[j];
	u32 lma = s->sh_addr;
	if (!(s->sh_flags & SHF_ALLOC) || !f->eh->e_phnum) return lma;
	const Elf32_Phdr *ph = (const Elf32_Phdr *)(f->data + f->eh->e_phoff);
	int nload = 0, k;
	for (k = 0; k < f->eh->e_phnum; k++) { if (ph[k].p_paddr) break; if (ph[k].p_type == PT_LOAD && ph[k].p_memsz) nload++; }
	if (k == f->eh->e_phnum && nload > 1) return lma;   /* all p_paddr zero: keep lma = vma */
	int load = s->sh_type != SHT_NOBITS;
	for (k = 0; k < f->eh->e_phnum; k++) {
		const Elf32_Phdr *p = &ph[k];
		if (!(((p->p_type == PT_LOAD && !(s->sh_flags & SHF_TLS)) || p->p_type == PT_TLS_) && in_segment(s, p))) continue;
		lma = load ? p->p_paddr + s->sh_offset - p->p_offset : p->p_paddr + s->sh_addr - p->p_vaddr;
		if (s->sh_addr >= p->p_vaddr && s->sh_addr + s->sh_size <= p->p_vaddr + p->p_memsz) break;
	}
	return lma;
}

/* ---- the copy ---------------------------------------------------------------------------------------------- */
static u8 *slurp(const char *path, long *size) {
	FILE *fp = fopen(path, "rb"); if (!fp) die("'%s': No such file", path);
	fseek(fp, 0, SEEK_END); *size = ftell(fp); fseek(fp, 0, SEEK_SET);
	u8 *b = xcalloc(*size, 1);
	if (fread(b, 1, *size, fp) != (size_t)*size) die("%s: read failed", path);
	fclose(fp); return b;
}
/* The result, in memory until the end (the input is never touched before it is complete). */
static u8 *obuf; static size_t olen; static int out_exec;   /* out_exec: the input was an executable / shared object */
static void emit(u8 *b, size_t n) { obuf = b; olen = n; }

typedef struct { u32 lma, size; int j; } BinSec;
static int bin_cmp(const void *x, const void *y) {   /* compare_section_lma: by load address, then size, then order */
	const BinSec *a = x, *b = y;
	if (a->lma != b->lma) return a->lma < b->lma ? -1 : 1;
	if (a->size != b->size) return a->size < b->size ? -1 : 1;
	return a->j - b->j;
}
/* -O binary: every kept section with contents that is allocated (BFD: SEC_LOAD), at its load address minus the
 * lowest such (sized) one; gaps between consecutive ones (by load address) filled with --gap-fill, the last one
 * extended to --pad-to. */
static void write_binary(const ElfFile *f, const int *keep) {
	BinSec *v = xcalloc(f->nsh, sizeof *v); int n = 0;
	for (int j = 1; j < f->nsh; j++) {
		const Elf32_Shdr *s = &f->sh[j];
		if (!keep[j] || !(s->sh_flags & SHF_ALLOC) || s->sh_type == SHT_NOBITS) continue;
		v[n++] = (BinSec){ section_lma(f, j), s->sh_size, j };
	}
	qsort(v, n, sizeof *v, bin_cmp);
	int found = 0; u32 low = 0;
	for (int i = 0; i < n; i++) if (v[i].size && (!found || v[i].lma < low)) { low = v[i].lma; found = 1; }
	u32 end = 0;
	u32 *grow = xcalloc(n, sizeof *grow);
	if (gap_fill_set || pad_to_set) {
		for (int i = 0; i + 1 < n; i++) {
			u32 gs = v[i].lma + v[i].size, ge = v[i + 1].lma;
			if (gap_fill_set && gs < ge) grow[i] = ge - gs;
		}
		if (pad_to_set && n && v[n - 1].lma + v[n - 1].size < pad_to) grow[n - 1] = pad_to - (v[n - 1].lma + v[n - 1].size);
	}
	for (int i = 0; i < n; i++) if (v[i].size + grow[i]) { u32 e = v[i].lma - low + v[i].size + grow[i]; if (e > end) end = e; }
	u8 *b = xcalloc(end, 1);
	for (int i = 0; i < n; i++) {
		if (!v[i].size && !grow[i]) continue;
		const Elf32_Shdr *s = &f->sh[v[i].j];
		memcpy(b + (v[i].lma - low), f->data + s->sh_offset, s->sh_size);
		memset(b + (v[i].lma - low) + s->sh_size, gap_fill, grow[i]);
	}
	emit(b, end);
	free(v); free(grow);
}

/* _bfd_elf_get_reloc_section: the section a (non-symtab) REL section applies to, by name — ".rel.X" -> "X"; ".rel.plt"
 * -> ".got.plt", else ".got" (ARM wants a .got.plt). 0 = none. */
static int section_named(const ElfFile *f, const char *nm) { for (int j = 1; j < f->nsh; j++) if (!strcmp(f->shstr + f->sh[j].sh_name, nm)) return j; return 0; }
static int reloc_target(const ElfFile *f, int j) {
	const char *nm = f->shstr + f->sh[j].sh_name;
	if (strncmp(nm, ".rel", 4)) return 0;
	nm += 4;
	if (!strcmp(nm, ".plt")) { int g = section_named(f, ".got.plt"); return g ? g : section_named(f, ".got"); }
	return section_named(f, nm);
}
static const char *out_name(const ElfFile *f, int j) {   /* a section's output name (--rename-section) */
	const char *nm = f->shstr + f->sh[j].sh_name;
	for (int k = 0; k < nrename; k++) if (!strcmp(renames[k].from, nm)) return renames[k].to;
	return nm;
}
typedef struct { int in; } Out;   /* an output section: its input index, or a made one (-1 .symtab, -2 .strtab, -3 .shstrtab) */
static void copy_elf(const char *in) {
	long size; u8 *data = slurp(in, &size);
	if (ar_is_archive(data, size)) die("%s: archives: unsupported", in);
	if (size < 18 || memcmp(data, "\177ELF", 4)) die("%s: file format not recognized", in);
	ElfFile f; elf_open(&f, in, data, size, data[16] | data[17] << 8, 0);
	int et = f.eh->e_type, rel_obj = et == ET_REL, nsh = f.nsh;
	out_exec = et == ET_EXEC || (et == ET_DYN && !out_binary);   /* (BFD: EXEC_P; a .so's DYNAMIC doesn't survive -O binary) */
	int symtab = f.symtab, strtab = symtab ? (int)f.sh[symtab].sh_link : 0, shstr = f.eh->e_shstrndx;
	int *relof = xcalloc(nsh, sizeof *relof), *relsec = xcalloc(nsh, sizeof *relsec), *keep = xcalloc(nsh, sizeof *keep);
	int has_reloc = 0;
	for (int j = 1; j < nsh; j++) {
		const Elf32_Shdr *s = &f.sh[j];
		if (s->sh_type == 4 /* RELA */) die("%s: RELA relocations: unsupported (ARM uses REL)", in);
		if (s->sh_type == 17 /* GROUP */ || s->sh_type == 18 /* SYMTAB_SHNDX */) die("%s: section %s: type %u unsupported", in, f.shstr + s->sh_name, s->sh_type);
		if (s->sh_type == SHT_REL && symtab && (int)s->sh_link == symtab && s->sh_info && (int)s->sh_info < nsh) {
			if (relsec[s->sh_info]) die("%s: two relocation sections for section %u", in, s->sh_info);
			relof[j] = (int)s->sh_info; relsec[s->sh_info] = j; has_reloc = 1;
		}
	}
	for (int j = 1; j < nsh; j++)
		if (j != symtab && f.sh[j].sh_link == (u32)strtab && strtab && f.sh[j].sh_type != SHT_REL) die("%s: section %d shares the symbol string table", in, j);
	/* is_strip_section */
	int only = any_pat('j');
	for (int j = 1; j < nsh; j++) {
		if (j == symtab || j == strtab || j == shstr || relof[j]) continue;
		const char *nm = f.shstr + f.sh[j].sh_name;
		int r = pat_match(nm, 'R'), c = pat_match(nm, 'j');
		if (r && c) die("error: section %s matches both remove and copy options", nm);
		int strip = r || (only && !c) || (is_debug(&f, j) && (strip_symbols != STRIP_NONE || discard_locals == LOCALS_ALL));
		keep[j] = !strip;
		if (strip && !out_binary && et != ET_REL && f.sh[j].sh_flags & SHF_ALLOC && f.sh[j].sh_size)
			die("%s: removing the allocated section %s from an executable: unsupported", in, nm);
	}
	if (out_binary) { write_binary(&f, keep); free(relof); free(relsec); free(keep); return; }
	for (int j = 1; j < nsh; j++)   /* a relocation section goes with its section, unless -R names it (or --remove-relocations) */
		if (relof[j]) keep[j] = keep[relof[j]] && !pat_match(f.shstr + f.sh[j].sh_name, 'R') && !pat_match(f.shstr + f.sh[relof[j]].sh_name, 'r');
	if (strip_symbols == STRIP_ALL) for (int j = 1; j < nsh; j++) if (relof[j] && keep[j]) die("%s: --strip-all of a relocatable object with relocations: unsupported", in);

	/* filter_symbols */
	int nsym = f.nsym; const Elf32_Sym *sym = f.sym;
	int *used = xcalloc(nsym, sizeof *used), *ksym = xcalloc(nsym, sizeof *ksym), *newsym = xcalloc(nsym, sizeof *newsym);
	for (int j = 1; j < nsh; j++) if (relof[j] && keep[j]) {
		const Elf32_Rel *r = (const Elf32_Rel *)(data + f.sh[j].sh_offset);
		for (u32 k = 0; k < f.sh[j].sh_size / sizeof *r; k++) used[ELF32_R_SYM(r[k].r_info)] = 1;
	}
	int nfilt = 0;
	for (int k = 1; k < nsym; k++) {
		const Elf32_Sym *s = &sym[k]; const char *nm = f.strtab + s->st_name;
		int bind = ELF32_ST_BIND(s->st_info), type = ELF32_ST_TYPE(s->st_info), x = s->st_shndx;
		int global = bind == STB_GLOBAL || bind == STB_WEAK || bind == 10, undef = x == SHN_UNDEF, com = x == SHN_COMMON;
		int debugging = type == STT_SECTION || type == STT_FILE, keepit;
		if (strip_symbols == STRIP_ALL) keepit = 0;
		else if (used[k]) keepit = 1;
		else if (rel_obj && (global || com)) keepit = 1;
		else if (global || undef || com) keepit = strip_symbols != STRIP_UNNEEDED;
		else if (debugging) keepit = strip_symbols != STRIP_DEBUG && strip_symbols != STRIP_UNNEEDED;
		else keepit = strip_symbols != STRIP_UNNEEDED && discard_locals != LOCALS_ALL
		              && !(discard_locals == LOCALS_START_L && ((nm[0] == '.' && (nm[1] == 'L' || nm[1] == '.')) || starts(nm, "_.L_")));
		if (keepit && named(nm, strip_syms, nstripsym)) {
			if (used[k]) die("not stripping symbol `%s' because it is named in a relocation", nm);
			keepit = 0;
		}
		if (!keepit && named(nm, keep_syms, nkeep)) keepit = 1;
		if (rel_obj && strip_symbols != STRIP_ALL && nm[0] == '$' && (nm[1] == 'a' || nm[1] == 't' || nm[1] == 'd') && (!nm[2] || nm[2] == '.'))
			keepit = 1;   /* an object's ARM mapping symbols stay — through --strip-unneeded, -x, even -N (as GNU's) */
		if (keepit && x && x < nsh && !keep[x]) keepit = 0;   /* in a removed section */
		if (keepit && (x == SHN_UNDEF || x == SHN_ABS || x == SHN_COMMON)) {   /* BFD's pseudo-sections answer to -R / -j too */
			const char *ps = x == SHN_UNDEF ? "*UND*" : x == SHN_ABS ? "*ABS*" : "*COM*";
			if (pat_match(ps, 'R') || (only && !pat_match(ps, 'j'))) keepit = 0;
		}
		ksym[k] = keepit; nfilt += keepit;
	}
	int need_symtab = symtab && strip_symbols != STRIP_ALL && (nfilt > 0 || (rel_obj && has_reloc));
	if (need_symtab) for (int k = 1; k < nsym; k++)   /* a kept relocation's symbol, gone with its section: GNU refuses */
		if (used[k] && !ksym[k]) {
			const char *nm = ELF32_ST_TYPE(sym[k].st_info) == STT_SECTION && sym[k].st_shndx < nsh ? f.shstr + f.sh[sym[k].st_shndx].sh_name : f.strtab + sym[k].st_name;
			die("%s: symbol `%s' required but not present", in, nm);
		}

	/* output sections, in BFD's order */
	Out *os = xcalloc(nsh + 3, sizeof *os); int nos = 1; int *newidx = xcalloc(nsh, sizeof *newidx);
	for (int j = 1; j < nsh; j++) {
		if (!keep[j] || relof[j]) continue;
		os[nos].in = j; newidx[j] = nos++;
		if (relsec[j] && keep[relsec[j]] && need_symtab) { os[nos].in = relsec[j]; newidx[relsec[j]] = nos++; }
	}
	int o_symtab = 0, o_strtab = 0;
	if (need_symtab) { o_symtab = nos; os[nos++].in = -1; o_strtab = nos; os[nos++].in = -2; }
	int o_shstr = nos; os[nos++].in = -3;

	/* elf_map_symbols: the kept locals; then, in an executable or shared object, a section symbol for each output
	 * section without one (in section order); then the globals — each in input order */
	Elf32_Sym *osym = xcalloc(nsym + nos + 1, sizeof *osym); const char **oname = xcalloc(nsym + nos + 1, sizeof *oname);
	int *hassec = xcalloc(nsh, sizeof *hassec), nout = 0, nlocal;
	for (int pass = 0; pass < 2; pass++) {
		for (int k = 1; k < nsym; k++) {
			if (!ksym[k] || (ELF32_ST_BIND(sym[k].st_info) != STB_LOCAL) != pass) continue;
			osym[++nout] = sym[k]; newsym[k] = nout;
			if (ELF32_ST_TYPE(sym[k].st_info) == STT_SECTION) { if (sym[k].st_shndx < nsh) hassec[sym[k].st_shndx] = 1; }
			else oname[nout] = f.strtab + sym[k].st_name;
		}
		if (pass) continue;
		for (int i = 1; i < nos && !rel_obj && need_symtab; i++) {
			int j = os[i].in;
			if (j > 0 && !relof[j] && !hassec[j]) osym[++nout] = (Elf32_Sym){ 0, f.sh[j].sh_addr, 0, ELF32_ST_INFO(STB_LOCAL, STT_SECTION), 0, (u16)j };
		}
		nlocal = nout;
	}

	/* names: .symtab .strtab .shstrtab first (BFD's prep_headers), then each section's */
	StrTab shs = { 0 }; int *nmi = xcalloc(nos, sizeof *nmi);
	int n_symtab = st_add(&shs, ".symtab"), n_strtab = st_add(&shs, ".strtab"), n_shstr = st_add(&shs, ".shstrtab");
	shs.e[n_symtab].ref = need_symtab; shs.e[n_strtab].ref = need_symtab;
	for (int i = 1; i < nos; i++) {
		if (os[i].in == -1) nmi[i] = n_symtab; else if (os[i].in == -2) nmi[i] = n_strtab; else if (os[i].in == -3) nmi[i] = n_shstr;
		else if (relof[os[i].in]) {   /* BFD names a relocation section after its section: ".rel" + that name */
			const char *t = out_name(&f, relof[os[i].in]); char *rn = malloc(strlen(t) + 5); sprintf(rn, ".rel%s", t);
			nmi[i] = st_add(&shs, rn);
		} else nmi[i] = st_add(&shs, out_name(&f, os[i].in));
	}
	u32 shs_size; u8 *shs_data = st_finish(&shs, &shs_size);

	/* the symbol table */
	StrTab sts = { 0 }; u8 *symdata = NULL, *strdata = NULL; u32 symsize = 0, strsize = 0;
	if (need_symtab) {
		int *si = xcalloc(nout + 1, sizeof *si);
		for (int i = 1; i <= nout; i++) si[i] = oname[i] ? st_add(&sts, oname[i]) : -1;
		strdata = st_finish(&sts, &strsize);
		symsize = (nout + 1) * sizeof(Elf32_Sym); symdata = xcalloc(symsize, 1);
		Elf32_Sym *o = (Elf32_Sym *)symdata;
		for (int i = 1; i <= nout; i++) {
			Elf32_Sym s = osym[i];
			s.st_name = st_off(&sts, si[i]);
			if (s.st_shndx && s.st_shndx < nsh) s.st_shndx = (u16)newidx[s.st_shndx];
			o[i] = s;
		}
		free(si);
	}

	/* contents + headers */
	Elf32_Shdr *sh = xcalloc(nos, sizeof *sh); const u8 **body = xcalloc(nos, sizeof *body); u8 **owned = xcalloc(nos, sizeof *owned);
	for (int i = 1; i < nos; i++) {
		int j = os[i].in;
		if (j > 0) {
			sh[i] = f.sh[j]; body[i] = data + f.sh[j].sh_offset;
			int nf = -1; const char *nm = f.shstr + f.sh[j].sh_name;
			for (int k = 0; k < nrename; k++) if (!strcmp(renames[k].from, nm) && renames[k].flags >= 0) nf = renames[k].flags;
			for (int k = nsetflags - 1; k >= 0 && nf < 0 && !relof[j]; k--)
				if (!fnmatch(setflags[k].pat, nm, 0)) nf = setflags[k].flags | (bfd_flags(&f, j, relsec[j] != 0) & (F_CONTENTS | F_RELOC));
			if (nf >= 0 && !relof[j] && nf != bfd_flags(&f, j, relsec[j] != 0)) {   /* BFD: new flags -> sh_flags, and sh_type anew */
				if (sh[i].sh_type != SHT_PROGBITS && sh[i].sh_type != SHT_NOBITS) die("%s: new flags for section %s (type %u): unsupported", in, nm, sh[i].sh_type);
				if (!rel_obj && f.sh[j].sh_flags & SHF_ALLOC) die("%s: new flags for the allocated section %s of an executable: unsupported", in, nm);
				u32 keepf = sh[i].sh_flags & (SHF_LINK_ORDER | 0x40 /* INFO_LINK */ | 0x200 /* GROUP */);
				sh[i].sh_flags = keepf | (nf & F_ALLOC ? SHF_ALLOC : 0) | (nf & F_READONLY ? 0 : SHF_WRITE) | (nf & F_CODE ? SHF_EXECINSTR : 0)
				               | (nf & F_MERGE ? SHF_MERGE : 0) | (nf & F_STRINGS ? SHF_STRINGS : 0) | (nf & F_EXCLUDE ? 0x80000000u : 0);
				u32 type = (nf & F_ALLOC) && (!(nf & (F_LOAD | F_CONTENTS)) || (nf & F_NOLOAD)) ? SHT_NOBITS : SHT_PROGBITS;
				if (type == SHT_PROGBITS && (sh[i].sh_type == SHT_NOBITS || !(nf & F_CONTENTS)))   /* bytes copied only with `contents` */
					body[i] = owned[i] = xcalloc(sh[i].sh_size, 1);
				sh[i].sh_type = type;
			}
			if (relof[j]) {   /* renumbered against the new symbol table */
				u32 n = f.sh[j].sh_size / sizeof(Elf32_Rel);
				Elf32_Rel *r = xcalloc(n, sizeof *r); memcpy(r, data + f.sh[j].sh_offset, n * sizeof *r);
				for (u32 k = 0; k < n; k++) r[k].r_info = ELF32_R_INFO((u32)newsym[ELF32_R_SYM(r[k].r_info)], ELF32_R_TYPE(r[k].r_info));
				body[i] = owned[i] = (u8 *)r;
				sh[i].sh_link = (u32)o_symtab; sh[i].sh_info = (u32)newidx[relof[j]];
			} else {
				sh[i].sh_link = sh[i].sh_link && sh[i].sh_link < (u32)nsh ? (u32)newidx[sh[i].sh_link] : 0;
				if (sh[i].sh_flags & 0x40 /* INFO_LINK */) sh[i].sh_info = sh[i].sh_info < (u32)nsh ? (u32)newidx[sh[i].sh_info] : 0;
				int t = sh[i].sh_type == SHT_REL ? reloc_target(&f, j) : 0;   /* a dynamic relocation section: BFD links it by name */
				if (t && keep[t]) { sh[i].sh_info = (u32)newidx[t]; sh[i].sh_flags |= 0x40; }
			}
		} else if (j == -1) {
			sh[i] = f.sh[symtab]; sh[i].sh_size = symsize; sh[i].sh_link = (u32)o_strtab; sh[i].sh_info = (u32)nlocal + 1; body[i] = symdata;
		} else if (j == -2) {
			sh[i] = f.sh[strtab]; sh[i].sh_size = strsize; body[i] = strdata;
			sh[i].sh_type = SHT_STRTAB; sh[i].sh_flags = 0; sh[i].sh_addr = 0; sh[i].sh_link = sh[i].sh_info = 0; sh[i].sh_addralign = 1; sh[i].sh_entsize = 0;
		} else {
			memset(&sh[i], 0, sizeof sh[i]); sh[i].sh_type = SHT_STRTAB; sh[i].sh_size = shs_size; sh[i].sh_addralign = 1; body[i] = shs_data;
		}
		sh[i].sh_name = st_off(&shs, nmi[i]);
	}
	/* file positions (BFD: assign_file_positions_*) */
	u32 off = sizeof(Elf32_Ehdr);
	if (!rel_obj) {
		off = f.eh->e_phoff + f.eh->e_phnum * sizeof(Elf32_Phdr);
		for (int i = 1; i < nos; i++) {
			int j = os[i].in;
			if (j <= 0 || !(f.sh[j].sh_flags & SHF_ALLOC)) continue;
			int seg = 0; const Elf32_Phdr *ph = (const Elf32_Phdr *)(data + f.eh->e_phoff);
			for (int k = 0; k < f.eh->e_phnum && !seg; k++) seg = ph[k].p_type == PT_LOAD && in_segment(&f.sh[j], &ph[k]);
			if (!seg && f.sh[j].sh_size) die("%s: allocated section %s is not in a segment: unsupported", in, f.shstr + f.sh[j].sh_name);
			if (f.sh[j].sh_type != SHT_NOBITS && f.sh[j].sh_offset + f.sh[j].sh_size > off) off = f.sh[j].sh_offset + f.sh[j].sh_size;
		}
	}
	#define PLACE(i) do { u32 a_ = sh[i].sh_addralign; if (a_ > 1) off = (off + (a_ & -a_) - 1) & ~((a_ & -a_) - 1); \
	                      sh[i].sh_offset = off; if (sh[i].sh_type != SHT_NOBITS) off += sh[i].sh_size; } while (0)
	for (int i = 1; i < nos; i++) {
		int j = os[i].in;
		if (j > 0 && !rel_obj && f.sh[j].sh_flags & SHF_ALLOC) continue;   /* sh_offset: the input's */
		if (j < 0 || (j > 0 && relof[j])) continue;
		PLACE(i);
	}
	if (o_symtab) { PLACE(o_symtab); PLACE(o_strtab); }
	for (int i = 1; i < nos; i++) if (os[i].in > 0 && relof[os[i].in]) PLACE(i);
	PLACE(o_shstr);
	off = (off + 3) & ~3u;
	u32 shoff = off, total = shoff + nos * sizeof(Elf32_Shdr);
	u8 *b = xcalloc(total, 1);
	Elf32_Ehdr eh = *f.eh; eh.e_shoff = shoff; eh.e_shnum = (u16)nos; eh.e_shstrndx = (u16)o_shstr;
	if (rel_obj) { eh.e_phoff = 0; eh.e_phnum = 0; }
	memcpy(b, &eh, sizeof eh);
	if (!rel_obj && eh.e_phnum) {
		Elf32_Phdr *ph = (Elf32_Phdr *)(b + eh.e_phoff);
		memcpy(ph, data + f.eh->e_phoff, eh.e_phnum * sizeof *ph);
		for (int k = 0; k < eh.e_phnum; k++) {   /* a non-LOAD segment's file span: its sections' (GNU recomputes it) */
			if (ph[k].p_type == PT_GNU_RELRO) {   /* its size is kept, but its file part ends with its PT_LOAD's */
				for (int l = 0; l < eh.e_phnum; l++)
					if (ph[l].p_type == PT_LOAD && ph[k].p_offset >= ph[l].p_offset && ph[k].p_offset - ph[l].p_offset <= ph[l].p_filesz
					    && ph[k].p_vaddr >= ph[l].p_vaddr && ph[k].p_vaddr - ph[l].p_vaddr < ph[l].p_memsz) {
						u32 room = ph[l].p_filesz - (ph[k].p_offset - ph[l].p_offset);
						if (ph[k].p_filesz > room) ph[k].p_filesz = room;
						break;
					}
				continue;
			}
			if (ph[k].p_type == PT_LOAD || ph[k].p_type == PT_GNU_STACK) continue;
			int first = 0, last = 0;
			for (int i = 1; i < nos; i++) {
				int j = os[i].in;
				if (j <= 0 || !in_segment(&f.sh[j], &ph[k])) continue;
				if (!first || f.sh[j].sh_offset < f.sh[first].sh_offset) first = j;
				if (!last || f.sh[j].sh_offset >= f.sh[last].sh_offset) last = j;
			}
			if (!first) continue;
			ph[k].p_offset = f.sh[first].sh_offset;
			ph[k].p_filesz = f.sh[last].sh_offset - f.sh[first].sh_offset + (f.sh[last].sh_type == SHT_NOBITS ? 0 : f.sh[last].sh_size);
		}
	}
	for (int i = 1; i < nos; i++) if (sh[i].sh_type != SHT_NOBITS && sh[i].sh_size) memcpy(b + sh[i].sh_offset, body[i], sh[i].sh_size);
	memcpy(b + shoff, sh, nos * sizeof(Elf32_Shdr));
	emit(b, total);
	free(sh); free(body); for (int i = 0; i < nos; i++) free(owned[i]); free(owned);
	free(os); free(newidx); free(nmi); free(shs.e); strmap_clear(&shs.at); free(shs_data); free(sts.e); strmap_clear(&sts.at); free(strdata); free(symdata);
	free(used); free(ksym); free(newsym); free(osym); free(oname); free(hassec); free(relof); free(relsec); free(keep); free(data);
}

/* ---- the command lines ---------------------------------------------------------------------------------------- */
static u32 num(const char *s) { char *e; unsigned long v = strtoul(s, &e, 0); if (*e) die("bad number '%s'", s); return (u32)v; }
static void push_sym(const char ***l, int *n, const char *s) { *l = realloc(*l, (*n + 1) * sizeof **l); (*l)[(*n)++] = s; }
static const char *optval(int argc, char **argv, int *i, const char *a, const char *lng, const char *shrt) {
	size_t L = strlen(lng);   /* --long=V, --long V, -s V, -sV */
	if (!strncmp(a, lng, L) && a[L] == '=') return a + L + 1;
	if (!strcmp(a, lng) || (shrt && !strcmp(a, shrt))) { if (*i + 1 >= argc) die("option '%s' needs an argument", a); return argv[++*i]; }
	if (shrt && !strncmp(a, shrt, strlen(shrt)) && a[strlen(shrt)]) return a + strlen(shrt);
	return NULL;
}
/* Where the result goes, as GNU does it: back INTO the input file when it is rewritten in place (its inode, owner and
 * mode stay); else a new file (0666 & ~umask, plus the execute bits for an executable or shared object — BFD's chmod
 * in bfd_close, -O binary included). -p: the input's access and modification times. */
static void finish(const char *in, const char *out) {
	struct stat st; int have = !stat(in, &st);
	int inplace = !out || !strcmp(in, out);
	if (!inplace) { struct stat o; if (!lstat(out, &o) && S_ISREG(o.st_mode) && o.st_size) unlink(out); }
	const char *dst = inplace ? in : out;
	FILE *fp = fopen(dst, "wb"); if (!fp) die("%s: cannot create", dst);
	if (fwrite(obuf, 1, olen, fp) != olen || fclose(fp)) die("%s: write failed", dst);
	if (!inplace && out_exec) {
		mode_t m = umask(0); umask(m);
		struct stat o; if (!stat(dst, &o)) chmod(dst, 0777 & (o.st_mode | (0111 & ~m)));
	}
	if (preserve_dates && have) { struct utimbuf ut = { st.st_atime, st.st_mtime }; utime(dst, &ut); }
	free(obuf); obuf = NULL;
}
/* Shared by objcopy and strip; 0 = not one of these. */
static int common_opt(int argc, char **argv, int *i) {
	const char *a = argv[*i], *v;
	if ((v = optval(argc, argv, i, a, "--remove-section", "-R"))) add_pat(v, 'R');
	else if ((v = optval(argc, argv, i, a, "--remove-relocations", NULL))) add_pat(v, 'r');
	else if ((v = optval(argc, argv, i, a, "--keep-symbol", "-K"))) push_sym(&keep_syms, &nkeep, v);
	else if ((v = optval(argc, argv, i, a, "--strip-symbol", "-N"))) push_sym(&strip_syms, &nstripsym, v);
	else if (!strcmp(a, "-S") || !strcmp(a, "--strip-all")) strip_symbols = STRIP_ALL;
	else if (!strcmp(a, "-g") || !strcmp(a, "-d") || !strcmp(a, "--strip-debug")) strip_symbols = STRIP_DEBUG;
	else if (!strcmp(a, "--strip-unneeded")) strip_symbols = STRIP_UNNEEDED;
	else if (!strcmp(a, "-x") || !strcmp(a, "--discard-all")) discard_locals = LOCALS_ALL;
	else if (!strcmp(a, "-X") || !strcmp(a, "--discard-locals")) discard_locals = LOCALS_START_L;
	else if (!strcmp(a, "-p") || !strcmp(a, "--preserve-dates")) preserve_dates = 1;
	else if (!strcmp(a, "-D") || !strcmp(a, "--enable-deterministic-archives") || !strcmp(a, "-v") || !strcmp(a, "--verbose")) ;   /* (archives: none; nothing to report) */
	else return 0;
	return 1;
}
static void check_target(const char *t, int out) {
	if (out && !strcmp(t, "binary")) { out_binary = 1; return; }
	if (!strcmp(t, "elf32-littlearm") || !strcmp(t, "elf32-little")) return;
	die("target '%s': unsupported (elf32-littlearm, elf32-little%s)", t, out ? ", binary" : "");
}

int main(int argc, char **argv) {
	const char *base = strrchr(argv[0], '/'); base = base ? base + 1 : argv[0];
	size_t bl = strlen(base);
	int strip = bl >= 5 && !strcmp(base + bl - 5, "strip");
	const char *out = NULL, **files = NULL; int nfiles = 0;
	if (strip) { tool = "strip"; strip_symbols = STRIP_ALL; }
	for (int i = 1; i < argc; i++) {
		const char *a = argv[i], *v;
		if (a[0] != '-' || !a[1]) { push_sym(&files, &nfiles, a); continue; }
		if (common_opt(argc, argv, &i)) continue;
		if (strip) {
			if (!strcmp(a, "-s")) strip_symbols = STRIP_ALL;
			else if ((v = optval(argc, argv, &i, a, "--output-file", "-o"))) out = v;
			else if (!strcmp(a, "--version") || !strcmp(a, "-V")) { printf("strip (the os toolchain) — GNU strip compatible\n"); return 0; }
			else die("unrecognized option '%s'", a);
			continue;
		}
		if ((v = optval(argc, argv, &i, a, "--only-section", "-j"))) add_pat(v, 'j');
		else if ((v = optval(argc, argv, &i, a, "--rename-section", NULL))) {
			const char *eq = strchr(v, '='); if (!eq || eq == v || !eq[1]) die("bad format for --rename-section: argument needed");
			const char *cm = strchr(eq + 1, ',');
			renames = realloc(renames, (nrename + 1) * sizeof *renames);
			renames[nrename].from = strndup(v, eq - v); renames[nrename].to = cm ? strndup(eq + 1, cm - eq - 1) : eq + 1;
			renames[nrename++].flags = cm ? parse_flags(cm + 1) : -1;
		}
		else if ((v = optval(argc, argv, &i, a, "--set-section-flags", NULL))) {
			const char *eq = strchr(v, '='); if (!eq || eq == v) die("bad format for --set-section-flags: argument needed");
			setflags = realloc(setflags, (nsetflags + 1) * sizeof *setflags);
			setflags[nsetflags].pat = strndup(v, eq - v); setflags[nsetflags++].flags = parse_flags(eq + 1);
		}
		else if ((v = optval(argc, argv, &i, a, "--output-target", "-O"))) check_target(v, 1);
		else if ((v = optval(argc, argv, &i, a, "--input-target", "-I"))) check_target(v, 0);
		else if ((v = optval(argc, argv, &i, a, "--target", "-F"))) { check_target(v, 1); }
		else if ((v = optval(argc, argv, &i, a, "--binary-architecture", "-B"))) { if (strcmp(v, "arm")) die("-B %s: unsupported", v); }
		else if ((v = optval(argc, argv, &i, a, "--gap-fill", "-i"))) { gap_fill = (u8)num(v); gap_fill_set = 1; }
		else if ((v = optval(argc, argv, &i, a, "--pad-to", NULL))) { pad_to = num(v); pad_to_set = 1; }
		else if (!strcmp(a, "--version") || !strcmp(a, "-V")) { printf("objcopy (the os toolchain) — GNU objcopy compatible\n"); return 0; }
		else die("unrecognized option '%s'", a);
	}
	if (pad_to_set && !out_binary) die("--pad-to: only with -O binary");
	if (gap_fill_set && !out_binary) die("--gap-fill: only with -O binary");
	if (strip) {
		if (!nfiles) die("no input file");
		if (out && nfiles > 1) die("-o with more than one input");
		for (int k = 0; k < nfiles; k++) { copy_elf(files[k]); finish(files[k], out); }
		return 0;
	}
	if (nfiles < 1 || nfiles > 2) die("usage: objcopy [options] in [out]");
	copy_elf(files[0]);
	finish(files[0], nfiles == 2 ? files[1] : NULL);
	return 0;
}
