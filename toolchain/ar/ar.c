/*
 * ar.c — our from-scratch archiver, the GNU `ar` equivalent. An archive (.a) is just a container: a
 * magic line then a sequence of members, each a 60-byte header + its bytes (padded to even length). We
 * write the GNU/SysV variant so BOTH our ld and GNU ld accept the result:
 *   "!<arch>\n"
 *   member "/"   — the ARMAP symbol index: for every global symbol DEFINED by a member, the file offset
 *                  of that member's header, so a linker can find "which member defines foo" in O(1).
 *   member "//"  — the extended name table (only if some member name won't fit the 16-byte name field).
 *   members ...  — the archived object files.
 * The symbol index uses BIG-ENDIAN 32-bit counts/offsets (the format is fixed big-endian, unlike ELF).
 *
 * Usage: ar <rc|cr|...> archive.a member.o ...   (key letters are accepted but we always create+replace).
 * We read each input's ELF symbol table to list its exported (global, defined) symbols for the index.
 * Shares the ELF format with the rest of the toolchain via common/elf.h. Only .o inputs are supported.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "elf.h"

static void die(const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	fputs("ar: ", stderr); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap); exit(1);
}

#define MAXMEM 256
#define MAXSYM 65536
typedef struct { const char *path; char name[64]; u8 *data; long size;
                 u32 hdr_off; char field[17]; } Member;    /* field = the 16-byte name written in the header */
static Member mem[MAXMEM]; static int nmem;
static struct { char *name; int member; } sym[MAXSYM]; static int nsym;

/* Read a whole file into a fresh buffer. */
static u8 *slurp(const char *path, long *size) {
	FILE *f = fopen(path, "rb"); if (!f) die("cannot open %s", path);
	fseek(f, 0, SEEK_END); *size = ftell(f); fseek(f, 0, SEEK_SET);
	u8 *b = malloc(*size); if (fread(b, 1, *size, f) != (size_t)*size) die("%s: read failed", path);
	fclose(f); return b;
}

/* basename without directory. */
static const char *base_of(const char *p) { const char *s = strrchr(p, '/'); return s ? s + 1 : p; }

/* Collect the global, DEFINED symbols of one ELF object into sym[] (owned by member `mi`). */
static void index_object(int mi) {
	u8 *d = mem[mi].data; long n = mem[mi].size;
	if (n < (long)sizeof(Elf32_Ehdr) || memcmp(d, "\177ELF\1\1", 6)) die("%s: not a little-endian ELF32", mem[mi].path);
	Elf32_Ehdr *eh = (Elf32_Ehdr *)d; Elf32_Shdr *sh = (Elf32_Shdr *)(d + eh->e_shoff);
	for (int i = 0; i < eh->e_shnum; i++) if (sh[i].sh_type == SHT_SYMTAB) {
		Elf32_Sym *s = (Elf32_Sym *)(d + sh[i].sh_offset);
		int ns = sh[i].sh_size / sizeof(Elf32_Sym);
		const char *str = (const char *)(d + sh[sh[i].sh_link].sh_offset);
		for (int k = 0; k < ns; k++) {
			if (ELF32_ST_BIND(s[k].st_info) != STB_GLOBAL || s[k].st_shndx == SHN_UNDEF || !s[k].st_name) continue;
			if (nsym >= MAXSYM) die("too many symbols");
			sym[nsym].name = strdup(str + s[k].st_name); sym[nsym].member = mi; nsym++;
		}
	}
}

/* Write a header field: left-justified in `width` bytes, space-padded, no NUL (archive headers are fixed-width). */
static void wr_field(FILE *f, const char *val, int width) {
	int n = strlen(val); for (int i = 0; i < width; i++) fputc(i < n ? val[i] : ' ', f);
}
static void put_be32(FILE *f, u32 v) { fputc(v >> 24, f); fputc(v >> 16, f); fputc(v >> 8, f); fputc(v, f); }

/* Emit a 60-byte member header (name already formatted into the 16-byte field). */
static void wr_header(FILE *f, const char *namefield, long size) {
	char sz[16]; snprintf(sz, sizeof sz, "%ld", size);
	wr_field(f, namefield, 16); wr_field(f, "0", 12); wr_field(f, "0", 6); wr_field(f, "0", 6);
	wr_field(f, "100644", 8); wr_field(f, sz, 10); fputc('`', f); fputc('\n', f);   /* fmag = "`\n" */
}

int main(int argc, char **argv) {
	if (argc < 3 || !strchr(argv[1], 'r')) die("usage: ar <rc|cr> archive.a member.o ...");
	const char *out = argv[2];

	/* Load every member + build the long-name table (`//`) for names that won't fit the 16-byte field. */
	char longtab[8192]; u32 longlen = 0;
	for (int i = 3; i < argc; i++) {
		if (nmem >= MAXMEM) die("too many members");
		Member *m = &mem[nmem];
		m->path = argv[i]; m->data = slurp(argv[i], &m->size);
		const char *b = base_of(argv[i]); snprintf(m->name, sizeof m->name, "%s", b);
		if (strlen(b) <= 15) snprintf(m->field, sizeof m->field, "%s/", b);   /* inline: "name/" */
		else {                                                                /* long: "/offset" into // */
			snprintf(m->field, sizeof m->field, "/%u", longlen);
			longlen += snprintf(longtab + longlen, sizeof longtab - longlen, "%s/\n", b);
		}
		index_object(nmem); nmem++;
	}
	int have_long = longlen > 0;
	u32 longlen_p = longlen + (longlen & 1);   /* members are padded to even length */

	/* Symbol-index (`/`) data size: BE count + BE offset per symbol + each name with its NUL. */
	u32 symdata = 4 + 4u * nsym; for (int i = 0; i < nsym; i++) symdata += strlen(sym[i].name) + 1;
	u32 symdata_p = symdata + (symdata & 1);

	/* Now every offset is known: place member headers after magic + `/` + optional `//`. */
	u32 pos = 8 + 60 + symdata_p + (have_long ? 60 + longlen_p : 0);
	for (int i = 0; i < nmem; i++) { mem[i].hdr_off = pos; pos += 60 + (u32)mem[i].size + (mem[i].size & 1); }

	FILE *f = fopen(out, "wb"); if (!f) die("cannot create %s", out);
	fwrite("!<arch>\n", 1, 8, f);

	wr_header(f, "/", symdata);                                    /* symbol index member */
	put_be32(f, nsym);
	for (int i = 0; i < nsym; i++) put_be32(f, mem[sym[i].member].hdr_off);
	for (int i = 0; i < nsym; i++) fwrite(sym[i].name, 1, strlen(sym[i].name) + 1, f);
	if (symdata & 1) fputc('\n', f);                              /* pad to even */

	if (have_long) { wr_header(f, "//", longlen); fwrite(longtab, 1, longlen, f); if (longlen & 1) fputc('\n', f); }

	for (int i = 0; i < nmem; i++) {                              /* the object members */
		wr_header(f, mem[i].field, mem[i].size);
		fwrite(mem[i].data, 1, mem[i].size, f);
		if (mem[i].size & 1) fputc('\n', f);
	}
	fclose(f);
	return 0;
}
