/*
 * addr2line.c — our from-scratch GNU `addr2line`: an address becomes a file and a line.
 *
 * This is the CONSUMER of the line table as and cc now produce. A fault gives you an address and nothing
 * else; this turns it back into a place in the source. Until now the project could emit debug information
 * that none of its own tools could read, and every check of it leaned on GNU's addr2line as both oracle and
 * reader.
 *
 * HOW THE LOOKUP WORKS. .debug_line holds a byte-code program, not a table: opcodes that advance an address
 * and a line and emit a row. Running it rebuilds the rows, which are sorted by address within a sequence; the
 * row covering an address is the last one at or below it, and a sequence's final end_sequence row marks the
 * end of its range (and belongs to no line). The file comes from the header's file table, joined with its
 * directory and, when that is relative, the compilation unit's DW_AT_comp_dir from .debug_info.
 *
 * SCOPE. DWARF 2, 3 and 4 line programs, which is what both gcc and our as emit (gcc's .debug_info is
 * version 5, but its .debug_line is still 3). A version 5 line header reorganises the file and directory
 * tables entirely and is refused rather than guessed at. .debug_info is read only far enough to find
 * DW_AT_name and DW_AT_comp_dir; for a version 5 unit, whose strings are indirected through .debug_str
 * offsets, comp_dir is skipped and a relative file name is printed as it stands.
 *
 * Function names (-f) come from the symbol table, not from DWARF: our .debug_info carries no subprogram
 * DIEs, and GNU falls back to the same place.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "elfread.h"

static const char *tool = "addr2line";
void die(const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	fprintf(stderr, "%s: ", tool); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap);
	exit(1);
}

/* ---- little-endian readers over a bounds-checked cursor ---- */
typedef struct { const unsigned char *p, *end; } Cur;
static int cur_left(const Cur *c, size_t n) { return (size_t)(c->end - c->p) >= n; }
static unsigned char rd8(Cur *c) { if (!cur_left(c, 1)) die("truncated debug section"); return *c->p++; }
static unsigned rd16(Cur *c) { unsigned v; if (!cur_left(c, 2)) die("truncated debug section"); v = c->p[0] | (unsigned)c->p[1] << 8; c->p += 2; return v; }
static unsigned rd32(Cur *c) {
	if (!cur_left(c, 4)) die("truncated debug section");
	unsigned v = c->p[0] | (unsigned)c->p[1] << 8 | (unsigned)c->p[2] << 16 | (unsigned)c->p[3] << 24;
	c->p += 4; return v;
}
static unsigned long rduleb(Cur *c) {
	unsigned long v = 0; int s = 0;
	for (;;) { unsigned char b = rd8(c); v |= (unsigned long)(b & 0x7f) << s; s += 7; if (!(b & 0x80)) return v; }
}
static long rdsleb(Cur *c) {
	long v = 0; int s = 0; unsigned char b;
	do { b = rd8(c); v |= (long)(b & 0x7f) << s; s += 7; } while (b & 0x80);
	if (s < (int)(8 * sizeof v) && (b & 0x40)) v -= 1L << s;      /* sign-extend */
	return v;
}
static const char *rdstr(Cur *c) {
	const unsigned char *s = c->p;
	while (c->p < c->end && *c->p) c->p++;
	if (c->p >= c->end) die("unterminated string in a debug section");
	c->p++;
	return (const char *)s;
}

/* ---- the rebuilt table ---- */
typedef struct { unsigned addr; int file; int line; int end; int sec; } Row;
static Row *rows; static int nrow, rowcap;
static const char **lfiles; static int *lfiledir; static int nlfiles;   /* per CU, reset each unit */
static const char **ldirs; static int nldirs;
static const char *cu_comp_dir, *cu_name;
/* The compilation unit's own address range. GNU distinguishes two kinds of failure with it, and prints them
 * differently: an address INSIDE the range with no row covering it is "found, line unknown" and comes out as
 * "??:?", while an address outside every unit is simply not found and comes out as "??:0". */
static unsigned cu_lo, cu_hi; static int cu_have_range;

static int cur_seq_sec;                 /* the section the sequence being run belongs to (0 = unknown) */
static void row_add(unsigned addr, int file, int line, int end) {
	if (nrow == rowcap) { rowcap = rowcap ? 2 * rowcap : 256; rows = realloc(rows, (size_t)rowcap * sizeof *rows); }
	rows[nrow++] = (Row){ addr, file, line, end, cur_seq_sec };
}
/* Which section a sequence covers: DW_LNE_set_address's operand carries a relocation naming it. Without this
 * the sequences of a relocatable object are indistinguishable -- every section's addresses start at 0 -- and a
 * lookup silently answers from whichever sequence happens to match first. */
static int sec_of_setaddr(const ElfFile *f, unsigned dl_off) {
	for (int i = 1; i < f->nsh; i++) {
		if (f->sh[i].sh_type != 9 /* SHT_REL */) continue;
		const char *n = elf_string(f, f->eh->e_shstrndx, f->sh[i].sh_name);
		if (!n || strcmp(n, ".rel.debug_line")) continue;
		Elf32_Rel *r = (Elf32_Rel *)(f->data + f->sh[i].sh_offset);
		unsigned cnt = f->sh[i].sh_size / sizeof *r;
		for (unsigned k = 0; k < cnt; k++) {
			if (r[k].r_offset != dl_off) continue;
			unsigned si = r[k].r_info >> 8;
			if (si < (unsigned)f->nsym) return (int)f->sym[si].st_shndx;
		}
	}
	return 0;
}

/* Find a section by name; returns its data and size, or NULL. */
static const unsigned char *sec_data(const ElfFile *f, const char *name, unsigned *size) {
	for (int i = 1; i < f->nsh; i++) {
		const char *n = elf_string(f, f->eh->e_shstrndx, f->sh[i].sh_name);
		if (n && !strcmp(n, name) && f->sh[i].sh_type != 8 /* NOBITS */) { *size = f->sh[i].sh_size; return f->data + f->sh[i].sh_offset; }
	}
	*size = 0; return NULL;
}

/* Read .debug_info just far enough for DW_AT_name and DW_AT_comp_dir of the first unit. */
static void read_cu(const ElfFile *f) {
	unsigned isz, asz, ssz;
	const unsigned char *info = sec_data(f, ".debug_info", &isz);
	const unsigned char *abbr = sec_data(f, ".debug_abbrev", &asz);
	const unsigned char *str  = sec_data(f, ".debug_str", &ssz);
	if (!info || !abbr) return;
	Cur ic = { info, info + isz };
	unsigned len = rd32(&ic);
	if (len == 0xffffffffu) return;                    /* 64-bit DWARF: not produced on ARM32 */
	const unsigned char *unit_end = ic.p + len;
	unsigned ver = rd16(&ic);
	unsigned abbrev_off;
	if (ver >= 5) { rd8(&ic); rd8(&ic); abbrev_off = rd32(&ic); }   /* unit_type, addr_size, then abbrev */
	else { abbrev_off = rd32(&ic); rd8(&ic); }
	if (ver >= 5) return;                              /* its strings are .debug_str_offsets-indirected: skipped */
	if (abbrev_off >= asz) return;
	unsigned long want = rduleb(&ic);                    /* the root DIE's abbrev code */
	if (!want) return;

	/* walk the abbrev table to that code, then read its attribute list */
	Cur ac = { abbr + abbrev_off, abbr + asz };
	for (;;) {
		unsigned long code = rduleb(&ac);
		if (!code) return;                             /* ran out: give up quietly */
		rduleb(&ac);                                     /* tag */
		rd8(&ac);                                       /* has children */
		if (code == want) break;
		for (;;) { unsigned long at = rduleb(&ac), fm = rduleb(&ac); if (!at && !fm) break; if (fm == 0x21) rduleb(&ac); }
	}
	for (;;) {
		unsigned long at = rduleb(&ac), fm = rduleb(&ac);
		if (!at && !fm) break;
		if (fm == 0x21) { rduleb(&ac); continue; }        /* implicit_const: no bytes in the DIE */
		const char *sval = NULL;
		switch (fm) {
		case 0x08: sval = rdstr(&ic); break;                                  /* string */
		case 0x0e: { unsigned o = rd32(&ic); if (str && o < ssz) sval = (const char *)str + o; break; }   /* strp */
		case 0x01: case 0x06: case 0x17: case 0x13: {                        /* addr, data4, sec_offset, ref4 */
			unsigned v = rd32(&ic);
			if (at == 0x11) { cu_lo = v; cu_have_range = 1; }                /* DW_AT_low_pc */
			else if (at == 0x12) cu_hi = v;                                  /* DW_AT_high_pc: a LENGTH in DWARF 4 */
			break;
		}
		case 0x0b: case 0x21: rd8(&ic); break;                                /* data1 */
		case 0x05: rd16(&ic); break;                                          /* data2 */
		case 0x07: rd32(&ic); rd32(&ic); break;                              /* data8 */
		case 0x0f: rduleb(&ic); break;                                         /* udata */
		case 0x0d: rdsleb(&ic); break;                                         /* sdata */
		case 0x19: break;                                                    /* flag_present: no bytes */
		case 0x18: { unsigned long n = rduleb(&ic); while (n-- && ic.p < ic.end) ic.p++; break; }   /* exprloc */
		default: return;                               /* a form we do not model: stop rather than desync */
		}
		if (at == 0x03 && sval) cu_name = sval;         /* DW_AT_name */
		if (at == 0x1b && sval) cu_comp_dir = sval;     /* DW_AT_comp_dir */
		if (ic.p >= unit_end) break;
	}
}

/* Run every line program in .debug_line, filling rows[]. */
static void read_lines(const ElfFile *f) {
	/* (d below is the section base, so an operand's offset within it can be computed) */
	unsigned size;
	const unsigned char *d = sec_data(f, ".debug_line", &size);
	if (!d) return;
	Cur all = { d, d + size };
	while (all.p < all.end) {
		unsigned unit_len = rd32(&all);
		if (unit_len == 0xffffffffu) die(".debug_line: 64-bit DWARF is not supported");
		const unsigned char *unit_end = all.p + unit_len;
		if (unit_end > all.end) die(".debug_line: unit length runs past the section");
		unsigned ver = rd16(&all);
		if (ver < 2 || ver > 4)
			die(".debug_line: version %u is not supported (2, 3 and 4 are; version 5 reorganises the file table)", ver);
		unsigned hdr_len = rd32(&all);
		const unsigned char *prog = all.p + hdr_len;
		unsigned min_inst = rd8(&all);
		if (ver >= 4) rd8(&all);                        /* maximum_operations_per_instruction */
		unsigned default_is_stmt = rd8(&all); (void)default_is_stmt;
		int line_base = (signed char)rd8(&all);
		unsigned line_range = rd8(&all), opcode_base = rd8(&all);
		unsigned char stdlen[256] = {0};
		for (unsigned i = 1; i < opcode_base && i < 256; i++) stdlen[i] = rd8(&all);

		nldirs = 0; nlfiles = 0;
		ldirs = realloc(ldirs, sizeof *ldirs);         /* index 0 is the compilation directory */
		ldirs[nldirs++] = NULL;
		while (all.p < all.end && *all.p) { ldirs = realloc(ldirs, (size_t)(nldirs + 1) * sizeof *ldirs); ldirs[nldirs++] = rdstr(&all); }
		rd8(&all);
		lfiles = realloc(lfiles, sizeof *lfiles); lfiledir = realloc(lfiledir, sizeof *lfiledir);
		lfiles[nlfiles] = NULL; lfiledir[nlfiles] = 0; nlfiles++;     /* 1-based in the program */
		while (all.p < all.end && *all.p) {
			const char *nm = rdstr(&all);
			unsigned long dir = rduleb(&all); rduleb(&all); rduleb(&all);   /* dir, mtime, length */
			lfiles = realloc(lfiles, (size_t)(nlfiles + 1) * sizeof *lfiles);
			lfiledir = realloc(lfiledir, (size_t)(nlfiles + 1) * sizeof *lfiledir);
			lfiles[nlfiles] = nm; lfiledir[nlfiles] = (int)dir; nlfiles++;
		}
		rd8(&all);

		Cur pc = { prog, unit_end };
		unsigned addr = 0; int line = 1, file = 1;
		while (pc.p < pc.end) {
			unsigned op = rd8(&pc);
			if (op >= opcode_base) {                   /* special: one byte, both advances */
				unsigned adj = op - opcode_base;
				addr += (adj / line_range) * min_inst;
				line += line_base + (int)(adj % line_range);
				row_add(addr, file, line, 0);
			} else if (op == 0) {                      /* extended */
				unsigned long len = rduleb(&pc);
				const unsigned char *next = pc.p + len;
				unsigned sub = len ? rd8(&pc) : 0;
				if (sub == 1) { row_add(addr, file, line, 1); addr = 0; line = 1; file = 1; }   /* end_sequence */
				else if (sub == 2) {                                                            /* set_address */
					cur_seq_sec = sec_of_setaddr(f, (unsigned)(pc.p - d));   /* the operand's offset in .debug_line */
					addr = rd32(&pc);
				}
				pc.p = next;
			} else switch (op) {
			case 1: row_add(addr, file, line, 0); break;                /* copy */
			case 2: addr += (unsigned)rduleb(&pc) * min_inst; break;      /* advance_pc */
			case 3: line += (int)rdsleb(&pc); break;                      /* advance_line */
			case 4: file = (int)rduleb(&pc); break;                       /* set_file */
			case 5: rduleb(&pc); break;                                   /* set_column */
			case 6: case 7: break;                                      /* negate_stmt, set_basic_block */
			case 8: addr += ((255 - opcode_base) / line_range) * min_inst; break;   /* const_add_pc */
			case 9: addr += rd16(&pc); break;                            /* fixed_advance_pc */
			case 10: case 11: break;                                    /* prologue_end, epilogue_begin */
			case 12: rduleb(&pc); break;                                  /* set_isa */
			default: for (unsigned i = 0; i < stdlen[op]; i++) rduleb(&pc); break;   /* unknown: skip its args */
			}
		}
		all.p = unit_end;
	}
}

/* The row covering `a`.
 *
 * Rows are grouped into SEQUENCES, each ended by an end_sequence row, and each sequence belongs to one output
 * section. In a relocatable object every section's addresses start at 0, so sequences OVERLAP and an address
 * on its own is ambiguous -- which is what `addr2line -j <section>` exists to resolve. GNU answers with the
 * FIRST sequence that contains the address, so this walks sequence by sequence and stops at the first hit
 * rather than scanning every row and keeping the last. Measured on a real kernel translation unit: scanning
 * all rows disagreed with GNU on 153 of 5655 addresses, every one of them a sequence-overlap case.
 */
static const ElfFile *lk_file;
static const Row *lookup(unsigned a) {
	int i = 0;
	while (i < nrow) {
		int j = i;                                  /* [i, j] is one sequence, rows[j] being its end row */
		while (j < nrow && !rows[j].end) j++;
		if (j >= nrow) break;
		/* The SECTION decides first. GNU resolves an address to a section and then consults that section's
		 * program, so an address inside the section but below its first row has no line -- it does not fall
		 * through to another section's sequence. */
		int sec = rows[i].sec;
		int sec_match = 0;
		if (sec > 0 && lk_file && sec < lk_file->nsh) {
			unsigned lo = lk_file->sh[sec].sh_addr, sz = lk_file->sh[sec].sh_size;
			sec_match = a >= lo && a < lo + sz;
		} else sec_match = rows[i].addr <= a && a < rows[j].addr;   /* no reloc to go on: the sequence's own range */
		if (sec_match) {
			const Row *best = NULL;
			for (int k = i; k < j; k++) if (rows[k].addr <= a && (!best || rows[k].addr >= best->addr)) best = &rows[k];
			return best;                            /* may be NULL: in the section, but no row covers it */
		}
		i = j + 1;
	}
	return NULL;
}

static const char *file_of(const Row *r, int basename_only) {
	static char buf[4096];
	const char *nm = (r->file >= 0 && r->file < nlfiles && lfiles[r->file]) ? lfiles[r->file] : NULL;
	if (!nm) return "??";
	if (basename_only) { const char *s = strrchr(nm, '/'); return s ? s + 1 : nm; }
	if (nm[0] == '/') return nm;
	const char *dir = (r->file < nlfiles && lfiledir[r->file] > 0 && lfiledir[r->file] < nldirs) ? ldirs[lfiledir[r->file]] : NULL;
	if (dir && dir[0] == '/') { snprintf(buf, sizeof buf, "%s/%s", dir, nm); return buf; }
	if (cu_comp_dir && dir) { snprintf(buf, sizeof buf, "%s/%s/%s", cu_comp_dir, dir, nm); return buf; }
	if (cu_comp_dir) { snprintf(buf, sizeof buf, "%s/%s", cu_comp_dir, nm); return buf; }
	return nm;
}

/* The function containing `a`, from the symbol table (STT_FUNC, sized). */
static const char *func_of(const ElfFile *f, unsigned a) {
	const char *best = NULL; unsigned bestv = 0;
	for (int i = 0; i < f->nsym; i++) {
		if ((f->sym[i].st_info & 0xf) != 2) continue;             /* STT_FUNC */
		unsigned v = f->sym[i].st_value, sz = f->sym[i].st_size;
		if (a < v || (sz && a >= v + sz)) continue;
		if (!best || v >= bestv) { best = elf_string(f, f->sh[f->symtab].sh_link, f->sym[i].st_name); bestv = v; }
	}
	return best && best[0] ? best : "??";
}

/* The STT_FILE symbol, which is what GNU falls back to for the file name when there is no line table. */
static const char *file_sym(const ElfFile *f) {
	for (int i = 0; i < f->nsym; i++)
		if ((f->sym[i].st_info & 0xf) == 4)                              /* STT_FILE */
			return elf_string(f, f->sh[f->symtab].sh_link, f->sym[i].st_name);
	return NULL;
}
static int func_known(const ElfFile *f, unsigned a) {                    /* inside a sized function symbol */
	for (int i = 0; i < f->nsym; i++) {
		if ((f->sym[i].st_info & 0xf) != 2) continue;
		if (f->sym[i].st_size && a >= f->sym[i].st_value && a < f->sym[i].st_value + f->sym[i].st_size) return 1;
	}
	return 0;
}
static const char *base_if(const char *p, int basenames) {
	if (!basenames || !p) return p ? p : "??";
	const char *s = strrchr(p, '/'); return s ? s + 1 : p;
}

static void usage(void) {
	fprintf(stderr, "Usage: addr2line [option(s)] [addr(s)]\n"
	                " Convert addresses into line number/file name pairs.\n"
	                " If no addresses are specified on the command line, they will be read from stdin\n"
	                " The options are:\n"
	                "  -a --addresses         Show addresses\n"
	                "  -e --exe=<executable>  Set the input file name (default is a.out)\n"
	                "  -f --functions         Show function names\n"
	                "  -p --pretty-print      Make the output easier to read for humans\n"
	                "  -s --basenames         Strip directory names\n"
	                "  -H --help              Display this information\n");
}

int main(int argc, char **argv) {
	const char *path = "a.out";
	int show_addr = 0, show_func = 0, basenames = 0, pretty = 0;
	const char **addrs = malloc((size_t)(argc + 1) * sizeof *addrs); int naddr = 0;
	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		if (!strcmp(a, "-e") || !strcmp(a, "--exe")) { if (i + 1 >= argc) die("option '-e' requires an argument"); path = argv[++i]; }
		else if (!strncmp(a, "--exe=", 6)) path = a + 6;
		else if (!strncmp(a, "-e", 2) && a[2]) path = a + 2;
		else if (!strcmp(a, "-a") || !strcmp(a, "--addresses")) show_addr = 1;
		else if (!strcmp(a, "-f") || !strcmp(a, "--functions")) show_func = 1;
		else if (!strcmp(a, "-s") || !strcmp(a, "--basenames")) basenames = 1;
		else if (!strcmp(a, "-p") || !strcmp(a, "--pretty-print")) pretty = 1;
		else if (!strcmp(a, "-C") || !strcmp(a, "--demangle")) ;   /* nothing here is C++; names pass through */
		else if (!strcmp(a, "-H") || !strcmp(a, "--help")) { usage(); return 0; }
		else if (!strcmp(a, "-i") || !strcmp(a, "--inlines"))
			die("option '%s' needs inlined-subroutine information, which is not produced (line numbers only)", a);
		else if (!strcmp(a, "-j") || !strcmp(a, "--section"))
			die("option '%s' (section-relative addresses) is not supported", a);
		else if (a[0] == '-' && a[1]) die("unrecognized option '%s'", a);
		else addrs[naddr++] = a;
	}

	FILE *fp = fopen(path, "rb");
	if (!fp) die("'%s': No such file", path);
	fseek(fp, 0, SEEK_END); long sz = ftell(fp); fseek(fp, 0, SEEK_SET);
	unsigned char *data = malloc((size_t)sz ? (size_t)sz : 1);
	if (fread(data, 1, (size_t)sz, fp) != (size_t)sz) die("'%s': read failed", path);
	fclose(fp);
	/* addr2line takes objects, executables and shared objects alike, and elf_open wants the exact e_type
	 * (only `machine` has an "any" value), so read the type out of the header and hand it back. */
	if (sz < (long)sizeof(Elf32_Ehdr)) die("'%s': not an ELF file", path);
	u16 etype = (u16)(data[16] | (unsigned)data[17] << 8);
	if (etype != 1 && etype != 2 && etype != 3) die("'%s': not an object, executable or shared object (e_type %u)", path, etype);
	ElfFile f;
	elf_open(&f, path, data, sz, etype, 0);
	lk_file = &f;
	read_cu(&f);
	read_lines(&f);

	char linebuf[256];
	for (int k = 0; ; k++) {
		const char *spec;
		if (naddr) { if (k >= naddr) break; spec = addrs[k]; }
		else {
			if (!fgets(linebuf, sizeof linebuf, stdin)) break;
			char *nl = strchr(linebuf, '\n'); if (nl) *nl = 0;
			if (!linebuf[0]) continue;
			spec = linebuf;
		}
		unsigned long a = strtoul(spec, NULL, 16);
		const Row *r = lookup((unsigned)a);
		/* "found" is wider than "a row covers it": an address inside the unit's range, or inside a sized
		 * function when there is no line information at all, is found with an unknown line. */
		int in_cu = cu_have_range && a >= cu_lo && a < (unsigned long)cu_lo + cu_hi;
		const char *ffile = file_sym(&f);
		int in_sym = !nrow && ffile && func_known(&f, (unsigned)a);
		int found = r || in_cu || in_sym;
		const char *fl = r ? file_of(r, basenames) : (in_sym ? base_if(ffile, basenames) : "??");
		const char *fn = show_func ? func_of(&f, (unsigned)a) : NULL;
		char lb[16];
		if (!found) snprintf(lb, sizeof lb, "0");                       /* not found at all: GNU prints 0 */
		else if (r) snprintf(lb, sizeof lb, "%d", r->line);
		else snprintf(lb, sizeof lb, "?");                              /* found, but no line for it */
		if (pretty) {
			if (show_addr) printf("0x%08lx: ", a);
			if (show_func) printf("%s at ", fn);
			printf("%s:%s\n", fl, lb);
		} else {
			if (show_addr) printf("0x%08lx\n", a);
			if (show_func) printf("%s\n", fn);
			printf("%s:%s\n", fl, lb);
		}
	}
	return 0;
}
