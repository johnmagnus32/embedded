/*
 * nm.c — our from-scratch symbol lister, the GNU `nm` equivalent (its BSD output, byte for byte): for each symbol of an
 * ELF32 object, executable or shared object — or of each member of an archive — "VALUE TYPE NAME", sorted by name.
 *
 * TYPE is GNU's (bfd_decode_symclass), decided from the symbol and the section it is in:
 *   U undefined, w / v undefined weak (v: an object), W / V defined weak, C common (its VALUE is its size), A / a
 *   absolute, i an STT_GNU_IFUNC, u STB_GNU_UNIQUE; else from the section — first by name (.drectve/.idata i, .edata e,
 *   .pdata p: GNU's COFF table), then by its flags: t code, r read-only data, d data, b no contents (bss), N debug
 *   (.debug*, .zdebug*, .line*, .stab*, …), n other read-only, ? anything else. Upper case = global, lower = local.
 * Hidden like GNU: section and file symbols (unless -a), ARM mapping/tag symbols "$a", "$d.foo", … (unless
 * --special-syms). Names collate with strcoll (LC_COLLATE, as GNU — the kernel build sets it to C); ties keep the
 * symbol-table order.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <locale.h>
#include "elfread.h"
#include "arread.h"

void die(const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	fputs("nm: ", stderr); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap); exit(1);
}

#define SHT_GNU_versym 0x6fffffffu
#define STB_GNU_UNIQUE 10
#define STT_GNU_IFUNC  10
#define STT_COMMON     5

static int all_syms, per_symbol_name, dynamic, extern_only, numeric, no_sort, reverse, print_size, size_sort,
           undefined_only, defined_only, special_syms;
static int per_file_header;                     /* more than one file named: "\nFILE:\n" before each */

typedef struct { const char *name; u32 value, size; char type; int ord; } Sym;

/* BFD's section flags for an ELF section, then its symbol letter (decode_section_type). */
static int starts(const char *s, const char *p) { return !strncmp(s, p, strlen(p)); }
static char section_letter(const ElfFile *f, int j) {
	const Elf32_Shdr *s = &f->sh[j]; const char *nm = f->shstr + s->sh_name;
	static const struct { const char *pfx; char c; } coff[] = { {".drectve", 'i'}, {".edata", 'e'}, {".idata", 'i'}, {".pdata", 'p'} };
	for (unsigned k = 0; k < sizeof coff / sizeof *coff; k++) if (starts(nm, coff[k].pfx)) return coff[k].c;
	int contents = s->sh_type != SHT_NOBITS, alloc = s->sh_flags & SHF_ALLOC, load = alloc && contents;
	int ro = !(s->sh_flags & SHF_WRITE), code = s->sh_flags & SHF_EXECINSTR, data = !code && load;
	int debug = !alloc && nm[0] == '.' && (starts(nm, ".debug") || starts(nm, ".gnu.debuglto_.debug_") || starts(nm, ".gnu.linkonce.wi.")
	            || starts(nm, ".zdebug") || starts(nm, ".line") || starts(nm, ".stab") || !strcmp(nm, ".gdb_index"));
	if (code) return 't';
	if (data) return ro ? 'r' : 'd';
	if (!contents) return 'b';
	if (debug) return 'N';
	if (ro) return 'n';
	return '?';
}
static int arm_special(const char *n) {   /* bfd_is_arm_special_symbol_name(ANY): "$" + a lower-case letter, then end or "." */
	return n[0] == '$' && n[1] >= 'a' && n[1] <= 'z' && (n[2] == 0 || n[2] == '.');
}
static char sym_letter(const ElfFile *f, const Elf32_Sym *s) {
	int bind = ELF32_ST_BIND(s->st_info), type = ELF32_ST_TYPE(s->st_info), obj = type == STT_OBJECT || type == STT_COMMON;
	if (s->st_shndx == SHN_COMMON) return 'C';
	if (s->st_shndx == SHN_UNDEF) return bind == STB_WEAK ? (obj ? 'v' : 'w') : 'U';
	if (type == STT_GNU_IFUNC) return 'i';
	if (bind == STB_WEAK) return obj ? 'V' : 'W';
	if (bind == STB_GNU_UNIQUE) return 'u';
	if (bind != STB_GLOBAL && bind != STB_LOCAL) return '?';
	char c = s->st_shndx == SHN_ABS ? 'a' : s->st_shndx < f->nsh ? section_letter(f, s->st_shndx) : '?';
	if (s->st_shndx != SHN_ABS && s->st_shndx >= f->nsh) return '?';
	return bind == STB_GLOBAL && c >= 'a' && c <= 'z' ? (char)(c - 'a' + 'A') : c;
}

static int cmp_name(const Sym *a, const Sym *b) { return strcoll(a->name, b->name); }
static int cmp_num(const Sym *a, const Sym *b) {   /* numeric_forward: undefined first, then by value, then by name */
	int ua = a->type == 'U' || a->type == 'w' || a->type == 'v', ub = b->type == 'U' || b->type == 'w' || b->type == 'v';
	if (ua != ub) return ua ? -1 : 1;
	if (!ua && a->value != b->value) return a->value < b->value ? -1 : 1;
	return cmp_name(a, b);
}
static int cmp_size(const Sym *a, const Sym *b) {   /* by size, then name; then by value (GNU's first pass, size_forward1) */
	if (a->size != b->size) return a->size < b->size ? -1 : 1;
	int c = cmp_name(a, b);
	return c ? c : a->value != b->value ? (a->value < b->value ? -1 : 1) : 0;
}
static int (*cmp)(const Sym *, const Sym *);
static int cmp_dir(const Sym *a, const Sym *b) { int c = cmp(a, b); return reverse ? -c : c; }
static void msort(Sym *v, Sym *tmp, int n) {   /* stable: equal symbols keep their table order (as glibc's merge qsort) */
	if (n < 2) return;
	int h = n / 2; msort(v, tmp, h); msort(v + h, tmp, n - h);
	int i = 0, j = h, k = 0;
	while (i < h && j < n) tmp[k++] = cmp_dir(&v[j], &v[i]) < 0 ? v[j++] : v[i++];
	while (i < h) tmp[k++] = v[i++];
	while (j < n) tmp[k++] = v[j++];
	memcpy(v, tmp, n * sizeof *v);
}

static int is_undef(char t) { return t == 'U' || t == 'w' || t == 'v'; }
/* One ELF image: its symbols, filtered, sorted, printed. `arname`: the archive it came from (for -A). */
static void list(const char *path, u8 *data, long size, const char *arname) {
	if (size < 18 || memcmp(data, "\177ELF", 4)) die("%s: file format not recognized", path);
	ElfFile f; elf_open(&f, path, data, size, data[16] | data[17] << 8, 0);
	int tab = 0;
	for (int i = 1; i < f.nsh; i++) {
		if (f.sh[i].sh_type == (dynamic ? SHT_DYNSYM : SHT_SYMTAB)) tab = i;
		if (dynamic && f.sh[i].sh_type == SHT_GNU_versym) die("%s: symbol versions: unsupported", path);
	}
	int n = tab ? (int)(f.sh[tab].sh_size / sizeof(Elf32_Sym)) - 1 : 0;
	if (n <= 0) { fprintf(stderr, "nm: %s: no symbols\n", path); return; }
	const Elf32_Sym *st = (const Elf32_Sym *)(data + f.sh[tab].sh_offset) + 1;
	const char *str = (const char *)data + f.sh[f.sh[tab].sh_link].sh_offset;
	Sym *v = malloc(n * sizeof *v), *tmp = malloc(n * sizeof *v); int m = 0;
	for (int k = 0; k < n; k++) {
		const Elf32_Sym *s = &st[k]; int type = ELF32_ST_TYPE(s->st_info), bind = ELF32_ST_BIND(s->st_info);
		Sym y = { str + s->st_name, s->st_shndx == SHN_COMMON ? s->st_size : s->st_value, s->st_size, sym_letter(&f, s), k };
		if (type == STT_SECTION) y.name = s->st_shndx < f.nsh ? f.shstr + f.sh[s->st_shndx].sh_name : "";   /* BFD names it by its section */
		if (!all_syms && (type == STT_SECTION || type == STT_FILE)) continue;   /* BSF_DEBUGGING */
		if (undefined_only && s->st_shndx != SHN_UNDEF) continue;
		if (extern_only && bind != STB_GLOBAL && bind != STB_WEAK && bind != STB_GNU_UNIQUE && s->st_shndx != SHN_UNDEF && s->st_shndx != SHN_COMMON) continue;
		if (size_sort && (s->st_shndx == SHN_ABS || s->st_shndx == SHN_UNDEF)) continue;
		if (defined_only && s->st_shndx == SHN_UNDEF) continue;
		if (!special_syms && arm_special(y.name)) continue;
		if (size_sort && !s->st_size) continue;
		v[m++] = y;
	}
	if (!no_sort) { cmp = size_sort ? cmp_size : numeric ? cmp_num : cmp_name; msort(v, tmp, m); }
	for (int k = 0; k < m; k++) {
		if (per_symbol_name) { if (arname) printf("%s:", arname); printf("%s:", path); }
		if (is_undef(v[k].type)) printf("        ");
		else {
			printf("%08x", size_sort && !print_size ? v[k].size : v[k].value);
			if (print_size && v[k].size) printf(" %08x", v[k].size);
		}
		printf(" %c %s\n", v[k].type, v[k].name);
	}
	free(v); free(tmp);
}

static u8 *slurp(const char *path, long *size) {
	FILE *fp = fopen(path, "rb"); if (!fp) die("'%s': No such file", path);
	fseek(fp, 0, SEEK_END); *size = ftell(fp); fseek(fp, 0, SEEK_SET);
	u8 *b = malloc(*size ? *size : 1);
	if (fread(b, 1, *size, fp) != (size_t)*size) die("%s: read failed", path);
	fclose(fp); return b;
}

static void nm_file(const char *path) {
	long size; u8 *d = slurp(path, &size);
	if (!ar_is_archive(d, size)) {
		if (per_file_header && !per_symbol_name) printf("\n%s:\n", path);
		list(path, d, size, NULL);
		return;
	}
	Archive ar; ar_open(&ar, path, d, size);
	if (per_file_header) printf("\n%s:\n", path);   /* (even with -A, as GNU) */
	ArMember m; long pos = 8;
	while (ar_next(&ar, &pos, &m)) {
		char *mpath = ar.thin ? ar_member_path(&ar, &m) : strdup(m.name);
		long msize = m.size; u8 *img;
		if (ar.thin) { img = slurp(mpath, &msize); if (msize != m.size) die("%s: thin member %s changed size since the archive was made", path, mpath); }
		else { img = malloc(msize ? msize : 1); memcpy(img, m.data, msize); }   /* an aligned copy */
		if (msize < 4 || memcmp(img, "\177ELF", 4)) die("%s: %s: file format not recognized", path, mpath);
		if (!per_symbol_name) printf("\n%s:\n", mpath);
		list(mpath, img, msize, path);
		free(img); free(mpath);
	}
}

int main(int argc, char **argv) {
	setlocale(LC_COLLATE, "");   /* as GNU nm: names sort in the locale's collation */
	int i = 1;
	for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
		const char *a = argv[i];
		if (!strcmp(a, "--")) { i++; break; }
		if (a[1] == '-') {
			if (!strcmp(a, "--debug-syms")) all_syms = 1;
			else if (!strcmp(a, "--print-file-name")) per_symbol_name = 1;
			else if (!strcmp(a, "--dynamic")) dynamic = 1;
			else if (!strcmp(a, "--extern-only")) extern_only = 1;
			else if (!strcmp(a, "--numeric-sort")) numeric = 1;
			else if (!strcmp(a, "--no-sort")) no_sort = 1;
			else if (!strcmp(a, "--reverse-sort")) reverse = 1;
			else if (!strcmp(a, "--print-size")) print_size = 1;
			else if (!strcmp(a, "--size-sort")) size_sort = 1;
			else if (!strcmp(a, "--undefined-only")) undefined_only = 1;
			else if (!strcmp(a, "--defined-only")) defined_only = 1;
			else if (!strcmp(a, "--special-syms")) special_syms = 1;
			else if (!strcmp(a, "--format=bsd") || !strcmp(a, "--radix=x") || !strcmp(a, "--no-demangle")) ;   /* the defaults */
			else if (!strcmp(a, "--version")) { printf("nm (the os toolchain) — GNU nm compatible\n"); return 0; }
			else die("unrecognized option '%s'", a);
			continue;
		}
		for (const char *c = a + 1; *c; c++)
			switch (*c) {
			case 'a': all_syms = 1; break;
			case 'A': case 'o': per_symbol_name = 1; break;
			case 'B': break;                                  /* BSD format: the only one */
			case 'D': dynamic = 1; break;
			case 'g': extern_only = 1; break;
			case 'n': case 'v': numeric = 1; break;
			case 'p': no_sort = 1; break;
			case 'r': reverse = 1; break;
			case 'S': print_size = 1; break;
			case 'u': undefined_only = 1; break;
			case 'U': defined_only = 1; break;
			case 'V': printf("nm (the os toolchain) — GNU nm compatible\n"); return 0;
			default: die("invalid option -- '%c'", *c);
			}
	}
	if (size_sort && reverse) die("--size-sort with -r: unsupported");
	if (size_sort && numeric) die("--size-sort with -n: unsupported");
	if (i == argc) { per_file_header = 0; nm_file("a.out"); return 0; }
	per_file_header = argc - i > 1;
	for (; i < argc; i++) nm_file(argv[i]);
	return 0;
}
