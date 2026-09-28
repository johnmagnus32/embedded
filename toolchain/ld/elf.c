/*
 * elf.c — the OBJECT-FORMAT backend: read ELF32 inputs (relocatable objects, archive members through the archive
 * index, shared objects as providers) into the Obj model, and serialize the output image. Everything that knows
 * the on-disk ELF layout lives here; the front-end operates on the parsed Obj, the arch backend never touches it.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "ld.h"
#include "elfread.h"

ShLib *shlibs; int nshlib; static int shlibcap;             /* -l: the shared libraries we link against */
ShExport *shexports; int nshexport; static int shexpcap;    /* the symbols those providers export */

/* Parse an in-memory ELF32 relocatable into objs[] (validated by elf_open; the image must be 4-aligned). `active`
 * distinguishes always-linked command-line objects (1) from lazy archive members (0, pulled on demand). */
static Obj *elf_parse(const char *path, u8 *data, long size, int active) {
	ElfFile f; elf_open(&f, path, data, size, ET_REL, md_e_machine);
	Obj *o = obj_new(); o->path = path; o->data = data; o->size = size; o->active = active;
	o->eh = f.eh; o->sh = f.sh; o->nsh = f.nsh; o->shstr = f.shstr; o->sym = f.sym; o->nsym = f.nsym; o->strtab = f.strtab;
	o->sec_vaddr = calloc(o->nsh, sizeof(u32));
	o->sec_lma   = calloc(o->nsh, sizeof(u32));
	o->sec_out   = calloc(o->nsh, sizeof *o->sec_out);
	return o;
}

/* Read a whole file into a fresh buffer. */
static u8 *slurp(const char *path, long *size) {
	FILE *f = fopen(path, "rb"); if (!f) die("cannot open %s", path);
	fseek(f, 0, SEEK_END); *size = ftell(f); fseek(f, 0, SEEK_SET);
	u8 *b = malloc(*size); if (fread(b, 1, *size, f) != (size_t)*size) die("%s: read failed", path);
	fclose(f); return b;
}

/* Parse one relocatable object FILE (always active). */
Obj *elf_load(const char *path) { long n; u8 *d = slurp(path, &n); return elf_parse(path, d, n, 1); }

/* An archive (`!<arch>\n`, GNU/SysV format) is used through its SYMBOL INDEX — the "/" member listing, for each
 * global symbol a member defines, that member's header offset (big-endian counts/offsets). Members are parsed
 * only when a symbol they define is needed (ar_pull), so a 1400-member libc.a costs a few pulled members, not
 * 1400 parses. "//" holds long member names ("/off" references it). An archive without an index is an error,
 * as in GNU ld ("run ranlib"). */
typedef struct { const char *path; u8 *data; long size; const char *longtab; long longsz; StrMap loaded; } Archive;
typedef struct { Archive *ar; long off; } ArSym;
static StrMap arsyms;                                  /* symbol -> its first defining member (command-line order) */

static long ar_field(const char *h, int at, int len) { char b[16]; memcpy(b, h + at, len); b[len] = 0; return strtol(b, NULL, 10); }
static u32 be32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }

void ar_load(const char *path) {
	long size; u8 *d = slurp(path, &size);
	if (size < 8 || memcmp(d, "!<arch>\n", 8)) die("%s: not an archive", path);
	Archive *ar = calloc(1, sizeof *ar); ar->path = path; ar->data = d; ar->size = size;
	const u8 *index = NULL; long index_size = 0;
	for (long p = 8; p + 60 <= size; ) {
		const char *h = (const char *)(d + p);
		long msize = ar_field(h, 48, 10);
		if (memcmp(h + 58, "`\n", 2) || msize < 0 || p + 60 + msize > size) die("%s: corrupt archive member header at %ld", path, p);
		if (!memcmp(h, "/ ", 2)) { index = d + p + 60; index_size = msize; }
		else if (!memcmp(h, "//", 2)) { ar->longtab = (const char *)(d + p + 60); ar->longsz = msize; }
		else if (!memcmp(h, "/SYM64/", 7) || !memcmp(h, "__.SYMDEF", 9)) die("%s: unsupported archive index format", path);
		p += 60 + msize + (msize & 1);                 /* members are padded to even length */
	}
	if (!index) die("%s: archive has no index; run ranlib (or ar s) to add one", path);
	if (index_size < 4) die("%s: corrupt archive index", path);
	u32 n = be32(index);
	if (4 + 4 * (long)n > index_size) die("%s: corrupt archive index", path);
	const char *names = (const char *)index + 4 + 4 * n, *end = (const char *)index + index_size;
	for (u32 i = 0; i < n; i++) {
		if (names >= end) die("%s: corrupt archive index names", path);
		ArSym *as = malloc(sizeof *as); as->ar = ar; as->off = (long)be32(index + 4 + 4 * i);
		if (as->off < 8 || as->off + 60 > size) die("%s: archive index points outside the file", path);
		if (!strmap_get(&arsyms, names)) strmap_put(&arsyms, names, as);   /* the first archive defining it wins */
		names += strlen(names) + 1;
	}
}

Obj *ar_pull(const char *sym) {
	ArSym *as = strmap_get(&arsyms, sym);
	if (!as) return NULL;
	Archive *ar = as->ar;
	char key[24]; snprintf(key, sizeof key, "%ld", as->off);
	if (strmap_get(&ar->loaded, key)) return NULL;     /* that member is already in (its definition didn't take) */
	strmap_put(&ar->loaded, strdup(key), (void *)1);
	const char *h = (const char *)(ar->data + as->off);
	char mname[256]; int k = 0;
	if (h[0] == '/' && h[1] >= '0' && h[1] <= '9') {   /* "/off": long name */
		long at = atol(h + 1);
		if (!ar->longtab || at < 0 || at >= ar->longsz) die("%s: member name outside the // table", ar->path);
		const char *nm = ar->longtab + at;
		while (k < 255 && at + k < ar->longsz && nm[k] != '/' && nm[k] != '\n') { mname[k] = nm[k]; k++; }
	} else while (k < 16 && h[k] != '/' && h[k] != ' ') { mname[k] = h[k]; k++; }
	mname[k] = 0;
	char *path = malloc(strlen(ar->path) + strlen(mname) + 3); sprintf(path, "%s(%s)", ar->path, mname);
	long size = ar_field(h, 48, 10);                   /* the header was bounds-checked by ar_load */
	u8 *copy = malloc(size ? size : 1); memcpy(copy, ar->data + as->off + 60, size);   /* an aligned image of its own */
	return elf_parse(path, copy, size, 1);
}

/* Read a shared library (-l) as a PROVIDER: register the symbols it EXPORTS + its soname, without laying
 * out any of its sections. We read its .dynsym (SHT_DYNSYM) + linked .dynstr for the global/weak DEFINED
 * names, and its DT_SONAME (falling back to the file's basename) for the DT_NEEDED we'll emit if used. */
void load_shared(const char *path) {
	long size; u8 *d = slurp(path, &size);
	ElfFile f; elf_open(&f, path, d, size, ET_DYN, md_e_machine);
	int dynsym = 0, dynamic = 0;
	for (int i = 1; i < f.nsh; i++) {
		if (f.sh[i].sh_type == SHT_DYNSYM) dynsym = i;
		else if (f.sh[i].sh_type == SHT_DYNAMIC) dynamic = i;
	}
	if (!dynsym) die("%s: no .dynsym (not a linkable shared object)", path);
	int dstr = (int)f.sh[dynsym].sh_link;               /* validated: a NUL-terminated string table */
	const char *son = NULL;
	if (dynamic) {
		Elf32_Shdr *s = &f.sh[dynamic];
		if (s->sh_offset & 3 || s->sh_size % sizeof(Elf32_Dyn)) die("%s: malformed .dynamic", path);
		Elf32_Dyn *dyn = (Elf32_Dyn *)(d + s->sh_offset);
		for (u32 i = 0; i < s->sh_size / sizeof *dyn && dyn[i].d_tag != DT_NULL; i++)
			if (dyn[i].d_tag == DT_SONAME) son = elf_string(&f, dstr, dyn[i].d_val);
	}
	if (!son) { const char *b = strrchr(path, '/'); son = strdup(b ? b + 1 : path); }
	shlibs = grow(shlibs, nshlib, &shlibcap, sizeof *shlibs);
	int lib = nshlib; shlibs[nshlib++] = (ShLib){ son, 0 };

	Elf32_Sym *dsym = (Elf32_Sym *)(d + f.sh[dynsym].sh_offset);
	for (u32 k = 0; k < f.sh[dynsym].sh_size / sizeof *dsym; k++) {
		Elf32_Sym *s = &dsym[k]; int b = ELF32_ST_BIND(s->st_info);
		if ((b == STB_GLOBAL || b == STB_WEAK) && s->st_shndx != SHN_UNDEF && s->st_name) {
			shexports = grow(shexports, nshexport, &shexpcap, sizeof *shexports);
			shexports[nshexport++] = (ShExport){ elf_string(&f, dstr, s->st_name), lib, s->st_size, ELF32_ST_TYPE(s->st_info) };
		}
	}
}

/* ---- the output image ----------------------------------------------------------------------------------- */
/* The whole file is built in memory, then written once:
 *   ELF header | program headers | the segments' bytes (each output section at its file offset) |
 *   .symtab | .strtab | .shstrtab | section headers
 * Layout (script.c) decided every address and file offset; this only serializes. */
static Elf32_Sym *syms; static int nsyms, symcap; static Strtab strtab;
static void add_sym(const char *name, u32 value, u32 size, u8 info, u8 other, u32 shndx) {
	syms = grow(syms, nsyms, &symcap, sizeof *syms);
	syms[nsyms++] = (Elf32_Sym){ .st_name = name ? str_add(&strtab, name) : 0, .st_value = value, .st_size = size,
	                             .st_info = info, .st_other = other, .st_shndx = (u16)shndx };
}
static u32 out_shndx(const Obj *o, u32 shndx) {        /* an input section's output index (SHN_ABS if not emitted) */
	if (shndx == SHN_ABS || shndx >= 0xff00) return SHN_ABS;
	OutSec *os = o->sec_out[shndx];
	return os && os != &os_discard && os->index ? (u32)os->index : SHN_ABS;
}
static int winner(const Obj *o, int k) {               /* this global is the definition the link uses */
	GSym *g = gsym_find(o->strtab + o->sym[k].st_name);
	return g && g->defined && g->obj == o && g->symidx == k;
}
static u32 near_shndx(const GSym *g) {                 /* a script symbol: its section, else the nearest one below it */
	if (g->abs) return SHN_ABS;
	if (g->os && g->os->index) return (u32)g->os->index;
	int best = noutsec ? 1 : SHN_ABS;
	for (int i = 0; i < noutsec; i++) if (outsecs[i]->vaddr <= g->vaddr) best = outsecs[i]->index;
	return (u32)best;
}
static int by_name(const void *a, const void *b) { return strcmp((*(GSym *const *)a)->name, (*(GSym *const *)b)->name); }
/* One half of .symtab: the LOCAL symbols (each object's, plus hidden globals made local, as GNU ld does) or the
 * GLOBAL ones — object definitions in command-line order, then linker-script symbols (and _DYNAMIC) by name. */
static void add_syms(int locals) {
	for (int i = 0; i < nobj; i++) {
		Obj *o = objs[i];
		if (!o->active || o == linker_obj) continue;
		for (int k = 1; k < o->nsym; k++) {
			Elf32_Sym *s = &o->sym[k]; int b = ELF32_ST_BIND(s->st_info), t = ELF32_ST_TYPE(s->st_info);
			int hidden = ELF32_ST_VISIBILITY(s->st_other) != STV_DEFAULT;
			if (!s->st_name || t == STT_SECTION || s->st_shndx == SHN_UNDEF) continue;
			if (b == STB_LOCAL ? !locals : (!winner(o, k) || hidden != locals)) continue;
			if (t == STT_FILE) { add_sym(o->strtab + s->st_name, 0, 0, s->st_info, 0, SHN_ABS); continue; }
			if (s->st_shndx != SHN_ABS && (!o->sec_out[s->st_shndx] || o->sec_out[s->st_shndx] == &os_discard)) continue;
			add_sym(o->strtab + s->st_name, sym_addr(o, k), s->st_size, locals ? ELF32_ST_INFO(STB_LOCAL, t) : s->st_info,
			        s->st_other, out_shndx(o, s->st_shndx));
		}
	}
	GSym **ls = malloc((ndeclared + 1) * sizeof *ls); int n = 0;
	for (int i = 0; i < ndeclared; i++) if (declared[i]->defined && !declared[i]->obj && !!declared[i]->hidden == locals) ls[n++] = declared[i];
	GSym *d = gsym_find("_DYNAMIC");
	if (locals && d && d->defined && !d->obj) ls[n++] = d;                /* hidden, as in GNU ld */
	qsort(ls, n, sizeof *ls, by_name);
	for (int i = 0; i < n; i++)
		add_sym(ls[i]->name, ls[i]->vaddr, 0, ELF32_ST_INFO(locals ? STB_LOCAL : STB_GLOBAL, STT_NOTYPE), ls[i]->hidden ? STV_HIDDEN : 0, near_shndx(ls[i]));
	free(ls);
}

void elf_write(const char *out, u32 entry) {
	str_add(&strtab, ""); add_sym(NULL, 0, 0, 0, 0, SHN_UNDEF);
	add_syms(1);
	int first_global = nsyms;
	add_syms(0);

	Strtab shstr = {0}; str_add(&shstr, "");
	u32 content = hdrsz;                                 /* end of the loadable bytes in the file */
	for (int i = 0; i < noutsec; i++) if (outsecs[i]->type != SHT_NOBITS && outsecs[i]->off + outsecs[i]->size > content)
		content = outsecs[i]->off + outsecs[i]->size;
	u32 symoff = alignup(content, 4), stroff = symoff + (u32)nsyms * sizeof(Elf32_Sym), shstroff = stroff + (u32)strtab.len;
	int nsh = 1 + noutsec + 3, isym = 1 + noutsec;       /* null, output sections, .symtab .strtab .shstrtab */
	Elf32_Shdr *sh = calloc(nsh, sizeof *sh);
	for (int i = 0; i < noutsec; i++) {
		OutSec *os = outsecs[i]; Obj *fo = os->first.obj; Elf32_Shdr *fs = fo ? &fo->sh[os->first.shndx] : NULL;
		u32 align = 1;
		for (int k = 0; k < nobj; k++) for (int j = 1; j < objs[k]->nsh; j++)
			if (objs[k]->sec_out && objs[k]->sec_out[j] == os && objs[k]->sh[j].sh_addralign > align) align = objs[k]->sh[j].sh_addralign;
		sh[1 + i] = (Elf32_Shdr){ .sh_name = str_add(&shstr, os->name), .sh_type = os->type, .sh_flags = os->flags,
		    .sh_addr = os->vaddr, .sh_offset = os->off, .sh_size = os->size, .sh_addralign = align, .sh_entsize = os->entsize,
		    .sh_link = fs && fs->sh_link ? out_shndx(fo, fs->sh_link) % SHN_ABS : 0,
		    .sh_info = !fs ? 0 : os->type == SHT_REL ? (fs->sh_info ? out_shndx(fo, fs->sh_info) % SHN_ABS : 0) : os->type == SHT_DYNSYM ? fs->sh_info : 0 };
	}
	sh[isym]     = (Elf32_Shdr){ .sh_name = str_add(&shstr, ".symtab"), .sh_type = SHT_SYMTAB, .sh_offset = symoff,
	    .sh_size = (u32)nsyms * sizeof(Elf32_Sym), .sh_link = isym + 1, .sh_info = first_global, .sh_addralign = 4, .sh_entsize = sizeof(Elf32_Sym) };
	sh[isym + 1] = (Elf32_Shdr){ .sh_name = str_add(&shstr, ".strtab"), .sh_type = SHT_STRTAB, .sh_offset = stroff, .sh_size = (u32)strtab.len, .sh_addralign = 1 };
	sh[isym + 2] = (Elf32_Shdr){ .sh_name = str_add(&shstr, ".shstrtab"), .sh_type = SHT_STRTAB, .sh_offset = shstroff, .sh_addralign = 1 };
	sh[isym + 2].sh_size = (u32)shstr.len;
	u32 shoff = alignup(shstroff + (u32)shstr.len, 4), total = shoff + (u32)nsh * sizeof(Elf32_Shdr);

	u8 *buf = calloc(1, total);
	if (sizeof(Elf32_Ehdr) + (u32)nphdr * sizeof(Elf32_Phdr) != hdrsz) die("internal: program header table size changed after layout");
	Elf32_Ehdr eh = { .e_type = (pie || shared) ? ET_DYN : ET_EXEC, .e_machine = md_e_machine, .e_version = 1, .e_entry = entry,
	    .e_phoff = nphdr ? sizeof(Elf32_Ehdr) : 0, .e_shoff = shoff, .e_flags = 0x05000000 /* EABI5 */, .e_ehsize = sizeof(Elf32_Ehdr),
	    .e_phentsize = sizeof(Elf32_Phdr), .e_phnum = (u16)nphdr, .e_shentsize = sizeof(Elf32_Shdr), .e_shnum = (u16)nsh, .e_shstrndx = (u16)(isym + 2) };
	memcpy(eh.e_ident, "\177ELF\1\1\1", 7);              /* MAG + ELFCLASS32 + ELFDATA2LSB + EV_CURRENT */
	memcpy(buf, &eh, sizeof eh);
	memcpy(buf + sizeof eh, phdrs, (size_t)nphdr * sizeof(Elf32_Phdr));
	for (int i = 0; i < nobj; i++) {                     /* every placed input section's (relocated) bytes */
		Obj *o = objs[i];
		if (!o->active) continue;
		for (int j = 1; j < o->nsh; j++) {
			OutSec *os = o->sec_out[j];
			if (!os || os == &os_discard || !os->index || os->type == SHT_NOBITS || o->sh[j].sh_type == SHT_NOBITS) continue;
			memcpy(buf + os->off + (o->sec_vaddr[j] - os->vaddr), o->data + o->sh[j].sh_offset, o->sh[j].sh_size);
		}
	}
	memcpy(buf + symoff, syms, (size_t)nsyms * sizeof *syms);
	memcpy(buf + stroff, strtab.b, strtab.len);
	memcpy(buf + shstroff, shstr.b, shstr.len);
	memcpy(buf + shoff, sh, (size_t)nsh * sizeof *sh);

	FILE *f = fopen(out, "wb"); if (!f) die("cannot open %s", out);
	if (fwrite(buf, 1, total, f) != total || fclose(f)) die("%s: write failed", out);
	mode_t m = umask(0); umask(m); chmod(out, 0777 & ~m);   /* executable, like GNU ld's output */
}
