/*
 * size.c — our from-scratch section sizer, the GNU `size` equivalent, byte for byte.
 *
 * In:  ELF32 objects / executables / shared objects (and archives: every member in turn).
 * Out: how much of the file is code, initialized data and zero-filled data, in either of GNU's two formats:
 *
 *   BERKELEY (the default) — one line per file, tab-separated 7-wide columns, totals in decimal AND hex:
 *        text    data     bss     dec     hex filename
 *       31510      16   22964   54490    d4da kernel.elf
 *     The split is BFD's berkeley_sum, which is NOT "the .text/.data/.bss sections": only ALLOC sections
 *     count, anything executable OR read-only is "text" (so .rodata lands there), the rest is "data" if it
 *     has contents and "bss" if it does not. A section's NAME never enters into it.
 *
 *   SYSV (-A) — one line per ALLOC section, name/size/addr, then a Total. Column widths are computed from
 *     the widest value actually printed, as BFD does.
 *
 * -d/-o/-x choose the radix of the size columns (decimal/octal/hex); the dec and hex totals Berkeley prints
 * last are always decimal and hex whatever the radix. -t adds a (TOTALS) line across all the files.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "elfread.h"
#include "arread.h"
#include "bfdsec.h"

static const char *tool = "size";
void die(const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	fprintf(stderr, "%s: ", tool); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap); exit(1);
}

static u8 *slurp(const char *path, long *size) {
	FILE *fp = fopen(path, "rb"); if (!fp) die("'%s': No such file", path);
	fseek(fp, 0, SEEK_END); *size = ftell(fp); fseek(fp, 0, SEEK_SET);
	u8 *b = malloc(*size ? *size : 1);
	if (fread(b, 1, *size, fp) != (size_t)*size) die("%s: read failed", path);
	fclose(fp); return b;
}

static int sysv, radix = 10, want_totals, hdr_done;
static unsigned long tot_text, tot_data, tot_bss;   /* -t, across every file */

/* A number in the chosen radix, right-aligned in `w` — BFD's rprint_number: hex and octal carry their C
 * prefix, which counts toward the width. */
static void rnum(int w, unsigned long v) {
	char b[32];
	if (radix == 10) snprintf(b, sizeof b, "%lu", v);
	else if (radix == 8) snprintf(b, sizeof b, "0%lo", v);
	else snprintf(b, sizeof b, "0x%lx", v);
	printf("%*s", w, b);
}
static int numlen(unsigned long v) { char b[32]; return radix == 10 ? snprintf(b, sizeof b, "%lu", v)
	: radix == 8 ? snprintf(b, sizeof b, "0%lo", v) : snprintf(b, sizeof b, "0x%lx", v); }

/* berkeley_sum: ALLOC only; executable or read-only is text; else with contents is data; else bss. */
static void berkeley(const ElfFile *f, unsigned long *text, unsigned long *data, unsigned long *bss) {
	*text = *data = *bss = 0;
	for (int j = 1; j < f->nsh; j++) {
		const Elf32_Shdr *s = &f->sh[j];
		if (!(s->sh_flags & SHF_ALLOC)) continue;
		if ((s->sh_flags & SHF_EXECINSTR) || !(s->sh_flags & SHF_WRITE)) *text += s->sh_size;
		else if (s->sh_type != SHT_NOBITS) *data += s->sh_size;
		else *bss += s->sh_size;
	}
}

static void print_berkeley(const ElfFile *f, const char *name, const char *member) {
	unsigned long text, data, bss;
	berkeley(f, &text, &data, &bss);
	if (!hdr_done) { printf("%7s\t%7s\t%7s\t%7s\t%7s\t%s\n", "text", "data", "bss", radix == 8 ? "oct" : "dec", "hex", "filename"); hdr_done = 1; }
	unsigned long total = text + data + bss;
	rnum(7, text); putchar('\t'); rnum(7, data); putchar('\t'); rnum(7, bss); putchar('\t');
	if (radix == 8) printf("%7lo\t%7lx\t%s", total, total, name); else printf("%7lu\t%7lx\t%s", total, total, name);
	if (member) printf(" (ex %s)", member);
	putchar('\n');
	tot_text += text; tot_data += data; tot_bss += bss;
}

static void print_sysv(const ElfFile *f, const char *name, const char *member) {
	/* widths first: BFD sizes the columns to the widest thing it is about to print */
	int wn = (int)strlen("section"), ws = (int)strlen("size"), wa = (int)strlen("addr");
	unsigned long total = 0;
	/* -A lists every section BFD SHOWS (not just the allocated ones): the symbol tables and the relocations
	 * it consumes are absent, everything else is here — the same list objdump -h prints. */
	for (int j = 1; j < f->nsh; j++) {
		const Elf32_Shdr *s = &f->sh[j];
		if (!bfd_has_section(f, j)) continue;
		int l = (int)strlen(f->shstr + s->sh_name); if (l > wn) wn = l;
		l = numlen(s->sh_size); if (l > ws) ws = l;
		l = numlen(s->sh_addr); if (l > wa) wa = l;
		total += s->sh_size;
	}
	int l = numlen(total); if (l > ws) ws = l;
	if (strlen("Total") > (size_t)wn) wn = (int)strlen("Total");
	printf("%s  ", name);                             /* an archive member says which archive it came from */
	if (member) printf(" (ex %s)", member);
	printf(":\n%-*s   %*s   %*s\n", wn, "section", ws, "size", wa, "addr");
	for (int j = 1; j < f->nsh; j++) {
		const Elf32_Shdr *s = &f->sh[j];
		if (!bfd_has_section(f, j)) continue;
		printf("%-*s   ", wn, f->shstr + s->sh_name);
		rnum(ws, s->sh_size); printf("   "); rnum(wa, s->sh_addr); putchar('\n');
	}
	printf("%-*s   ", wn, "Total"); rnum(ws, total); putchar('\n');
	printf("\n\n");   /* GNU leaves TWO blank lines after each file's table */
	unsigned long text, data, bss; berkeley(f, &text, &data, &bss);
	tot_text += text; tot_data += data; tot_bss += bss;
}

static void one_elf(const u8 *data, long size, const char *name, const char *member) {
	if (size < 18 || memcmp(data, "\177ELF", 4)) {   /* GNU exits 3 for a file it cannot read as an object */
		fprintf(stderr, "%s: %s: file format not recognized\n", tool, name); exit(3);
	}
	ElfFile f; elf_open(&f, name, (u8 *)data, size, data[16] | data[17] << 8, 0);
	if (sysv) print_sysv(&f, name, member); else print_berkeley(&f, name, member);
}

static void one_file(const char *name) {
	long size; u8 *data = slurp(name, &size);
	if (ar_is_archive(data, size)) {
		Archive ar; ar_open(&ar, name, data, size);
		ArMember m; long pos = 8;
		while (ar_next(&ar, &pos, &m)) {
			if (m.size >= 18 && !memcmp(m.data, "\177ELF", 4)) one_elf(m.data, m.size, m.name, name);
		}
		return;
	}
	one_elf(data, size, name, NULL);
}

int main(int argc, char **argv) {
	const char **files = NULL; int nfiles = 0;
	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		if (!strcmp(a, "-A") || !strcmp(a, "--format=sysv")) sysv = 1;
		else if (!strcmp(a, "-B") || !strcmp(a, "--format=berkeley")) sysv = 0;
		else if (!strcmp(a, "-d") || !strcmp(a, "--radix=10")) radix = 10;
		else if (!strcmp(a, "-o") || !strcmp(a, "--radix=8")) radix = 8;
		else if (!strcmp(a, "-x") || !strcmp(a, "--radix=16")) radix = 16;
		else if (!strcmp(a, "-t") || !strcmp(a, "--totals")) want_totals = 1;
		else if (!strcmp(a, "--version") || !strcmp(a, "-V")) { printf("size (os toolchain) 2.42\n"); return 0; }
		else if (!strcmp(a, "--help")) { printf("usage: size [-A|-B] [-d|-o|-x] [-t] file...\n"); return 0; }
		else if (a[0] == '-' && a[1]) die("unrecognized option '%s'", a);
		else { files = realloc(files, (nfiles + 1) * sizeof *files); files[nfiles++] = a; }
	}
	if (!nfiles) { files = realloc(files, sizeof *files); files[nfiles++] = "a.out"; }
	for (int i = 0; i < nfiles; i++) one_file(files[i]);
	if (want_totals && !sysv) {
		unsigned long total = tot_text + tot_data + tot_bss;
		rnum(7, tot_text); putchar('\t'); rnum(7, tot_data); putchar('\t'); rnum(7, tot_bss); putchar('\t');
		if (radix == 8) printf("%7lo\t%7lx\t(TOTALS)\n", total, total); else printf("%7lu\t%7lx\t(TOTALS)\n", total, total);
	}
	return 0;
}
