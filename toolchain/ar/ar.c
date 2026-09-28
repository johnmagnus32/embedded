/*
 * ar.c — our from-scratch archiver, the GNU `ar` equivalent. An archive (.a) is just a container: a
 * magic line then a sequence of members, each a 60-byte header + its bytes (padded to even length). We
 * write the GNU/SysV variant so BOTH our ld and GNU ld accept the result:
 *   "!<arch>\n"
 *   member "/"   — the ARMAP symbol index: for every global/weak symbol DEFINED by a member, the file offset
 *                  of that member's header, so a linker can find "which member defines foo" in O(1).
 *   member "//"  — the extended name table (only if some member name won't fit the 16-byte name field).
 *   members ...  — the archived object files.
 * The symbol index uses BIG-ENDIAN 32-bit counts/offsets (the format is fixed big-endian, unlike ELF).
 *
 * Usage: ar <rc|cr|...> archive.a member.o ...   (key letters are accepted but we always create+replace).
 * We read each input's ELF symbol table (validated by common/elfread) to list the symbols it defines for the index.
 * Only relocatable (.o) inputs are supported.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "elfread.h"

void die(const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	fputs("ar: ", stderr); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap); exit(1);
}

typedef struct { const char *path; u8 *data; long size;
                 u32 hdr_off; char field[17]; } Member;    /* field = the 16-byte name written in the header */
static Member *mem; static int nmem, memcap;
static struct { const char *name; int member; } *sym; static int nsym, symcap;

static void *grow(void *v, int n, int *cap, size_t esz) {
	if (n < *cap) return v;
	*cap = *cap ? *cap * 2 : 64;
	if (!(v = realloc(v, (size_t)*cap * esz))) die("out of memory");
	return v;
}

/* Read a whole file into a fresh buffer. */
static u8 *slurp(const char *path, long *size) {
	FILE *f = fopen(path, "rb"); if (!f) die("cannot open %s", path);
	fseek(f, 0, SEEK_END); *size = ftell(f); fseek(f, 0, SEEK_SET);
	u8 *b = malloc(*size); if (fread(b, 1, *size, f) != (size_t)*size) die("%s: read failed", path);
	fclose(f); return b;
}

/* basename without directory. */
static const char *base_of(const char *p) { const char *s = strrchr(p, '/'); return s ? s + 1 : p; }

/* Index the symbols one object DEFINES — global and weak, as GNU ar does (a weak definition still satisfies a
 * linker's search) — into sym[] (owned by member `mi`). */
static void index_object(int mi) {
	ElfFile f; elf_open(&f, mem[mi].path, mem[mi].data, mem[mi].size, ET_REL, 0);
	for (int k = 1; k < f.nsym; k++) {
		int b = ELF32_ST_BIND(f.sym[k].st_info);
		if ((b != STB_GLOBAL && b != STB_WEAK) || f.sym[k].st_shndx == SHN_UNDEF || !f.sym[k].st_name) continue;
		sym = grow(sym, nsym, &symcap, sizeof *sym);
		sym[nsym].name = f.strtab + f.sym[k].st_name; sym[nsym].member = mi; nsym++;
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
	char *longtab = NULL; u32 longlen = 0;
	for (int i = 3; i < argc; i++) {
		mem = grow(mem, nmem, &memcap, sizeof *mem);
		Member *m = &mem[nmem];
		m->path = argv[i]; m->data = slurp(argv[i], &m->size);
		const char *b = base_of(argv[i]);
		if (strlen(b) <= 15) snprintf(m->field, sizeof m->field, "%s/", b);   /* inline: "name/" */
		else {                                                                /* long: "/offset" into // */
			snprintf(m->field, sizeof m->field, "/%u", longlen);
			longtab = realloc(longtab, longlen + strlen(b) + 3);
			longlen += (u32)sprintf(longtab + longlen, "%s/\n", b);
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
