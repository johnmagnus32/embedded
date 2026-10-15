/*
 * dwline.c — the DWARF line-number program: `.file N "name"` / `.loc N LINE` turned into .debug_line.
 *
 * WHAT IT IS FOR. Machine code does not remember where it came from, so a fault gives you an address and
 * nothing else. .debug_line is the table that maps an address back to a file and line, which is what
 * addr2line reads, what a debugger uses to show source in a backtrace, and what turns a kernel oops into
 * something actionable instead of an afternoon of computing offsets by hand out of a 121 MiB vmlinux.
 *
 * WHY IT IS A PROGRAM AND NOT A TABLE. A row per instruction would be enormous, so DWARF stores a byte-code
 * that a consumer interprets to rebuild the table: "advance the address", "advance the line", "emit a row".
 * Most steps fit in ONE byte — a "special" opcode encodes a small address advance and a small line delta
 * together — which is where the compression comes from.
 *
 * The header fields below are GNU as's for ARM, read back out of its own output and verified by decoding its
 * bytes by hand (version 3, min_inst_len 2, line_base -5, line_range 14, opcode_base 13). Matching them is
 * what lets the two assemblers' objects be compared byte for byte.
 *
 *   special opcode = opcode_base + (addr_advance / min_inst_len) * line_range + (line_delta - line_base)
 * and it is usable only when the line delta is in [line_base, line_base + line_range) and the resulting
 * opcode fits in a byte; otherwise the advances are spelled out with standard opcodes first.
 */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "as.h"

#define DW_VERSION      3
#define MIN_INST_LEN    2      /* ARM: every instruction is 2- or 4-byte aligned, so advances are even */
#define DEFAULT_IS_STMT 1
#define LINE_BASE      (-5)
#define LINE_RANGE      14
#define OPCODE_BASE     13

/* standard opcodes */
#define DW_LNS_copy             1
#define DW_LNS_advance_pc       2
#define DW_LNS_advance_line     3
#define DW_LNS_set_file         4
#define DW_LNS_set_column       5
#define DW_LNS_const_add_pc     8
/* extended */
#define DW_LNE_end_sequence     1
#define DW_LNE_set_address      2

static const unsigned char std_lens[OPCODE_BASE - 1] = { 0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1 };

static char **files; static int nfiles;            /* 1-based: files[i-1] is `.file i "..."` */
static struct Loc { int sec; u32 off; int file; int line; int col; } *locs; static int nloc, loccap;

void dwline_file(int n, const char *name) {
	if (n < 1) die(".file: index must be 1 or more");
	if (n > nfiles) { files = realloc(files, (size_t)n * sizeof *files); for (int i = nfiles; i < n; i++) files[i] = NULL; nfiles = n; }
	free(files[n - 1]);
	files[n - 1] = strdup(name);
}
int dwline_have_files(void) { return nfiles > 0; }

void dwline_loc(int file, int line, int col, int sec, u32 off) {
	if (file < 1 || file > nfiles || !files[file - 1]) die(".loc: file %d was never given by .file", file);
	if (nloc == loccap) { loccap = loccap ? 2 * loccap : 64; locs = realloc(locs, (size_t)loccap * sizeof *locs); }
	/* Two .loc directives at one address both produce a row, as GAS does — a consumer wants to see that the
	 * statement boundary was there, even though no instruction came between them. */
	locs[nloc++] = (struct Loc){ sec, off, file, line, col };
}

/* ---- byte emission into the current section (.debug_line) ---- */
static void u8b(unsigned v) { unsigned char b = (unsigned char)v; emit(&b, 1); }
static void uleb(unsigned long v) { do { unsigned char b = v & 0x7f; v >>= 7; if (v) b |= 0x80; emit(&b, 1); } while (v); }
static void sleb(long v) {
	for (;;) {
		unsigned char b = (unsigned char)(v & 0x7f);
		v >>= 7;                                   /* arithmetic shift: sign propagates */
		int done = (v == 0 && !(b & 0x40)) || (v == -1 && (b & 0x40));
		if (!done) b |= 0x80;
		emit(&b, 1);
		if (done) return;
	}
}

/* One row: advance to (addr, line) from (*caddr, *cline). GNU uses DW_LNS_copy when neither advances (same
 * one byte as a special opcode, but different bytes — and the objects are compared byte for byte). */
static void row(u32 addr, int line, u32 *caddr, int *cline) {
	long dl = line - *cline;
	u32 da = addr - *caddr;
	if (da % MIN_INST_LEN) die("internal: .debug_line address advance %u is not a multiple of %d", da, MIN_INST_LEN);
	if (dl < LINE_BASE || dl >= LINE_BASE + LINE_RANGE) { u8b(DW_LNS_advance_line); sleb(dl); *cline = line; dl = 0; }
	if (!da && !dl) { u8b(DW_LNS_copy); return; }   /* no advance left to encode: one byte, and GNU's choice */
	unsigned long units = da / MIN_INST_LEN;
	unsigned long op = OPCODE_BASE + units * LINE_RANGE + (unsigned long)(dl - LINE_BASE);
	if (op > 255) {                                 /* too far for one byte: advance explicitly, then a special */
		u8b(DW_LNS_advance_pc); uleb(units);
		op = OPCODE_BASE + (unsigned long)(dl - LINE_BASE);
	}
	u8b((unsigned)op);
	*caddr = addr; *cline = line;
}

/* Build .debug_line. Called at the end of assembly, after every address is known. */
void dwline_emit(void) {
	if (!nloc) return;
	int save = cursec;
	int ds = sec_get(".debug_line", SHT_PROGBITS, 0);   /* not allocated: no SHF_ALLOC */
	cursec = ds;

	u32 unit_at = (u32)secs[ds].len;
	emit32(0);                                      /* unit_length, patched below */
	u8b(DW_VERSION); u8b(0);
	u32 hdr_at = (u32)secs[ds].len;
	emit32(0);                                      /* header_length, patched below */
	u32 after_hdr_len = (u32)secs[ds].len;
	u8b(MIN_INST_LEN); u8b(DEFAULT_IS_STMT); u8b((unsigned char)LINE_BASE); u8b(LINE_RANGE); u8b(OPCODE_BASE);
	emit(std_lens, sizeof std_lens);
	/* GNU SPLITS A PATH: the directory goes in the include_directories table (deduplicated, 1-based) and the
	 * file entry keeps only the basename plus that directory's index. A name with no '/' gets directory 0,
	 * meaning the compilation directory. Storing whole paths instead is legal DWARF but not byte-identical,
	 * and gcc passes absolute paths, so this is the common case rather than a corner. */
	char **dirs = NULL; int ndirs = 0;
	int *filedir = calloc((size_t)(nfiles ? nfiles : 1), sizeof *filedir);
	for (int i = 0; i < nfiles; i++) {
		if (!files[i]) continue;
		const char *slash = strrchr(files[i], '/');
		if (!slash) continue;                       /* no directory part: index 0 */
		size_t dl = (size_t)(slash - files[i]);
		if (!dl) dl = 1;                            /* "/x" -> the root directory */
		int found = 0;
		for (int d = 0; d < ndirs && !found; d++)
			if (!strncmp(dirs[d], files[i], dl) && !dirs[d][dl]) { filedir[i] = d + 1; found = 1; }
		if (!found) {
			dirs = realloc(dirs, (size_t)(ndirs + 1) * sizeof *dirs);
			dirs[ndirs] = malloc(dl + 1); memcpy(dirs[ndirs], files[i], dl); dirs[ndirs][dl] = 0;
			ndirs++; filedir[i] = ndirs;
		}
	}
	for (int d = 0; d < ndirs; d++) emit(dirs[d], strlen(dirs[d]) + 1);
	u8b(0);                                         /* end of include_directories */
	for (int i = 0; i < nfiles; i++) {              /* file_names: basename, dir index, mtime, length */
		if (!files[i]) continue;
		const char *slash = strrchr(files[i], '/');
		const char *base = slash ? slash + 1 : files[i];
		emit(base, strlen(base) + 1);
		uleb((unsigned long)filedir[i]); u8b(0); u8b(0);
	}
	u8b(0);                                         /* end of file_names */
	for (int d = 0; d < ndirs; d++) free(dirs[d]);
	free(dirs); free(filedir);
	patch32(ds, hdr_at, (u32)secs[ds].len - after_hdr_len);

	/* One sequence per section that has any .loc, in the order the directives appeared. */
	int done_any;
	int *seen = calloc((size_t)nsec, sizeof *seen);
	do {
		done_any = 0;
		int sec = -1;
		for (int i = 0; i < nloc; i++) if (!seen[locs[i].sec]) { sec = locs[i].sec; break; }
		if (sec < 0) break;
		seen[sec] = 1; done_any = 1;

		u32 caddr = 0; int cline = 1, cfile = 1, ccol = 0;
		/* GAS streams a column change when it READS the directive, while set_address is produced at the start
		 * of the sequence — so the first .loc's set_column lands BEFORE set_address, and every later one after
		 * its predecessor's row. Measured both ways; this reproduces the byte order. */
		for (int i = 0; i < nloc; i++) if (locs[i].sec == sec) {
			if (locs[i].col != ccol) { u8b(DW_LNS_set_column); uleb((unsigned long)locs[i].col); ccol = locs[i].col; }
			break;
		}
		u8b(0); uleb(1 + 4); u8b(DW_LNE_set_address);   /* extended: length, opcode, then the address */
		u32 at = (u32)secs[ds].len;
		emit32(0);
		add_reloc(ds, at, section_symbol(sec), md_reloc_by_name("R_ARM_ABS32"));

		for (int i = 0; i < nloc; i++) {
			if (locs[i].sec != sec) continue;
			if (locs[i].file != cfile) { u8b(DW_LNS_set_file); uleb((unsigned long)locs[i].file); cfile = locs[i].file; }
			if (locs[i].col != ccol) { u8b(DW_LNS_set_column); uleb((unsigned long)locs[i].col); ccol = locs[i].col; }
			row(locs[i].off, locs[i].line, &caddr, &cline);
		}
		/* the sequence covers the whole section: advance to its end, then end it */
		u32 end = (u32)secs[sec].len;
		if (end > caddr) { u8b(DW_LNS_advance_pc); uleb((end - caddr) / MIN_INST_LEN); }
		u8b(0); uleb(1); u8b(DW_LNE_end_sequence);
	} while (done_any);
	free(seen);

	patch32(ds, unit_at, (u32)secs[ds].len - unit_at - 4);
	cursec = save;
}

/* ---- the minimal compilation unit that makes the line table USABLE --------------------------------------
 * A line table on its own is not enough: with only .debug_line present, GNU addr2line answers "demo.c:?" --
 * it finds the file name and no line, because it locates the line program THROUGH a compilation unit. So a
 * CU DIE has to exist, pointing at the program with DW_AT_stmt_list.
 *
 * This is the smallest one that works, and deliberately NOT a copy of gcc's: gcc emits DWARF 5 with the whole
 * type and variable tree and strings indirected through .debug_str. Ours is DWARF 4 with inline strings and a
 * single childless DIE -- the same thing clang calls "line tables only". So .debug_line is byte-identical to
 * GNU as's, while .debug_info is ours and smaller by design.
 */
#define DW_TAG_compile_unit 0x11
#define DW_AT_name          0x03
#define DW_AT_stmt_list     0x10
#define DW_AT_low_pc        0x11
#define DW_AT_high_pc       0x12
#define DW_AT_comp_dir      0x1b
#define DW_FORM_addr        0x01
#define DW_FORM_data4       0x06
#define DW_FORM_string      0x08
#define DW_FORM_sec_offset  0x17

void dwinfo_emit(void) {
	if (!nloc || !nfiles) return;
	int save = cursec;

	int ab = sec_get(".debug_abbrev", SHT_PROGBITS, 0);
	cursec = ab;
	u32 ab_at = (u32)secs[ab].len;
	uleb(1); uleb(DW_TAG_compile_unit); u8b(0);            /* abbrev 1, no children */
	uleb(DW_AT_name); uleb(DW_FORM_string);
	uleb(DW_AT_comp_dir); uleb(DW_FORM_string);
	uleb(DW_AT_low_pc); uleb(DW_FORM_addr);
	uleb(DW_AT_high_pc); uleb(DW_FORM_data4);
	uleb(DW_AT_stmt_list); uleb(DW_FORM_sec_offset);
	uleb(0); uleb(0);                                      /* end of this abbrev's attributes */
	uleb(0);                                               /* end of the abbrev table */

	/* the first .loc's section is the one the CU describes (our cc emits code into one .text) */
	int code = locs[0].sec;
	char cwd[4096]; if (!getcwd(cwd, sizeof cwd)) cwd[0] = 0;

	int in = sec_get(".debug_info", SHT_PROGBITS, 0);
	cursec = in;
	u32 unit_at = (u32)secs[in].len;
	emit32(0);                                             /* unit_length, patched below */
	u8b(4); u8b(0);                                        /* version 4 */
	u32 abo_at = (u32)secs[in].len; emit32(0);             /* abbrev_offset -> .debug_abbrev */
	add_reloc(in, abo_at, section_symbol(ab), md_reloc_by_name("R_ARM_ABS32"));
	u8b(4);                                                /* address size */
	uleb(1);                                               /* abbrev code: our compile_unit */
	emit(files[0], strlen(files[0]) + 1);                  /* DW_AT_name */
	emit(cwd, strlen(cwd) + 1);                            /* DW_AT_comp_dir */
	u32 lo_at = (u32)secs[in].len; emit32(0);              /* DW_AT_low_pc -> the code section */
	add_reloc(in, lo_at, section_symbol(code), md_reloc_by_name("R_ARM_ABS32"));
	emit32((u32)secs[code].len);                           /* DW_AT_high_pc: its size (DWARF 4 allows a length) */
	u32 sl_at = (u32)secs[in].len; emit32(0);              /* DW_AT_stmt_list -> .debug_line */
	add_reloc(in, sl_at, section_symbol(sec_find(".debug_line")), md_reloc_by_name("R_ARM_ABS32"));
	patch32(in, unit_at, (u32)secs[in].len - unit_at - 4);
	(void)ab_at;
	cursec = save;
}
