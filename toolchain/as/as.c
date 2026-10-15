/*
 * as.c — the assembler FRONT-END (architecture-independent), the gas `read.c`/`as.c` equivalent.
 *
 * Reads a .s file, strips comments, and walks it line-by-line: peel labels, handle the GENERIC
 * directives every ELF target shares (.section/.global/.type/.size/.align/.word), and hand each
 * instruction to the machine-dependent backend via md_assemble(). It owns the symbol / section /
 * relocation / fixup tables + the byte buffers, exposing them to the backends as services (as.h);
 * it never encodes an instruction or writes an ELF byte itself. Arch-specific pseudo-ops fall through
 * to md_directive(); the ELF object is written by the obj backend (obj_write). One target is selected
 * at build time (we link exactly one md_* backend + one obj backend), exactly like GNU as.
 */
#define _POSIX_C_SOURCE 200809L   /* strdup */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include "as.h"
#include "strmap.h"

/* ------------------------------------------------------------------ tables (the shared model) ----- */
Section *secs; int nsec, cursec = -1; static int seccap;
/* GAS section model: `prevsec` = the section before the most recent switch (what `.previous` returns to);
 * .pushsection saves the (current, previous) PAIR and .popsection restores both. */
static int secstack[32][2], secsp, prevsec = -1;
Sym *syms; int nsym; static int symcap; static StrMap symindex;   /* name -> index + 1 (the first of that name) */
Reloc *rels; int nrel; static int relcap;
void *grow(void *v, int n, int *cap, size_t esz) {
	if (n < *cap) return v;
	*cap = *cap ? *cap * 2 : 256;
	if (!(v = realloc(v, (size_t)*cap * esz))) die("out of memory");
	return v;
}
int sym_add(Sym s) {
	syms = grow(syms, nsym, &symcap, sizeof *syms);
	syms[nsym] = s;
	if (s.name && s.name[0] != '$' && !strmap_get(&symindex, s.name)) strmap_put(&symindex, s.name, (void *)(long)(nsym + 1));
	return nsym++;                                   /* ($a/$d mapping symbols are markers, never looked up by name) */
}
/* Numeric local labels (GAS: ANY decimal number, e.g. the kernel's `9998:`) — GAS's design: definition k of N:
 * is a hidden local symbol `.Lfb<N>$<k>`; `Nb` names instance k (the latest), `Nf` instance k+1 (created now,
 * defined when the next N: appears). Everything else — branches, adr, ldr, data words, expressions — then uses
 * ordinary symbols, so forward refs, cross-section refs and `1f - 1b` all just work. */
static struct { int num, count; } *fbtab; static int nfbtab, fbcap;
static int fb_slot(int n) {
	for (int i = 0; i < nfbtab; i++) if (fbtab[i].num == n) return i;
	fbtab = grow(fbtab, nfbtab, &fbcap, sizeof *fbtab);
	fbtab[nfbtab].num = n; fbtab[nfbtab].count = 0; return nfbtab++;
}

void die(const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	fputs("as: ", stderr); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap); exit(1);
}

/* name -> index, O(1). This was a linear scan over every section, run from sec_new/sec_get on every .section
 * directive, so N sections cost O(N^2) strcmp calls — and the kernel really builds such objects:
 * .vmlinux.export.o has 26,968 section headers. Measured on a synthetic input, ours vs GNU as:
 * 1k 0.018s/0.009s, 4k 0.223s/0.034s, 13k 3.08s/0.114s, 27k 14.77s/0.277s — clean quadratic, 53x at the top.
 * Safe as a map because sections are only ever appended (nsec never decreases) and secs[i].name is strdup'd,
 * so the key strmap holds (it does not copy keys) outlives the table's reallocs. Values are index + 1, since
 * strmap_get returns NULL for absent. */
static StrMap sec_map;
int sec_find(const char *name) { return (int)(intptr_t)strmap_get(&sec_map, name) - 1; }
/* ARM mapping symbols ($a = ARM code starts here, $d = data), placed with GAS's rules (tc-arm.c mapping_state /
 * arm_init_frag / check_mapping_symbols): per-section state UNDEF/ARM/DATA. An instruction enters ARM (and if the
 * section was UNDEF with bytes already in it, a $d at 0 first). A data directive enters DATA, but from UNDEF
 * that's DEFERRED (no symbol). An alignment/fill "frag" (.align/.space/...) marks DATA at its start even from
 * UNDEF; code-section alignment pads with NOPs ($a) after any sub-word zero bytes ($d). Two mapping symbols at
 * one address: the later wins; one at the very end of a section is dropped. objdump and ld rely on these. */
enum { MAP_UNDEF, MAP_ARM, MAP_DATA, MAP_THUMB };
int thumb_mode;
/* The mapping state is the SECTION's, shared by its subsections (GAS keeps it per segment): after `.subsection 1;
 * .long x; .previous` the next instruction gets a $a even though its own subsection was in ARM state. */
static int mstate_sec(int s) { return secs[s].base >= 0 ? secs[s].base : s; }
static int is_mapsym(int k) { const char *n = syms[k].name; return n && n[0] == '$' && (n[1] == 'a' || n[1] == 'd' || n[1] == 't') && !n[2]; }
static char *mapname(int state) { return state == MAP_ARM ? "$a" : state == MAP_THUMB ? "$t" : "$d"; }
static void add_mapsym_in(int sec, int state, u32 value) {
	int l = secs[mstate_sec(sec)].lastmap;           /* same address as this section's latest mapping symbol: replace it */
	if (l >= 0 && syms[l].sec == sec && syms[l].value == value && l != secs[sec].lead) syms[l].name = NULL;   /* (not a leading
	                                                   * .align's: that is its own frag, settled when the subsection lands) */
	secs[mstate_sec(sec)].lastmap = sym_add((Sym){ .name = mapname(state), .sec = sec, .value = value, .type = STT_NOTYPE, .defined = 1 });
}
static void add_mapsym(int state, u32 value) { add_mapsym_in(cursec, state, value); }
static void map_to(int state, int deferred_ok) {   /* deferred_ok: a plain data directive (UNDEF->DATA waits) */
	if (cursec < 0) return;   /* NOBITS too: GAS marks .bss alignment/fill frags with $d */
	int *m = &secs[mstate_sec(cursec)].mapstate; u32 o = (u32)secs[cursec].len;
	if (*m == state) return;
	if (*m == MAP_UNDEF && state == MAP_DATA && deferred_ok) return;
	/* entering code with bytes before it — in this frag chain, or (any later subsection) in an earlier one: $d at the
	 * section's first byte */
	if (*m == MAP_UNDEF && state != MAP_DATA && (o > 0 || secs[cursec].base >= 0)) add_mapsym_in(mstate_sec(cursec), MAP_DATA, 0);
	*m = state; add_mapsym(state, o);
}
static void sec_align_at_least(u32 a) { if (cursec >= 0 && secs[cursec].align < a) secs[cursec].align = a; }
/* An instruction: ARM or Thumb state here (GAS mapping_state records the section's alignment for it: 4, or 2), and
 * its frag now holds code of that state. */
void map_insn(void) { map_to(thumb_mode ? MAP_THUMB : MAP_ARM, 0); sec_align_at_least(thumb_mode ? 2 : 4); if (cursec >= 0) secs[cursec].fragmode = thumb_mode; }
static int frag_mode(int s) { return secs[s].fragmode >= 0 ? secs[s].fragmode : thumb_mode; }   /* arm_init_frag's frag_thumb_mode */
static void new_frag(void) { if (cursec >= 0) secs[cursec].fragmode = -1; }   /* after GAS's frag_var: a fresh frag, no mode yet */
void map_pool_data(void) { secs[mstate_sec(cursec)].mapstate = MAP_DATA; add_mapsym(MAP_DATA, (u32)secs[cursec].len); }   /* s_ltorg: $d unconditionally */
/* GAS writes code-alignment padding (and its $d/$a pair) after parsing, so those symbols come LAST in the table. */
static struct { int sec; u32 d_at, a_at; int state; } *padmap; static int npadmap, padcap;
static const char *mapkey(int sec, u32 at) { static char k[32]; snprintf(k, sizeof k, "%d:%u", sec, at); return k; }
static void flush_padmaps(void) {
	StrMap at = {0};                                 /* "sec:address" -> index + 1 of the live mapping symbol there */
	for (int k = 0; k < nsym; k++) if (syms[k].name && syms[k].name[0] == '$') strmap_put(&at, strdup(mapkey(syms[k].sec, syms[k].value)), (void *)(long)(k + 1));
	for (int i = 0; i < npadmap; i++) {
		int sec = padmap[i].sec;
		long d = (long)strmap_get(&at, mapkey(sec, padmap[i].d_at)), a = (long)strmap_get(&at, mapkey(sec, padmap[i].a_at));
		if (d && syms[d - 1].name) syms[d - 1].name = NULL;                   /* insert_data_mapping_symbol: replace */
		int have_a = a && syms[a - 1].name && a != d;                         /* next frag already starts with one */
		strmap_put(&at, strdup(mapkey(sec, padmap[i].d_at)), (void *)(long)(sym_add((Sym){ .name = "$d", .sec = sec, .value = padmap[i].d_at, .type = STT_NOTYPE, .defined = 1 }) + 1));
		if (!have_a) strmap_put(&at, strdup(mapkey(sec, padmap[i].a_at)), (void *)(long)(sym_add((Sym){ .name = mapname(padmap[i].state), .sec = sec, .value = padmap[i].a_at, .type = STT_NOTYPE, .defined = 1 }) + 1));
	}
}
static void map_data(void) { map_to(MAP_DATA, 1); }
void map_section_data(void) { if (secs[mstate_sec(cursec)].mapstate != MAP_DATA) map_pool_data(); }   /* unwind tables: GAS marks them $d at once */
static void map_frag_data(void) { map_to(MAP_DATA, 0); new_frag(); }
/* .arm / .thumb / .code (GAS opcode_select): a real change records alignment 2 on the section; going back to ARM
 * also word-aligns with a data alignment frag (its $d, zero fill) */
void mode_switch(int to_thumb) {
	if (to_thumb == thumb_mode) return;
	if (!to_thumb && cursec >= 0) {
		map_frag_data();
		if (secs[cursec].type == SHT_NOBITS) secs[cursec].len = (secs[cursec].len + 3) & ~(size_t)3;
		else while (secs[cursec].len % 4) { u8 z = 0; emit(&z, 1); }
	}
	thumb_mode = to_thumb;
	sec_align_at_least(2);
}
static void drop_end_mapsyms(void) {
	for (int i = 0; i < nsym; i++) if (is_mapsym(i) && syms[i].sec >= 0 && syms[i].value == (u32)secs[syms[i].sec].len) syms[i].name = NULL;
}
int section_symbol(int sec);
static int sec_new(const char *name, u32 type, u32 flags) {
	int i = sec_find(name); if (i >= 0) { cursec = i; return i; }
	secs = grow(secs, nsec, &seccap, sizeof *secs);
	secs[nsec] = (Section){ .name = strdup(name), .type = type, .flags = flags, .align = 1, .mapstate = MAP_UNDEF, .lastmap = -1, .secsym = -1, .link = -1, .base = -1, .lead = -1, .fragmode = -1 };
	strmap_put(&sec_map, secs[nsec].name, (void *)(intptr_t)(nsec + 1));   /* the strdup'd name is the key (see sec_find) */
	return cursec = nsec++;
}
int sec_get(const char *name, u32 type, u32 flags) {
	int known = sec_find(name) >= 0, i = sec_new(name, type, flags);
	if (!known) section_symbol(i);   /* GAS makes the section symbol when the section is created */
	return i;
}
static int sec_get_nosym(const char *name, u32 type, u32 flags) { return sec_new(name, type, flags); }   /* its symbol: at write time */
/* Subsection n of section b: assembled on its own (an internal section with no symbol), appended to b in order after
 * parsing (merge_subsections). 0 is b itself. */
static int subsection_of(int b, long n) {
	if (secs[b].base >= 0) b = secs[b].base;
	if (n == 0) return b;
	if (n < 0 || n > 8192) die(".subsection %ld: out of range", n);
	char nm[300]; snprintf(nm, sizeof nm, "%s\001%ld", secs[b].name, n);
	int i = sec_find(nm);
	if (i < 0) { i = sec_new(nm, secs[b].type, secs[b].flags); secs[i].base = b; secs[i].subsec = (int)n; secs[b].nsub++; }
	return i;
}
static int map_pending;   /* an instruction is being assembled: map ARM state at its first byte */
void emit(const void *p, size_t n) {
	if (map_pending) { map_pending = 0; map_insn(); }
	Section *s = &secs[cursec];
	if (s->len + n > s->cap) { s->cap = (s->len + n) * 2 + 64; s->data = realloc(s->data, s->cap); }
	memcpy(s->data + s->len, p, n); s->len += n;
}
void emit32(u32 w) { if (cursec < 0) die("instruction outside any section"); u8 b[4] = { w, w >> 8, w >> 16, w >> 24 }; emit(b, 4); }
void emit16(u32 h) { if (cursec < 0) die("instruction outside any section"); u8 b[2] = { h, h >> 8 }; emit(b, 2); }
void patch32(int sec, u32 off, u32 w) { u8 *d = secs[sec].data + off; d[0]=w; d[1]=w>>8; d[2]=w>>16; d[3]=w>>24; }
u32  read32(int sec, u32 off) { u8 *d = secs[sec].data + off; return d[0] | d[1]<<8 | d[2]<<16 | (u32)d[3]<<24; }
u32  here(void) { if (cursec < 0) die("instruction/label outside any section"); return secs[cursec].len; }

int sym_find(const char *name) { return (int)(long)strmap_get(&symindex, name) - 1; }
int sym_intern(const char *name) {
	int i = sym_find(name); if (i >= 0) return i;
	return sym_add((Sym){ .name = strdup(name), .type = STT_NOTYPE });   /* global=0 (local until .global'd) */
}
u32 parse_seq; long reloc_seq = -1;
static int *seq_sub; static u32 seq_subcap;   /* statement -> the subsection it was in (a fixup placed later keeps it) */
static int sub_of(int sec) { return sec >= 0 && secs[sec].base >= 0 ? secs[sec].subsec : 0; }
void add_reloc(int sec, u32 off, int symidx, u32 type) {
	rels = grow(rels, nrel, &relcap, sizeof *rels);
	rels[nrel++] = (Reloc){ sec, off, symidx, type, reloc_seq >= 0 ? (u32)reloc_seq : parse_seq, reloc_seq >= 0 ? seq_sub[reloc_seq] : sub_of(sec), 0 };
}
static int fb_intern(int n, int k) { char nm[40]; snprintf(nm, sizeof nm, ".Lfb%d$%d", n, k); return sym_intern(nm); }
int fb_symbol(int n, char dir) {
	int i = fb_slot(n);
	if (dir == 'b') { if (!fbtab[i].count) die("backward reference to undefined local label %db", n); return fb_intern(n, fbtab[i].count); }
	return fb_intern(n, fbtab[i].count + 1);
}
static void fb_define(int n, u32 value) {   /* N: — define the next instance */
	int i = fb_slot(n), si = fb_intern(n, ++fbtab[i].count);
	syms[si].sec = cursec; syms[si].value = value; syms[si].defined = 1;
}
int  parse_local_ref(const char *s, int *n, char *dir) {
	int k = 0; long v = 0;
	while (isdigit((unsigned char)s[k])) { v = v * 10 + (s[k] - '0'); k++; if (v > 0x7fffffff) return 0; }
	if (!k || (s[k] != 'b' && s[k] != 'f')) return 0;
	if (isalnum((unsigned char)s[k + 1]) || s[k + 1] == '_' || s[k + 1] == '.' || s[k + 1] == '$') return 0;
	*n = (int)v; *dir = s[k]; return k + 1;
}

/* ------------------------------------------------------------------ lexing ------------------------ */
/* Strip C-style block comments (which may span lines) + @ and // line comments, in place, replacing
   them with spaces while preserving newlines so line boundaries stay intact. */
static void include_text(char *buf);
static void code_pad(u32 bytes, int thumb);
static struct { int sym; char *text; int sec; u32 at; } *dset; static int ndset, dsetcap;   /* .set with forward references */
static struct { XVal off; u32 type; XVal sym; u32 seq; int sec; } *dreloc; static int ndreloc, dreloccap;   /* .reloc (sec: the
                                                                                     * section a constant offset is in) */
int file_front = -1;   /* the first .file symbol: written first in the symbol table (GAS elf_file_symbol) */
/* -I directories, for .include / .incbin (after the name as given, relative to the working directory — GAS). */
static const char *incdirs[64]; static int nincdirs;
static char *decode_quoted(const char *tok) {   /* "a\"b" -> a"b (the file names .incbin/.include take) */
	size_t l = strlen(tok); char *o = malloc(l + 1), *w = o;
	for (size_t i = 1; i + 1 < l; i++) { if (tok[i] == '\\' && i + 2 < l) i++; *w++ = tok[i]; }
	*w = 0; return o;
}
static u8 *read_included(const char *tok, size_t *n, const char *what) {
	char *name = decode_quoted(tok); FILE *f = fopen(name, "rb"); char path[1024];
	for (int i = 0; !f && i < nincdirs && name[0] != '/'; i++) { snprintf(path, sizeof path, "%s/%s", incdirs[i], name); f = fopen(path, "rb"); }
	if (!f) die("%s: cannot open \"%s\"", what, name);
	size_t cap = 1 << 16, r; u8 *b = malloc(cap); *n = 0;
	while ((r = fread(b + *n, 1, cap - *n - 1, f)) > 0) { *n += r; if (*n + 1 >= cap) b = realloc(b, cap *= 2); }
	b[*n] = 0; fclose(f); free(name); return b;
}
static void strip_comments(char *s) {
	int in_block = 0, inq = 0;
	for (char *p = s; *p; p++) {
		if (in_block) { if (p[0] == '*' && p[1] == '/') { *p++ = ' '; *p = ' '; in_block = 0; } else if (*p != '\n') *p = ' '; continue; }
		if (inq) {   /* inside "...": '@', '//' and block-comment openers are string bytes (kernel format strings: "%pS @ %i") */
			if (*p == '\\' && p[1] && p[1] != '\n') p++;   /* skip an escaped char (incl. \") */
			else if (*p == '"' || *p == '\n') inq = 0;
			continue;
		}
		if (*p == '"') { inq = 1; continue; }
		if (p[0] == '/' && p[1] == '*') { *p++ = ' '; *p = ' '; in_block = 1; continue; }
		if ((p[0] == '@' && !(p > s && p[-1] == '\\')) || (p[0] == '/' && p[1] == '/')) {   /* `\@` = macro counter, not a comment */ while (*p && *p != '\n') *p++ = ' '; if (!*p) break; }
	}
}

/* Split a line into tokens on whitespace + commas ('#','{','}','[',']' stay attached to their operand). */
#define MAXTOK 4100  /* mnemonic + operands: a data directive's list is as long as the line (8K) allows */
static char *toks[MAXTOK]; static int ntok; static char linebuf[8192];   /* kernel .ascii/.asciz lines can be long */
/* Inside a `#` immediate, does the whitespace at p end it? Yes if only whitespace (then a comma or end) follows. */
static int imm_ends_here(const char *p) { while (*p == ' ' || *p == '\t') p++; return *p == ',' || *p == 0 || *p == '\n'; }
static void tokenize(const char *line) {
	ntok = 0;
	size_t ll = strcspn(line, "\n"); if (ll >= sizeof linebuf) die("line too long (%zu > %zu bytes)", ll, sizeof linebuf - 1);   /* was silently truncated */
	memcpy(linebuf, line, ll); linebuf[ll] = 0;
	char *p = linebuf;
	while (*p) {
		while (*p == ' ' || *p == '\t' || *p == ',') p++;
		if (!*p || *p == '\n') break;
		if (ntok >= MAXTOK) die("too many tokens on a line");
		toks[ntok++] = p;
		int inq = 0, isimm = (*p == '#' || *p == ':' || (ntok > 1 && *p == '(')), depth = 0;   /* `(1 << 15) | 1`: an operand expression */   /* `#expr` and `:pc_g0:(expr)` group-reloc operands */   /* inside "..." spaces/commas are part of the token (.ascii "a b") */
		while (*p && (inq || (*p != '\t' && *p != ',' && *p != '\n' && *p != ' ') || (isimm && depth > 0 && *p != '\n')
		              || (isimm && (*p == ' ' || *p == '\t') && !imm_ends_here(p)))) {
			/* a `#` immediate is an EXPRESSION: it keeps its spaces up to the next top-level comma
			 * (`#(. - bar - 8) & 0xff`) — was split at the first space and silently mis-parsed */
			if (*p == '\\' && p[1]) { p += 2; continue; }   /* skip an escaped char (incl. \") */
			if (!inq && *p == '\'') {                       /* a character constant keeps its blanks AND commas:
			                                                * `mov r0, #','` is one operand (GAS allows a bare 'c too) */
				p++;
				if (*p == '\\' && p[1]) p += 2; else if (*p) p++;
				if (*p == '\'') p++;
				continue;
			}
			if (*p == '"') inq = !inq;
			if (!inq && *p == '(') depth++; else if (!inq && *p == ')' && depth) depth--;
			p++;
		}
		if (isimm) { char *e = p; while (e > toks[ntok - 1] && (e[-1] == ' ' || e[-1] == '\t')) *--e = 0; }   /* trim */
		if (*p) *p++ = 0;
	}
}

/* ------------------------------------------------------------------ labels + directives ----------- */
static int thumb_func_pending;
void thumb_func_next_label(void) { thumb_func_pending = 1; }
static void def_label(const char *name) {
	if (cursec < 0) die("label '%s' outside any section", name);
	{ const char *q = name; while (isdigit((unsigned char)*q)) q++;
	  if (q != name && !*q) { long v = strtol(name, NULL, 10); if (v > 0x7fffffff) die("local label %s too large", name); fb_define((int)v, (u32)secs[cursec].len); return; } }
	int i = sym_intern(name);
	if (syms[i].defined) die("symbol '%s' is already defined", name);   /* GAS: a label may be defined once (.set may redefine) */
	syms[i].sec = cursec; syms[i].value = secs[cursec].len; syms[i].defined = 1; syms[i].thumb = thumb_mode;
	if (thumb_func_pending && strncmp(name, ".L", 2) && (secs[cursec].flags & SHF_EXECINSTR)) {   /* GAS arm_frob_label */
		syms[i].thumb_func = 1; syms[i].type = STT_FUNC; thumb_func_pending = 0; }
}

/* Emit the bytes of a C-string token like "\"Unknown error\000\"" (quotes included), decoding escapes
 * (\ooo octal, \xHH.. hex, \n \t \r \b \f \\ \" \0). add_nul appends a terminating NUL (.asciz), else not (.ascii). */
static struct { int sym; char *target; } *alias_of; static int nalias, aliascap;   /* pending `.set name, target` */
static struct { int sym, target; } *alias_done; static int nalias_done, donecap;     /* resolved, in resolution order */
static void emit_string(const char *tok, int add_nul) {
	const char *p = tok; if (*p == '"') p++;
	while (*p && *p != '"') {
		u8 b;
		if (*p == '\\') {
			p++;
			if (*p >= '0' && *p <= '7') { int v = 0, n = 0; while (*p >= '0' && *p <= '7' && n < 3) { v = v*8 + (*p++ - '0'); n++; } b = (u8)v; }
			else if ((*p == 'x' || *p == 'X') && isxdigit((unsigned char)p[1])) {   /* GAS: \x takes ALL following hex digits, low byte kept */
				unsigned v = 0; p++; while (isxdigit((unsigned char)*p)) { v = v * 16 + (isdigit((unsigned char)*p) ? *p - '0' : (tolower((unsigned char)*p) - 'a' + 10)); p++; } b = (u8)v; }
			else { switch (*p) { case 'n': b='\n'; break; case 't': b='\t'; break; case 'r': b='\r'; break;
			                     case 'b': b='\b'; break; case 'f': b='\f'; break; default: b=(u8)*p; } p++; }
		} else b = (u8)*p++;
		emit(&b, 1);
	}
	if (add_nul) { u8 z = 0; emit(&z, 1); }
}

/* GENERIC directives (shared by every ELF target). Arch pseudo-ops fall through to md_directive(). */
/* Switch to the section named in toks[1] (optional flag string in toks[2]), defaulting flags/type by the
 * well-known name — the shared body of .section / .pushsection. */
static void select_section(void) {
	u32 type = SHT_PROGBITS, flags = 0; char nmbuf[128]; const char *nm = toks[1];
	if (nm[0] == '"') { size_t l = strlen(nm + 1); if (l && nm[l] == '"') l--; if (l >= sizeof nmbuf) die(".section: name too long");
		memcpy(nmbuf, nm + 1, l); nmbuf[l] = 0; nm = nmbuf; }   /* `.section ".export_symbol","a"`: the quotes aren't part of the name */
	/* Without an explicit flag string, GAS gives a WELL-KNOWN name its standard flags and type (elf_special_sections):
	 * some match a `.name.suffix` too (`.text.foo`, `.data.rel.ro`, `.rodata.str1.1`), the rest only exactly — `.init`
	 * is AX but `.init.x` is a plain section, and `.textfoo` is NOT `.text`. Any other name: PROGBITS, no flags. */
	static const struct { const char *n; u32 flags, type; int suffix; } special[] = {
		{ ".text",          SHF_ALLOC | SHF_EXECINSTR,   SHT_PROGBITS, 1 },
		{ ".data",          SHF_ALLOC | SHF_WRITE,       SHT_PROGBITS, 1 },
		{ ".rodata",        SHF_ALLOC,                   SHT_PROGBITS, 1 },
		{ ".bss",           SHF_ALLOC | SHF_WRITE,       SHT_NOBITS,   1 },
		{ ".tdata",         SHF_ALLOC | SHF_WRITE | 0x400, SHT_PROGBITS, 1 },   /* 0x400 = SHF_TLS */
		{ ".tbss",          SHF_ALLOC | SHF_WRITE | 0x400, SHT_NOBITS,   1 },
		{ ".init_array",    SHF_ALLOC | SHF_WRITE,       14,           1 },
		{ ".fini_array",    SHF_ALLOC | SHF_WRITE,       15,           1 },
		{ ".preinit_array", SHF_ALLOC | SHF_WRITE,       16,           1 },
		{ ".note",          0,                           7,            1 },
		{ ".init",          SHF_ALLOC | SHF_EXECINSTR,   SHT_PROGBITS, 0 },   /* musl's crti.s: `.section .init` */
		{ ".fini",          SHF_ALLOC | SHF_EXECINSTR,   SHT_PROGBITS, 0 },
		{ ".ARM.exidx",     SHF_ALLOC | 0x80,            0x70000001,   0 },   /* 0x80 = SHF_LINK_ORDER */
		{ ".ARM.attributes", 0,                          0x70000003,   0 },
	};
	for (unsigned i = 0; i < sizeof special / sizeof *special; i++) {
		size_t l = strlen(special[i].n);
		if (strncmp(nm, special[i].n, l)) continue;
		if (nm[l] && !(special[i].suffix && nm[l] == '.')) continue;
		flags = special[i].flags; type = special[i].type; break;
	}
	u32 entsize = 0;
	if (ntok >= 3 && toks[2][0] == '"') { flags = 0;   /* explicit "flags" overrides: GAS letters */
		for (const char *f = toks[2] + 1; *f && *f != '"'; f++) switch (*f) {
		case 'a': flags |= SHF_ALLOC; break;
		case 'w': flags |= SHF_WRITE; break;
		case 'x': flags |= SHF_EXECINSTR; break;
		case 'M': flags |= SHF_MERGE; break;
		case 'S': flags |= SHF_STRINGS; break;
		case 'T': flags |= 0x400; break;   /* SHF_TLS */
		default: die(".section %s: unsupported flag '%c'", nm, *f);
		}
		if (ntok >= 4) {   /* type: %progbits / @nobits / ... */
			const char *t = toks[3] + (toks[3][0] == '%' || toks[3][0] == '@');
			if      (!strcmp(t, "progbits")) type = SHT_PROGBITS;
			else if (!strcmp(t, "nobits")) type = SHT_NOBITS;
			else if (!strcmp(t, "note")) type = 7;
			else if (!strcmp(t, "init_array")) type = 14;
			else if (!strcmp(t, "fini_array")) type = 15;
			else if (!strcmp(t, "preinit_array")) type = 16;
			else die(".section %s: unsupported type '%s'", nm, toks[3]);
		}
		if (flags & SHF_MERGE) { if (ntok < 5) die(".section %s: M needs an entity size", nm); entsize = (u32)eval_const_expr(toks[4]); }
	}
	int explicit = ntok >= 3 && toks[2][0] == '"', old = sec_find(nm);
	if (explicit && old >= 0 && secs[old].flags != flags) die(".section %s: changed section attributes", nm);   /* GAS: an error, not a silent merge */
	int si = sec_get(nm, type, flags);
	if (entsize) secs[si].entsize = entsize;
}

static const char *cur_stmt;   /* raw text of the current statement (labels peeled) — for expression operands */

/* ---- data-directive expressions: `.word/.long e1, e2, ...` ---------------------------------------
 * A relocatable value is  c + addr(sym) + dot*P  (sym<0: none; P = the address of the word being emitted).
 * Enough for everything kernel asm puts in data: constants with ( ) and C operators (BUG's
 * `((0xe7f001f2) & 0xFFFFFFFF)`), `sym+k`, numeric local labels `1b` (in ANY section — they become the
 * label's section symbol + its offset), and PC-relative `9998b - .` (alternatives tables). */
/* c + sym - msym + dot*P; rtype: explicit reloc from `sym(OP)`. A bare `.` is dot=+1 and `-` subtracts it
 * (e_add: `a.dot -= b.dot`), so `9998b - .` is dot=-1 and every consumer ADDS dot*P. */
typedef struct { long c; int sym, dot, msym; u32 rtype; } RVal;
static const char *ep;
static void ews(void) { while (*ep == ' ' || *ep == '\t') ep++; }
static RVal e_or(void);
static RVal rconst(long c) { RVal r = { c, -1, 0, -1, 0 }; return r; }
/* Soft mode: a non-constant operand of * / >> & ... (a forward reference, resolved only at the end) marks the whole
 * expression for later instead of dying; the caller re-evaluates its TEXT once every label is known. */
static int expr_soft, expr_failed;
static long dot_at = -1;   /* re-evaluating a deferred expression: `.` is where it appeared (else the current spot) */
static long cur_dot(void) { return dot_at >= 0 ? dot_at : (long)secs[cursec].len; }
static void need_const(RVal a, const char *op) {
	if (!(a.sym >= 0 || a.dot || a.msym >= 0 || a.rtype)) return;
	if (expr_soft) { expr_failed = 1; return; }
	die("data expr: operator '%s' needs constant operands", op);
}
static RVal e_prim(void) {
	ews();
	if (*ep == '(') { ep++; RVal r = e_or(); ews(); if (*ep != ')') die("data expr: expected ')' in '%s'", cur_stmt); ep++; return r; }
	if (*ep == '-') {   /* -(a - b) is b - a, -(. - a) is a - .; negating a lone symbol is an error (or, soft, for later) */
		ep++; RVal r = e_prim();
		if (!r.rtype && r.msym >= 0 && ((r.sym >= 0 && !r.dot) || (r.sym < 0 && r.dot == 1))) {
			int t = r.sym; r.sym = r.msym; r.msym = t; r.dot = -r.dot; r.c = -r.c; return r; }
		need_const(r, "unary -"); r.c = -r.c; return r;
	}
	if (*ep == '~') { ep++; RVal r = e_prim(); need_const(r, "~"); r.c = ~r.c; return r; }
	if (*ep == '!' && ep[1] != '=') { ep++; RVal r = e_prim(); need_const(r, "!"); return rconst(!r.c); }   /* logical not: 1 / 0 */
	if (*ep == '+') { ep++; return e_prim(); }
	{ int n; char dir; int k = parse_local_ref(ep, &n, &dir); if (k) { ep += k;
		RVal r = { 0, fb_symbol(n, dir), 0, -1, 0 }; return r; } }
	if (*ep == '\'' && ep[1]) {   /* GAS char constant 'c (escapes: '\\ '\n '\t '\a ...; closing quote optional) */
		long c; ep++;
		/* GAS (checked): only \b \f \n \r \t are control chars; any other '\x is x itself ('\a = 'a', '\0 = '0') */
		if (*ep == '\\' && ep[1]) { ep++; switch (*ep) { case 'n': c = 10; break; case 't': c = 9; break; case 'r': c = 13; break;
			case 'b': c = 8; break; case 'f': c = 12; break; default: c = (unsigned char)*ep; } ep++; }
		else c = (unsigned char)*ep++;
		if (*ep == '\'') ep++;
		return rconst(c); }
	if (isdigit((unsigned char)*ep)) { char *e; unsigned long long v = strtoull(ep, &e, 0); ep = e; while (*ep == 'u' || *ep == 'U' || *ep == 'l' || *ep == 'L') ep++; return rconst((long)v); }
	if (*ep == '.' && !(isalnum((unsigned char)ep[1]) || ep[1] == '_' || ep[1] == '.' || ep[1] == '$')) { ep++; RVal r = { 0, -1, 1, -1, 0 }; return r; }
	if (isalpha((unsigned char)*ep) || *ep == '_' || *ep == '.' || *ep == '$') {
		char nm[128]; int k = 0;
		while ((isalnum((unsigned char)*ep) || *ep == '_' || *ep == '.' || *ep == '$') && k < 127) nm[k++] = *ep++;
		nm[k] = 0; int si = sym_intern(nm);
		if (syms[si].defined && syms[si].sec == SEC_ABS) return rconst((long)(int32_t)syms[si].value);   /* `.equ N, 16` */
		RVal r = { 0, si, 0, -1, 0 };
		if (*ep == '(') {   /* `sym(OP)`: an explicit relocation (GOT, GOTOFF, GOT_PREL, TARGET1/2, SBREL, TLS...) */
			const char *q = ep + 1; char op[24]; int k2 = 0;
			while ((isalnum((unsigned char)*q) || *q == '_') && k2 < 23) op[k2++] = *q++;
			op[k2] = 0;
			if (*q == ')' && k2) { if (!md_reloc_operator(op, &r.rtype)) die("unknown relocation operator '(%s)' in '%s'", op, cur_stmt); ep = q + 1; }
		}
		return r;
	}
	die("data expr: bad operand near '%s' in '%s'", ep, cur_stmt); return rconst(0);
}
/* Binary operators at GAS's precedence (expr.c op_rank), which is NOT C's: rank 9 `* / % << >>`, rank 8 `| & ^ !`
 * (`!` = or-not), rank 7 `+ -`, rank 4 `== != <> < <= > >=` (true is -1), rank 3 `&&`, rank 2 `||` (1 / 0); all
 * left-associative. So `1 + 2 << 3` is 17 and `1 | 2 ^ 3` is 0 (was C's 24 and 1: silently different values). */
static RVal e_mul(void) {   /* rank 9 */
	RVal a = e_prim();
	for (;;) { ews();
		if (*ep == '*') { ep++; RVal b = e_prim(); need_const(a, "*"); need_const(b, "*"); a.c *= b.c; }
		else if (*ep == '/') { ep++; RVal b = e_prim(); need_const(a, "/"); need_const(b, "/"); if (expr_failed) continue; if (!b.c) die("data expr: divide by 0"); a.c /= b.c; }
		else if (*ep == '%') { ep++; RVal b = e_prim(); need_const(a, "%"); need_const(b, "%"); if (expr_failed) continue; if (!b.c) die("data expr: modulo by 0"); a.c %= b.c; }
		else if (ep[0] == '<' && ep[1] == '<') { ep += 2; RVal b = e_prim(); need_const(a, "<<"); need_const(b, "<<"); a.c = (unsigned long)b.c >= 64 ? 0 : (long)((unsigned long)a.c << b.c); }
		else if (ep[0] == '>' && ep[1] == '>') { ep += 2; RVal b = e_prim(); need_const(a, ">>"); need_const(b, ">>"); a.c = (unsigned long)b.c >= 64 ? 0 : (long)((unsigned long)a.c >> b.c); }   /* logical, as GAS */
		else return a; }
}
static RVal e_bit(void) {   /* rank 8 */
	RVal a = e_mul();
	for (;;) { ews();
		if (*ep == '|' && ep[1] != '|') { ep++; RVal b = e_mul(); need_const(a, "|"); need_const(b, "|"); a.c |= b.c; }
		else if (*ep == '&' && ep[1] != '&') { ep++; RVal b = e_mul(); need_const(a, "&"); need_const(b, "&"); a.c &= b.c; }
		else if (*ep == '^') { ep++; RVal b = e_mul(); need_const(a, "^"); need_const(b, "^"); a.c ^= b.c; }
		else if (*ep == '!' && ep[1] != '=') { ep++; RVal b = e_mul(); need_const(a, "!"); need_const(b, "!"); a.c |= ~b.c; }
		else return a; }
}
static RVal e_add(void) {   /* rank 7 */
	RVal a = e_bit();
	for (;;) { ews();
		if (*ep == '+') { ep++; RVal b = e_bit(); if (a.sym >= 0 && b.sym >= 0) die("data expr: sym + sym");
			if (a.msym >= 0 && b.msym >= 0) die("data expr: too many symbol differences");
			a.c += b.c; if (b.sym >= 0) { a.sym = b.sym; a.rtype = b.rtype; } if (b.msym >= 0) a.msym = b.msym; a.dot += b.dot; }
		else if (*ep == '-') { ep++; RVal b = e_bit(); a.c -= b.c; a.dot -= b.dot;
			if (b.sym < 0 && a.sym >= 0 && a.dot == -1 && syms[a.sym].defined && syms[a.sym].sec == cursec) {   /* sym - . (same section): constant */
				a.c += (long)syms[a.sym].value - cur_dot(); a.sym = -1; a.dot = 0; continue; }
			if (b.sym >= 0) {   /* sym - sym: fine when both resolve into the same section */
				if (a.sym < 0 && a.dot == 1 && syms[b.sym].defined && syms[b.sym].sec == cursec) {   /* (. + k) - sym, same section */
					a.c += cur_dot() - (long)syms[b.sym].value; a.dot = 0; continue; }
				if (a.sym < 0) die("data expr: const - sym");
				if (b.msym >= 0) die("data expr: subtracting a symbol difference");
				Sym *x = &syms[a.sym], *y = &syms[b.sym];
				if (x->defined && y->defined && x->sec == y->sec) { a.c += (long)x->value - (long)y->value; a.sym = -1; }
				else if (a.msym < 0) a.msym = b.sym;   /* not resolvable yet (forward, e.g. `1f - 1b`) or cross-section: defer */
				else die("data expr: too many symbol differences");
			} }
		else return a; }
}
static int cmp_operands(RVal *a, RVal *b, const char *op) {   /* both constant, or two symbols' places in one section */
	if (a->sym >= 0 && b->sym >= 0 && !a->dot && !b->dot && a->msym < 0 && b->msym < 0 && syms[a->sym].defined && syms[b->sym].defined
	    && syms[a->sym].sec == syms[b->sym].sec) { a->c += syms[a->sym].value; b->c += syms[b->sym].value; a->sym = b->sym = -1; }
	need_const(*a, op); need_const(*b, op); return 1;
}
static RVal e_cmp(void) {   /* rank 4: -1 true, 0 false */
	RVal a = e_add();
	for (;;) { ews();
		int op = 0; const char *nm = "";
		if (ep[0] == '=' && ep[1] == '=') { op = 1; nm = "=="; ep += 2; }
		else if ((ep[0] == '!' && ep[1] == '=') || (ep[0] == '<' && ep[1] == '>')) { op = 2; nm = "!="; ep += 2; }
		else if (ep[0] == '<' && ep[1] == '=') { op = 3; nm = "<="; ep += 2; }
		else if (ep[0] == '>' && ep[1] == '=') { op = 4; nm = ">="; ep += 2; }
		else if (ep[0] == '<' && ep[1] != '<') { op = 5; nm = "<"; ep++; }
		else if (ep[0] == '>' && ep[1] != '>') { op = 6; nm = ">"; ep++; }
		else return a;
		RVal b = e_add(); cmp_operands(&a, &b, nm);
		int t = op == 1 ? a.c == b.c : op == 2 ? a.c != b.c : op == 3 ? a.c <= b.c : op == 4 ? a.c >= b.c : op == 5 ? a.c < b.c : a.c > b.c;
		a = rconst(t ? -1 : 0);
	}
}
static RVal e_land(void) {   /* rank 3 */
	RVal a = e_cmp();
	for (;;) { ews(); if (!(ep[0] == '&' && ep[1] == '&')) return a;
		ep += 2; RVal b = e_cmp(); need_const(a, "&&"); need_const(b, "&&"); a = rconst(a.c && b.c); }
}
static RVal e_or(void) {   /* rank 2: the whole expression */
	RVal a = e_land();
	for (;;) { ews(); if (!(ep[0] == '|' && ep[1] == '|')) return a;
		ep += 2; RVal b = e_land(); need_const(a, "||"); need_const(b, "||"); a = rconst(a.c || b.c); }
}

/* Fold a pending `sym - msym` if both are now defined in one section. */
static RVal fold_msym(RVal r) {
	if (r.msym >= 0 && r.sym >= 0 && syms[r.sym].defined && syms[r.msym].defined && syms[r.sym].sec == syms[r.msym].sec) {
		r.c += (long)syms[r.sym].value - (long)syms[r.msym].value; r.sym = -1; r.msym = -1; }
	return r;
}
/* An operand that needs every label first (`.word (end - start) >> 5`): its text, re-evaluated at the end with `.`
 * where it appeared. size 4 may become a relocation (c + sym); 1/2/8 must come out constant. */
static struct { int sec; u32 off; int size; char *text; u32 seq; } *dtext; static int ndtext, dtextcap;
static int soft_eval(const char *text, RVal *out) {   /* 1: evaluated now; 0: deferred (a forward reference inside an operator) */
	const char *save = ep; ep = text; expr_soft = 1; expr_failed = 0;
	RVal r = e_or(); expr_soft = 0;
	int ok = !expr_failed; const char *end = ep; ep = save;
	if (ok) { *out = r; ep = end; }
	return ok;
}
static const char *operand_end(const char *p) {   /* the end of one comma-separated operand (paren- and quote-aware) */
	int d = 0, q = 0;
	for (; *p; p++) { if (q) { if (*p == '"') q = 0; continue; } if (*p == '"') q = 1; else if (*p == '(') d++; else if (*p == ')') d--; else if (*p == ',' && !d) break; }
	return p;
}
static void defer_text(const char *start, const char *end, int size) {
	dtext = grow(dtext, ndtext, &dtextcap, sizeof *dtext);
	dtext[ndtext].sec = cursec; dtext[ndtext].off = (u32)secs[cursec].len; dtext[ndtext].size = size; dtext[ndtext].text = strndup(start, (size_t)(end - start)); dtext[ndtext].seq = parse_seq; ndtext++;
	for (int k = 0; k < size; k++) { u8 z = 0; emit(&z, 1); }
}
/* 1- and 2-byte data whose value depends on later symbols: resolved after parsing (or ABS8/ABS16-relocated). */
static struct { int sec; u32 off; int size; RVal r; u32 seq; } *dsmall; static int ndsmall, dsmallcap;
/* Data words whose value depends on symbols not yet defined (`.word 1f - 1b`): resolved after parsing. */
static struct { int sec; u32 off; RVal r; u32 seq; } *dword; static int ndword, dwordcap;
static struct { int sym; RVal r; } *dsize; static int ndsize, dsizecap;
static void resolve_deferred(void) {
	for (int i = 0; i < ndsmall; i++) {
		RVal r = fold_msym(dsmall[i].r); int sz = dsmall[i].size; u8 *d = secs[dsmall[i].sec].data + dsmall[i].off;
		if (r.sym >= 0 && r.msym < 0 && syms[r.sym].defined && syms[r.sym].sec == SEC_ABS) { r.c += (long)(int32_t)syms[r.sym].value; r.sym = -1; }
		if (r.msym >= 0) die("a %d-byte field: a symbol difference that doesn't resolve in one section", sz);
		long lim = 1L << (8 * sz); if (r.c >= lim || r.c < -(lim / 2)) die("a %d-byte field: value %ld doesn't fit", sz, r.c);
		for (int k = 0; k < sz; k++) d[k] = (u8)((unsigned long)r.c >> (8 * k));
		if (r.sym >= 0) { reloc_seq = dsmall[i].seq; add_reloc(dsmall[i].sec, dsmall[i].off, r.sym, sz == 2 ? 5u : 8u); reloc_seq = -1; }   /* R_ARM_ABS16 / R_ARM_ABS8 */
	}
	for (int i = 0; i < ndword; i++) {
		RVal r = fold_msym(dword[i].r); int sec = dword[i].sec; u32 off = dword[i].off;
		if (r.sym < 0 && r.msym < 0) { patch32(sec, off, (u32)r.c); continue; }
		if (r.msym >= 0 && syms[r.msym].defined && syms[r.msym].sec == sec && r.sym >= 0 && !r.dot) {
			/* S - M with M in this word's section: = S - P + (P - M) -> R_ARM_REL32, addend c + (P - M) */
			u32 got = md_data_reloc_for(syms[r.sym].name, 0);   /* _GLOBAL_OFFSET_TABLE_ - L: R_ARM_BASE_PREL (GAS), same addend */
			patch32(sec, off, (u32)(r.c + (long)off - (long)syms[r.msym].value)); reloc_seq = dword[i].seq; add_reloc(sec, off, r.sym, got ? got : md_r_rel32); reloc_seq = -1; continue; }
		die("data expression: symbol difference across sections can't be represented");
	}
	for (int i = 0; i < ndsize; i++) {
		RVal r = fold_msym(dsize[i].r);
		if (r.sym >= 0 || r.msym >= 0 || r.dot) die(".size of '%s': not a constant", syms[dsize[i].sym].name);
		syms[dsize[i].sym].size = (u32)r.c;
	}
}
XVal expr_x(const char *s) {
	const char *save = ep; ep = s; RVal r = e_or(); ews();
	if (*ep) die("expression: junk '%s' in '%s'", ep, s);
	ep = save; XVal x = { r.c, r.sym, r.dot, r.msym, r.rtype }; return x;
}
void expr_fold(XVal *v) {
	RVal r = { v->c, v->sym, v->dot, v->msym, v->rtype }; r = fold_msym(r);
	v->c = r.c; v->sym = r.sym; v->msym = r.msym;
}
/* A constant expression (instruction immediates, .equ): `.` is the current location, a same-section
 * `sym - .` folds, absolute symbols are constants. Anything relocatable or trailing junk is an error (was:
 * strtol, which silently turned `#(. - bar - 8)` into 0). */
long eval_const_expr(const char *s) {
	const char *save = ep; ep = s; RVal r = e_or(); ews();
	if (*ep) die("expression: junk '%s' in '%s'", ep, s);
	ep = save;
	r = fold_msym(r);
	if (r.msym >= 0) die("expression '%s' is not a constant (symbols not yet defined, or in different sections)", s);
	if (r.sym >= 0 && r.dot == -1 && syms[r.sym].defined && syms[r.sym].sec == cursec) return r.c + (long)syms[r.sym].value - (long)secs[cursec].len;
	if (r.sym < 0 && r.dot == 1) return r.c + (long)secs[cursec].len;
	if (r.sym >= 0 || r.dot) die("expression '%s' is not a constant", s);
	return r.c;
}
int const_expr_soft(const char *s, long *v) {
	const char *save = ep; RVal r;
	if (!soft_eval(s, &r)) { ep = save; return 0; }
	ep = save; r = fold_msym(r);
	if (r.msym >= 0 || (r.sym >= 0 && !syms[r.sym].defined)) return 0;
	*v = eval_const_expr(s); return 1;
}
long const_expr_at(const char *s, int sec, u32 at) {
	int save = cursec; cursec = sec; dot_at = at;
	const char *sv = ep; ep = s; RVal r = e_or(); ews();
	if (*ep) die("expression: junk '%s' in '%s'", ep, s);
	ep = sv; r = fold_msym(r);
	if (r.dot == 1 && r.sym < 0 && r.msym < 0) { r.c += at; r.dot = 0; }
	if (r.dot == -1 && r.sym >= 0 && syms[r.sym].defined && syms[r.sym].sec == sec) { r.c += (long)syms[r.sym].value - at; r.sym = -1; r.dot = 0; }
	if (r.sym >= 0 && r.msym < 0 && !r.dot && syms[r.sym].defined && syms[r.sym].sec == SEC_ABS) { r.c += (long)(int32_t)syms[r.sym].value; r.sym = -1; }
	dot_at = -1; cursec = save;
	if (r.sym >= 0 || r.msym >= 0 || r.dot) die("'%s': not a constant after all (undefined, or symbols in different sections)", s);
	return r.c;
}
void eval_reloc_expr(const char *s, long *c, int *sym, int *dot) {
	const char *save = ep; ep = s; RVal r = e_or(); ews();
	if (*ep) die("expression: junk '%s' in '%s'", ep, s);
	ep = save; r = fold_msym(r);
	if (r.msym >= 0) die("expression '%s': symbol difference not resolvable here", s);
	*c = r.c; *sym = r.sym; *dot = r.dot;
}
/* Emit one 32-bit data word for a relocatable value placed at the current location. */
static void emit_word_rval(RVal r) {
	u32 off = secs[cursec].len;
	r = fold_msym(r);
	if (r.msym >= 0) {   /* defer until all labels are known */
		dword = grow(dword, ndword, &dwordcap, sizeof *dword);
		dword[ndword].sec = cursec; dword[ndword].off = off; dword[ndword].r = r; dword[ndword].seq = parse_seq; ndword++; emit32(0); return; }
	if (r.dot == 1 && r.sym < 0) {   /* `.word .` / `. + k`: this location, as the section symbol + offset (GAS) */
		emit32((u32)(r.c + (long)off)); add_reloc(cursec, off, section_symbol(cursec), r.rtype ? r.rtype : md_r_abs32); return; }
	if (r.dot != 0 && r.dot != -1) die("data expr: '.' may only appear as '- .' or alone (with a constant)");
	if (r.sym >= 0 && syms[r.sym].defined && syms[r.sym].sec == cursec && r.dot == -1) {   /* X - . in the same section: a constant */
		emit32((u32)(r.c + (long)syms[r.sym].value - (long)off)); return;
	}
	if (r.sym < 0) { if (r.dot) die("data expr: '- .' with no symbol"); emit32((u32)r.c); return; }
	emit32((u32)r.c);   /* REL-style: the addend lives in place */
	if (r.rtype) { if (r.dot) die("data expr: '- .' with an explicit relocation operator"); add_reloc(cursec, off, r.sym, r.rtype); return; }
	/* _GLOBAL_OFFSET_TABLE_ in data is always R_ARM_BASE_PREL (B(S) + A - P) in GAS — the `- .` is implied by the
	 * relocation, so `.word GOT - (. + 8)` has addend -8 (was: REL32 whenever `.` appeared) */
	u32 dflt = md_data_reloc_for(syms[r.sym].name, 0);
	if (dflt) { add_reloc(cursec, off, r.sym, dflt); return; }
	add_reloc(cursec, off, r.sym, r.dot ? md_r_rel32 : md_r_abs32);
}
/* `.byte/.hword/.quad` operand lists: constant expressions (was strtol per space-split token: `.byte X - Y`
 * silently became X, junk ignored). Range-checked against the width (signed or unsigned). */
static void data_consts(int size) {
	const char *s = cur_stmt; while (*s == ' ' || *s == '\t') s++;
	while (*s && *s != ' ' && *s != '\t') s++;
	ep = s; ews(); if (!*ep) die("%s: missing operand", toks[0]);
	for (;;) {
		RVal r; const char *o0 = ep;
		if (!soft_eval(ep, &r)) { if (size == 8) die("%s: a forward reference in an 8-byte field", toks[0]); const char *oe = operand_end(ep); defer_text(o0, oe, size); ep = oe; ews(); if (*ep == ',') { ep++; continue; } if (*ep) die("%s: junk '%s'", toks[0], ep); break; }
		r = fold_msym(r);
		if (r.dot == -1 && r.sym >= 0 && r.msym < 0 && syms[r.sym].defined && syms[r.sym].sec == cursec) { r.c += (long)syms[r.sym].value - (long)secs[cursec].len; r.sym = -1; r.dot = 0; }
		if (r.dot) die("%s: '.' in a %d-byte field", toks[0], size);
		if (r.sym >= 0 && syms[r.sym].defined && syms[r.sym].sec == SEC_ABS && r.msym < 0) { r.c += (long)(int32_t)syms[r.sym].value; r.sym = -1; }
		if (r.sym >= 0 || r.msym >= 0) {   /* a forward reference (`.short section_count`, `.short b - a`): resolved at the end */
			if (size == 8) die("%s: a symbol in an 8-byte field (no 64-bit relocation)", toks[0]);
			dsmall = grow(dsmall, ndsmall, &dsmallcap, sizeof *dsmall);
			dsmall[ndsmall].sec = cursec; dsmall[ndsmall].off = (u32)secs[cursec].len; dsmall[ndsmall].size = size; dsmall[ndsmall].r = r; dsmall[ndsmall].seq = parse_seq; ndsmall++;
			for (int k = 0; k < size; k++) { u8 z = 0; emit(&z, 1); }
			ews(); if (*ep == ',') { ep++; continue; } if (*ep) die("%s: junk '%s'", toks[0], ep); break;
		}
		if (size < 8) { long lim = 1L << (8 * size); if (r.c >= lim || r.c < -(lim / 2)) die("%s: value %ld doesn't fit in %d byte(s)", toks[0], r.c, size); }
		for (int k = 0; k < size; k++) { u8 b = (u8)((unsigned long)r.c >> (8 * k)); emit(&b, 1); }
		ews(); if (*ep == ',') { ep++; continue; } if (*ep) die("%s: junk '%s'", toks[0], ep); break;
	}
}
/* Split "a, b, c" at top-level commas (respecting parens) into up to three trimmed strings. */
static void split_args(const char *s, char *a1, char *a2, char *a3) {
	char *out[3] = { a1, a2, a3 }; int n = 0, depth = 0; size_t k = 0;
	while (*s == ' ' || *s == '\t') s++;
	for (; *s && n < 3; s++) {
		if (*s == '(') depth++; else if (*s == ')') depth--;
		if (*s == ',' && depth == 0) { out[n][k] = 0; n++; k = 0; while (s[1] == ' ' || s[1] == '\t') s++; continue; }
		if (k < 255) out[n][k++] = *s; else die("directive operand too long");
	}
	if (*s) die("too many operands");
	if (n < 3) { out[n][k] = 0; }
	for (int i = 0; i < 3; i++) { size_t l = strlen(out[i]); while (l && (out[i][l - 1] == ' ' || out[i][l - 1] == '\t')) out[i][--l] = 0; }
}
/* `.word/.long` operand list from the raw statement text (after the directive name). */
static void data_words(void) {
	const char *s = cur_stmt; while (*s == ' ' || *s == '\t') s++;
	while (*s && *s != ' ' && *s != '\t') s++;   /* skip the directive name */
	ep = s; ews();
	if (!*ep) die("%s: missing operand", toks[0]);
	for (;;) { RVal r; const char *o0 = ep;
		if (!soft_eval(ep, &r)) { const char *oe = operand_end(ep); defer_text(o0, oe, 4); ep = oe; ews(); if (*ep == ',') { ep++; continue; } if (*ep) die("data expr: junk '%s' in '%s'", ep, cur_stmt); break; }
		ews();
		if (*ep == '(') {   /* `expr(OP)`: GAS lets the operator follow the whole expression (`.word foo + 0x1234(TARGET2)`) */
			const char *q = ep + 1; char op[24]; int k = 0;
			while ((isalnum((unsigned char)*q) || *q == '_') && k < 23) op[k++] = *q++;
			op[k] = 0;
			if (*q != ')' || !k || r.sym < 0) die("data expr: bad relocation operator in '%s'", cur_stmt);
			if (r.rtype) die("data expr: two relocation operators in '%s'", cur_stmt);
			u32 rt; if (!md_reloc_operator(op, &rt)) die("unknown relocation operator '(%s)' in '%s'", op, cur_stmt);
			r.rtype = rt; ep = q + 1; ews();
		}
		emit_word_rval(r); if (*ep == ',') { ep++; continue; } if (*ep) die("data expr: junk '%s' in '%s'", ep, cur_stmt); break; }
}

/* ---- floating-point data: .float/.single (binary32), .double (binary64), .float16 (IEEE or ARM alternative
 * half), .bfloat16. Decimal -> double via strtod (correctly rounded), then ONE round-to-nearest-even into the
 * target; checked against GNU as. */
static int fp16_alt;   /* .float16_format alternative: no Inf/NaN, exponent 31 is a normal exponent */
static double fp_parse(const char *t) {
	const char *q = t; int neg = 0;
	if ((q[0] == '0') && (q[1] == 'f' || q[1] == 'F' || q[1] == 'd' || q[1] == 'D' || q[1] == 'e' || q[1] == 'E' || q[1] == 'r' || q[1] == 'R') && q[2] && !isdigit((unsigned char)q[1])) q += 2;   /* 0f1.5 */
	if (*q == '+' || *q == '-') { neg = *q == '-'; q++; }
	double v;
	if (!strcasecmp(q, "inf") || !strcasecmp(q, "infinity")) v = __builtin_inf();
	else if (!strcasecmp(q, "nan")) v = __builtin_nan("");
	else { char *e; v = strtod(q, &e); if (e == q || *e) die("bad floating-point value '%s'", t); }
	return neg ? -v : v;
}
/* Round double -> (sign, exp bits eb, mantissa bits mb) IEEE-style, RNE; alt = no inf/nan (ARM alt half). */
static unsigned long long fp_pack(double v, int eb, int mb, int alt) {
	unsigned long long bits; memcpy(&bits, &v, 8);
	unsigned long long sign = bits >> 63; int e = (int)((bits >> 52) & 0x7ff); unsigned long long m = bits & ((1ULL << 52) - 1);
	int bias = (1 << (eb - 1)) - 1, emax = (1 << eb) - 1;
	unsigned long long sgn = sign << (eb + mb);
	if (e == 0x7ff) {   /* inf / nan */
		if (alt) die(".float16: Inf/NaN not representable in the alternative format");
		if (m) return sgn | ((unsigned long long)emax << mb) | ((1ULL << mb) - 1);   /* NaN: GAS sets every mantissa bit */
		return sgn | ((unsigned long long)emax << mb);
	}
	if (e == 0 && m == 0) return sgn;
	/* value = 1.m * 2^(e-1023) (normal double; double subnormals are far below any target's range -> 0/tiny) */
	int E = e ? e - 1023 : -1022; unsigned long long M = e ? (m | (1ULL << 52)) : m;   /* M: 53-bit significand */
	int te = E + bias;   /* target biased exponent if normal */
	int shift = 52 - mb;   /* drop this many bits for a normal result */
	if (te <= 0) shift += 1 - te, te = 0;   /* subnormal: denormalize */
	if (shift > 63) return sgn;   /* underflows to zero */
	unsigned long long q = M >> shift, rem = M & ((1ULL << shift) - 1), half = 1ULL << (shift - 1);
	if (rem > half || (rem == half && (q & 1))) q++;   /* round to nearest, ties to even */
	/* q holds (hidden bit + mantissa) for normals, mantissa for subnormals; renormalize on carry */
	unsigned long long mant; int ex;
	if (te == 0) { if (q >> mb) { ex = 1; mant = q & ((1ULL << mb) - 1); } else { ex = 0; mant = q; } }
	else { if (q >> (mb + 1)) { q >>= 1; te++; } ex = te; mant = q & ((1ULL << mb) - 1); }
	if (ex >= (alt ? emax + 1 : emax)) {   /* overflow */
		if (alt) die(".float16: value %g out of range for the alternative format", v);
		return sgn | ((unsigned long long)emax << mb);   /* +-Inf */
	}
	return sgn | ((unsigned long long)ex << mb) | mant;
}
static void fp_directive(const char *d) {
	int sz = !strcmp(d, ".double") ? 8 : (!strcmp(d, ".float16") || !strcmp(d, ".bfloat16")) ? 2 : 4;
	if (ntok < 2) die("%s: missing operand", d);
	map_data();
	for (int i = 1; i < ntok; i++) {
		double v = fp_parse(toks[i]); unsigned long long w;
		if (v != v) { int neg = toks[i][0] == '-'; w = sz == 8 ? 0x7fffffffffffffffULL : sz == 4 ? 0x7fffffffULL : 0x7fffULL;   /* NaN: all mantissa bits (GAS) */
			if (neg) w |= 1ULL << (8 * sz - 1);
			if (sz == 2 && fp16_alt && strcmp(d, ".bfloat16")) die(".float16: NaN not representable in the alternative format"); }
		else if (sz == 8) memcpy(&w, &v, 8);
		else if (sz == 4) { float f = (float)v; u32 b; memcpy(&b, &f, 4); w = b; }   /* (float) of a double: RNE */
		else if (!strcmp(d, ".bfloat16")) w = fp_pack(v, 8, 7, 0);
		else w = fp_pack(v, 5, 10, fp16_alt);
		for (int k = 0; k < sz; k++) { u8 b = (u8)(w >> (8 * k)); emit(&b, 1); }
	}
}
static void do_directive(void) {
	const char *d = toks[0];
	int was = cursec;
	if (!strcmp(d, ".section")) {
		select_section(); prevsec = was;   /* default flags/type by well-known name; an explicit "flags" string overrides */
	} else if (!strcmp(d, ".pushsection")) {   /* save (current, previous), then switch */
		if (secsp >= 32) die("too many nested .pushsection (>32)");
		secstack[secsp][0] = cursec; secstack[secsp][1] = prevsec; secsp++;
		select_section(); prevsec = was;
	} else if (!strcmp(d, ".popsection")) {
		if (secsp <= 0) die(".popsection without .pushsection");
		secsp--; cursec = secstack[secsp][0]; prevsec = secstack[secsp][1];
	} else if (!strcmp(d, ".previous")) {       /* swap with the section before the most recent switch */
		if (prevsec < 0) die(".previous with no previous section");
		cursec = prevsec; prevsec = was;
	} else if (!strcmp(d, ".text")) { int t = sec_get(".text", SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR); if (ntok > 1) cursec = subsection_of(t, eval_const_expr(toks[1])); prevsec = was;
	} else if (!strcmp(d, ".data")) { int t = sec_get(".data", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE); if (ntok > 1) cursec = subsection_of(t, eval_const_expr(toks[1])); prevsec = was;
	} else if (!strcmp(d, ".subsection")) {   /* .subsection N of the current section */
		if (ntok < 2 || cursec < 0) die(".subsection: expected a number (in a section)");
		cursec = subsection_of(cursec, eval_const_expr(toks[1])); prevsec = was;
	} else if (!strcmp(d, ".global") || !strcmp(d, ".globl")) {   /* a list: `.globl vdso_start, vdso_end` */
		if (ntok < 2) die("%s: expected a symbol", d);
		for (int k = 1; k < ntok; k++) { int i = sym_intern(toks[k]); syms[i].global = 1; }   /* (intern FIRST: it may grow syms) */
	} else if (!strcmp(d, ".hidden") || !strcmp(d, ".internal") || !strcmp(d, ".protected")) {   /* symbol visibility (st_other) */
		int v = d[1] == 'h' ? STV_HIDDEN : d[1] == 'i' ? STV_INTERNAL : STV_PROTECTED;
		if (ntok < 2) die("%s: expected a symbol", d);
		for (int k = 1; k < ntok; k++) { int i = sym_intern(toks[k]); syms[i].vis = v; }
	} else if (!strcmp(d, ".weak")) {   /* STB_WEAK binding (kernel COND_SYSCALL); a list */
		if (ntok < 2) die(".weak: expected a symbol");
		for (int k = 1; k < ntok; k++) { int i = sym_intern(toks[k]); syms[i].weak = 1; }
	} else if (!strcmp(d, ".type")) {   /* .type sym, %function | @object | "notype" | STT_FUNC ... (the comma optional, GAS) */
		if (ntok < 3) die(".type: expected 'sym, type'");
		int i = sym_intern(toks[1]); const char *t = toks[2]; char tb[64];
		if (*t == '%' || *t == '@' || *t == '#') t++;
		if (*t == '"') { snprintf(tb, sizeof tb, "%s", t + 1); char *q = strchr(tb, '"'); if (q) *q = 0; t = tb; }
		static const struct { const char *n; int type; } tt[] = { {"function", STT_FUNC}, {"STT_FUNC", STT_FUNC}, {"object", STT_OBJECT},
			{"STT_OBJECT", STT_OBJECT}, {"tls_object", STT_TLS}, {"STT_TLS", STT_TLS}, {"notype", STT_NOTYPE}, {"STT_NOTYPE", STT_NOTYPE} };
		int found = -1; for (unsigned k = 0; k < sizeof tt / sizeof *tt; k++) if (!strcmp(t, tt[k].n)) found = tt[k].type;
		if (found < 0) die(".type %s: unsupported type '%s'", toks[1], toks[2]);
		syms[i].type = found;
	} else if (!strcmp(d, ".size")) {
		/* .size <sym>, <expr> — any expression (was: only `. - label`; anything else was silently ignored) */
		int i = sym_intern(toks[1]);
		const char *q = strchr(cur_stmt, ','); if (!q) die(".size: expected 'sym, expr'");
		const char *save = ep; ep = q + 1; RVal r = e_or(); ews(); if (*ep) die(".size: junk '%s'", ep); ep = save;
		syms[i].sized = 1;
		if (r.dot == 1 && r.msym >= 0 && r.sym < 0 && syms[r.msym].defined && syms[r.msym].sec == cursec) {   /* . - sym */
			r.c += (long)secs[cursec].len - (long)syms[r.msym].value; r.dot = 0; r.msym = -1; }
		r = fold_msym(r);
		if (r.sym < 0 && r.msym < 0 && !r.dot) syms[i].size = (u32)r.c;
		else { dsize = grow(dsize, ndsize, &dsizecap, sizeof *dsize); dsize[ndsize].sym = i; dsize[ndsize].r = r; ndsize++; }
	} else if (!strcmp(d, ".align") || !strncmp(d, ".p2align", 8) || !strncmp(d, ".balign", 7)) {
		/* .align/.p2align N (a power of two) or .balign N (bytes) [, FILL [, MAX]] — GAS s_align: FILL a byte, or for the
		 * w/l forms a 2/4-byte little-endian pattern repeated (the padding must be a multiple of it); without one, code
		 * gets NOPs and data zeros. Padding over MAX: none. ARM's .align: no operand or 0 is 2 (TC_ALIGN_ZERO_IS_DEFAULT). */
		int bytes_p = d[1] == 'b', base = bytes_p ? 7 : d[1] == 'p' ? 8 : 6;
		if (d[base] && (d[base + 1] || !strchr("wl", d[base]))) die("unknown directive '%s'", d);
		u32 fill_len = d[base] == 'w' ? 2 : d[base] == 'l' ? 4 : 1;
		if (cursec < 0) return;
		char a1[256] = "", a2[256] = "", a3[256] = "";
		{ const char *q = cur_stmt; while (*q == ' ' || *q == '\t') q++; while (*q && *q != ' ' && *q != '\t') q++; split_args(q, a1, a2, a3); }
		long a = a1[0] ? eval_const_expr(a1) : 0;
		if (base == 6 && a == 0) a = 2;
		if (bytes_p ? (a < 0 || a > 65536 || (a & (a - 1))) : (a < 0 || a > 16)) die("%s: bad alignment '%s'", d, a1);
		u32 bytes = bytes_p ? (a ? (u32)a : 1) : (1u << a);
		int fill_p = a2[0] != 0; long fill = fill_p ? eval_const_expr(a2) : 0, max = a3[0] ? eval_const_expr(a3) : 0;
		if (max < 0) die("%s: bad maximum '%s'", d, a3);
		if ((fill_p || max) && secs[cursec].base >= 0) die("%s with a fill or a maximum in a subsection: unsupported", d);
		if (secs[cursec].base >= 0 && secs[cursec].len > 0 && bytes > secs[cursec].pad_align) secs[cursec].pad_align = bytes;
		if (secs[cursec].base >= 0 && secs[cursec].len == 0 && bytes > secs[cursec].first_align) secs[cursec].first_align = bytes, secs[cursec].first_align_mode = frag_mode(cursec);   /* padded where
		                                                           * the subsection lands (merge_subsections) */
		sec_align_at_least(bytes);
		if (bytes == 1) return;   /* (GAS: no frag) */
		u32 pad = (bytes - (u32)(secs[cursec].len % bytes)) % bytes;
		if (max && pad > (u32)max) pad = 0;
		if (secs[cursec].type == SHT_NOBITS) { map_frag_data(); secs[cursec].len += pad; return; }   /* (a fill ignored) */
		if (fill_p) {   /* a data alignment frag even in code (arm_init_frag: rs_align is $d) */
			map_frag_data();
			if (pad % fill_len) die("%s: alignment padding (%u bytes) not a multiple of %u", d, pad, fill_len);
			for (u32 k = 0; k < pad; k++) { u8 b = (u8)((unsigned long)fill >> (8 * (k % fill_len))); emit(&b, 1); }
			new_frag();
			return;
		}
		int nb = nsym, fm = frag_mode(cursec);
		if (secs[cursec].flags & SHF_EXECINSTR) map_to(fm ? MAP_THUMB : MAP_ARM, 0);   /* rs_align_code: the frag's code state at
		                                                  * parse time (may add $d@0 + $a/$t), not an instruction's alignment */
		else map_frag_data();
		if (secs[cursec].base >= 0 && secs[cursec].len == 0 && secs[cursec].lead < 0)
			for (int k = nb; k < nsym; k++) if (is_mapsym(k) && syms[k].sec == cursec) secs[cursec].lead = k;
		if (secs[cursec].flags & SHF_EXECINSTR) { if (pad) code_pad(bytes, fm); }
		else for (u32 k = 0; k < pad; k++) { u8 zb = 0; emit(&zb, 1); }
		new_frag();
	} else if ((!strcmp(d, ".inst") && thumb_mode) || !strcmp(d, ".inst.n") || !strcmp(d, ".inst.w")) {
		/* Thumb .inst (GAS emit_insn): each constant is an instruction — .n a halfword, .w a word written as its two
		 * halfwords high first; unsuffixed, the size follows the value (< 0xe800 narrow, >= 0xe8000000 wide) */
		if (!thumb_mode) die("%s: width suffixes are invalid in ARM mode", d);
		const char *q = cur_stmt; while (*q == ' ' || *q == '\t') q++; while (*q && *q != ' ' && *q != '\t') q++;
		for (;;) {
			while (*q == ' ' || *q == '\t') q++;
			if (!*q) die("%s: missing expression", d);
			const char *e = q; int depth = 0; while (*e && !(depth == 0 && *e == ',')) { if (*e == '(') depth++; else if (*e == ')') depth--; e++; }
			char *x = strndup(q, (size_t)(e - q)); u32 v = (u32)eval_const_expr(x); free(x);
			int size = d[6] == 'n' ? 2 : d[6] == 'w' ? 4 : v < 0xe800u ? 2 : v >= 0xe8000000u ? 4 : 0;
			if (!size) die(".inst: cannot determine Thumb instruction size of %#x; use .inst.n/.inst.w", v);
			if (size == 2 && v > 0xffff) die(".inst.n operand too big (%#x); use .inst.w", v);
			map_insn();
			if (size == 4) { emit16(v >> 16); emit16(v & 0xffff); } else emit16(v);
			if (!*e) break;
			q = e + 1;
		}
	} else if (!strcmp(d, ".word") || !strcmp(d, ".4byte") || !strcmp(d, ".long") || !strcmp(d, ".inst")) {
		/* .word <number> emits the value; .word <symbol>[+addend] emits the addend in place + an
		 * absolute (R_ARM_ABS32) relocation the linker fills with the symbol's address. */
		if (!strcmp(d, ".inst")) map_insn(); else map_data();
		data_words();   /* expressions, lists, local labels, `X - .`, `sym(OP)` relocation operators */
	} else if (!strcmp(d, ".float") || !strcmp(d, ".single") || !strcmp(d, ".double") || !strcmp(d, ".float16") || !strcmp(d, ".bfloat16")) {
		fp_directive(d);
	} else if (!strcmp(d, ".float16_format")) {
		if (ntok != 2) die(".float16_format: expected ieee|alternative");
		static int fp16_set = -1; int want;
		if (!strcmp(toks[1], "ieee")) want = 0; else if (!strcmp(toks[1], "alternative")) want = 1; else die(".float16_format: expected ieee|alternative, got '%s'", toks[1]);
		if (fp16_set >= 0 && fp16_set != want) die(".float16_format: the format can be set only once (GAS ignores a change)");   /* we fail loud instead */
		fp16_set = want; fp16_alt = want;
	} else if (!strcmp(d, ".byte")) {
		map_data(); data_consts(1);
	} else if (!strcmp(d, ".hword") || !strcmp(d, ".2byte") || !strcmp(d, ".short")) {
		map_data(); data_consts(2);
	} else if (!strcmp(d, ".quad") || !strcmp(d, ".8byte")) {
		map_data(); data_consts(8);
	} else if (!strcmp(d, ".bss")) {
		sec_get(".bss", SHT_NOBITS, SHF_ALLOC | SHF_WRITE); prevsec = was;
	} else if (!strcmp(d, ".space") || !strcmp(d, ".skip") || !strcmp(d, ".zero") || !strcmp(d, ".fill")) {
		/* .space n[, fill]  |  .fill repeat[, size[, value]]  — a fill frag: $d at its start (even from UNDEF) */
		const char *q = cur_stmt; while (*q == ' ' || *q == '\t') q++; while (*q && *q != ' ' && *q != '\t') q++;
		char a1[256] = "", a2[256] = "", a3[256] = ""; split_args(q, a1, a2, a3);
		long n = eval_const_expr(a1), size = 1, val = 0;
		if (!strcmp(d, ".fill")) { if (a2[0]) size = eval_const_expr(a2); if (a3[0]) val = eval_const_expr(a3); if (size < 0 || size > 8) die(".fill: bad size %ld", size); }
		else if (a2[0]) val = eval_const_expr(a2);
		if (n < 0) die("%s: negative size %ld", d, n);
		if (n * size == 0) return;   /* GAS: "repeat count is zero, ignored" — no frag, so no mapping symbol either */
		if (secs[cursec].type == SHT_NOBITS) { if (val) die("%s: non-zero fill in a NOBITS section", d); map_frag_data(); secs[cursec].len += (size_t)(n * size); }
		else { map_frag_data(); for (long i = 0; i < n; i++) for (long k = 0; k < size; k++) { u8 b = (u8)(k < 4 ? (val >> (8 * k)) : 0); emit(&b, 1); } }
	} else if (!strcmp(d, ".ascii") || !strcmp(d, ".asciz") || !strcmp(d, ".string")) {
		map_data();
		for (int i = 1; i < ntok; i++) emit_string(toks[i], strcmp(d, ".ascii") != 0);   /* each operand (`.ascii "" "\0"`); .ascii: no NUL, .asciz/.string: NUL per string */
	} else if (!strcmp(d, ".set") || !strcmp(d, ".equ")) {
		/* two forms: `.set name, . [+ N]` (anchor at the current spot) and `.set name, othersym`
		 * (a symbol ALIAS — kernel COND_SYSCALL weak-aliases an unimplemented syscall to sys_ni_syscall). */
		int i = sym_intern(toks[1]);
		if (ntok == 3 && !strcmp(toks[2], ".")) {   /* `.set name, .` (with an expression — `. - start`, `. + 4` — below) */
			syms[i].sec = cursec; syms[i].value = secs[cursec].len; syms[i].defined = 1;
		/* An EXPRESSION is tested before the shape of toks[2], because soft_eval is the only path that can
		 * DEFER a forward reference. Deciding on the first character instead sent `(end - start) >> 2` —
		 * which opens with '(' — to the constant-only evaluator, where a label defined later is fatal,
		 * while the identical expression without parentheses deferred correctly. GAS accepts both. */
		} else if (ntok >= 4 || (ntok == 3 && strpbrk(toks[2], "+-*/()<>&|^~!"))) {   /* `.set x, . - start` / `sym + 4`: an expression */
			const char *q = strchr(cur_stmt, ','); if (!q) die(".set: expected 'name, value'");
			RVal sr; XVal v = { 0, -1, 0, -1, 0 }; int now = soft_eval(q + 1, &sr);
			if (now) { v.c = sr.c; v.sym = sr.sym; v.dot = sr.dot; v.msym = sr.msym; v.rtype = sr.rtype; expr_fold(&v); ews(); if (*ep) die(".set: junk '%s'", ep); }
			else v.dot = 2;                                  /* (not evaluable now: straight to the deferred list) */
			if (v.dot == -1 && v.sym >= 0 && v.msym < 0 && syms[v.sym].defined && syms[v.sym].sec == cursec) { v.c += (long)syms[v.sym].value - (long)secs[cursec].len; v.sym = -1; v.dot = 0; }
			if (v.dot == 1 && v.sym < 0 && v.msym < 0) { syms[i].sec = cursec; syms[i].value = (u32)(secs[cursec].len + v.c); syms[i].defined = 1; }
			else if (v.dot == 1 && v.msym >= 0 && v.sym < 0 && syms[v.msym].defined && syms[v.msym].sec == cursec) {   /* `. - start` */
				syms[i].sec = SEC_ABS; syms[i].value = (u32)((long)secs[cursec].len + v.c - (long)syms[v.msym].value); syms[i].defined = 1; }
			else if (!v.dot && v.sym < 0 && v.msym < 0) { syms[i].sec = SEC_ABS; syms[i].value = (u32)v.c; syms[i].defined = 1; }
			else if (!v.dot && v.sym >= 0 && v.msym < 0 && syms[v.sym].defined) {   /* `sym + k`: sym's place plus k */
				syms[i].sec = syms[v.sym].sec; syms[i].value = (u32)((long)syms[v.sym].value + v.c); syms[i].defined = 1; }
			else {   /* forward references: resolved at the end, with `.` taken here (GAS makes `.` a symbol at this spot) */
				dset = grow(dset, ndset, &dsetcap, sizeof *dset);
				dset[ndset].sym = i; dset[ndset].text = strdup(q + 1); dset[ndset].sec = cursec; dset[ndset].at = (u32)secs[cursec].len; ndset++;
			}
		} else if (ntok >= 3 && !(isalpha((unsigned char)toks[2][0]) || toks[2][0] == '_' || toks[2][0] == '.')) {   /* `.equ N, 16`: a plain absolute value */
			const char *q = strchr(cur_stmt, ','); if (!q) die(".set: expected 'name, value'");
			syms[i].sec = SEC_ABS; syms[i].value = (u32)eval_const_expr(q + 1); syms[i].defined = 1;
		} else if (ntok == 3 && sym_find(toks[2]) >= 0 && syms[sym_find(toks[2])].defined && syms[sym_find(toks[2])].sec == SEC_ABS) {
			syms[i].sec = SEC_ABS; syms[i].value = syms[sym_find(toks[2])].value; syms[i].defined = 1;   /* an absolute target: its value now */
		} else if (ntok >= 3) {   /* alias: copy the target's location/type — at the END (GAS: the target may come later) */
			alias_of = grow(alias_of, nalias, &aliascap, sizeof *alias_of);
			alias_of[nalias].sym = i; alias_of[nalias].target = strdup(toks[2]); nalias++;
		} else die(".set: expected 'name, . [+ N]' or 'name, target'");
	} else if (!strcmp(d, ".file")) {
		/* Two forms. `.file "name"` is an STT_FILE symbol (GAS moves the first to the table's front).
		 * `.file N "name"` is DWARF: it names entry N of the line table's file list — see dwline.c. */
		if (ntok == 3 && toks[1][0] >= '0' && toks[1][0] <= '9' && toks[2][0] == '"') {
			char *nm2 = decode_quoted(toks[2]);
			dwline_file((int)strtol(toks[1], NULL, 10), nm2);
			free(nm2);
			return;
		}
		if (ntok != 2 || toks[1][0] != '"') die(".file: expected \"name\" or N \"name\"");
		char *nm = decode_quoted(toks[1]);
		int i = sym_add((Sym){ .name = nm, .sec = SEC_ABS, .value = 0, .type = STT_FILE, .defined = 1 });
		if (file_front < 0) file_front = i;
	} else if (!strcmp(d, ".loc")) {
		/* .loc FILE LINE [COLUMN] [is_stmt N] [basic_block] ...: the code from here on comes from that place.
		 * Only FILE and LINE affect the table we build; the rest is accepted and ignored, as it only refines
		 * what a debugger shows (GAS accepts the same spellings). */
		if (ntok < 3) die(".loc: expected FILE LINE [COLUMN]");
		dwline_loc((int)strtol(toks[1], NULL, 10), (int)strtol(toks[2], NULL, 10),
		           ntok > 3 ? (int)strtol(toks[3], NULL, 10) : 0, cursec, (u32)secs[cursec].len);
	} else if (!strcmp(d, ".ident")) {   /* .ident "s": appended to .comment (merge strings, entsize 1), after one leading NUL */
		if (ntok != 2 || toks[1][0] != '"') die(".ident: expected \"string\"");
		int was2 = cursec, c = sec_find(".comment");
		if (c < 0) {   /* SHF_MERGE|SHF_STRINGS; its section symbol comes at the end (GAS subseg_new, not .section) */
			c = sec_get_nosym(".comment", SHT_PROGBITS, 0x30); secs[c].entsize = 1; u8 z = 0; emit(&z, 1); }
		cursec = c; char *str = decode_quoted(toks[1]); emit(str, strlen(str) + 1); free(str);
		cursec = was2;
	} else if (!strcmp(d, ".reloc")) {   /* .reloc offset, R_ARM_NAME[, symbol]: a relocation, placed at the end */
		const char *q = cur_stmt; while (*q == ' ' || *q == '\t') q++; while (*q && *q != ' ' && *q != '\t') q++;
		char a1[256] = "", a2[256] = "", a3[256] = ""; split_args(q, a1, a2, a3);
		char *nm = a2; while (*nm == ' ' || *nm == '\t') nm++; { char *e = nm + strlen(nm); while (e > nm && (e[-1] == ' ' || e[-1] == '\t')) *--e = 0; }
		u32 ty = md_reloc_by_name(nm);
		dreloc = grow(dreloc, ndreloc, &dreloccap, sizeof *dreloc);
		dreloc[ndreloc].off = expr_x(a1); dreloc[ndreloc].type = ty; dreloc[ndreloc].seq = parse_seq; dreloc[ndreloc].sec = mstate_sec(cursec);
		if (dreloc[ndreloc].off.dot == 1) { dreloc[ndreloc].off.dot = 0; dreloc[ndreloc].off.c += secs[cursec].len; dreloc[ndreloc].off.sym = dreloc[ndreloc].off.sym < 0 ? section_symbol(cursec) : (die(".reloc: `sym + .` offset"), -1); }
		XVal z = { 0, -1, 0, -1, 0 };
		char *a3t = a3; while (*a3t == ' ' || *a3t == '\t') a3t++; { char *e = a3t + strlen(a3t); while (e > a3t && (e[-1] == ' ' || e[-1] == '\t')) *--e = 0; }
		if (!strcmp(a3t, ".")) {   /* `.`: GAS names this spot with a fake label, "L0\001", kept in the symbol table */
			z.sym = sym_add((Sym){ .name = "L0\001", .sec = cursec, .value = (u32)secs[cursec].len, .type = STT_NOTYPE, .defined = 1 });
			dreloc[ndreloc].sym = z;
		} else dreloc[ndreloc].sym = a3t[0] ? expr_x(a3t) : z;
		ndreloc++;
	} else if (!strcmp(d, ".org")) {   /* .org expr[, fill]: pad forward to offset expr of this section */
		const char *q = cur_stmt; while (*q == ' ' || *q == '\t') q++; while (*q && *q != ' ' && *q != '\t') q++;
		char a1[256] = "", a2[256] = "", a3[256] = ""; split_args(q, a1, a2, a3);
		const char *save = ep; ep = a1; RVal r = e_or(); ews(); if (*ep) die(".org: junk '%s'", ep); ep = save; r = fold_msym(r);
		long target;
		if (r.sym >= 0 && !r.dot && r.msym < 0 && syms[r.sym].defined && syms[r.sym].sec == cursec) target = (long)syms[r.sym].value + r.c;
		else if (r.sym < 0 && !r.dot && r.msym < 0) target = r.c;
		else if (r.sym < 0 && r.dot == 1) target = (long)secs[cursec].len + r.c;
		else die(".org: the location must be a constant or in this section");
		long fill = a2[0] ? eval_const_expr(a2) : 0;
		if (target < (long)secs[cursec].len) die(".org: attempt to move .org backwards");
		if (secs[cursec].base >= 0) die(".org inside a .subsection is not supported");
		new_frag();
		if (target > (long)secs[cursec].len) {   /* GAS writes .org padding with no mapping symbol (in code or data) */
			if (secs[cursec].type == SHT_NOBITS) secs[cursec].len = (size_t)target;
			else while ((long)secs[cursec].len < target) { u8 b = (u8)fill; emit(&b, 1); }
		}
	} else if (!strcmp(d, ".incbin")) {   /* .incbin "file"[, skip[, count]]: the file's bytes (piggy.S, vdso.S, initramfs) */
		if (ntok < 2 || toks[1][0] != '"') die(".incbin: expected a \"file\"");
		const char *q = strchr(cur_stmt, '"'); q = strchr(q + 1, '"'); q = q ? q + 1 : "";
		while (*q == ' ' || *q == '\t') q++;
		if (*q == ',') q++;
		char a1[256] = "", a2[256] = "", a3[256] = ""; split_args(q, a1, a2, a3);
		long skip = a1[0] ? eval_const_expr(a1) : 0, count = a2[0] ? eval_const_expr(a2) : -1;
		size_t n; u8 *b = read_included(toks[1], &n, ".incbin");
		if (skip < 0 || (size_t)skip > n) die(".incbin: skip %ld beyond the file's %zu bytes", skip, n);
		if (count < 0 || (size_t)(skip + count) > n) { if (count >= 0) die(".incbin: skip+count beyond the file's %zu bytes", n); count = (long)(n - skip); }
		map_data(); emit(b + skip, (size_t)count); free(b);
	} else if (!strcmp(d, ".include")) {   /* .include "file": its lines, here */
		if (ntok < 2 || toks[1][0] != '"') die(".include: expected a \"file\"");
		size_t n; char *b = (char *)read_included(toks[1], &n, ".include");
		include_text(b); free(b);
	} else if (!md_directive(toks, ntok)) {
		die("unknown directive '%s'", d);
	}
}

/* Resolve `.set name, target` aliases once the whole input is read (chains resolve over repeated passes).
 * A target never defined in this file is an error (not silently an undefined alias). */
static void resolve_aliases(void) {
	for (int left = nalias, prev = -1; left && left != prev; ) {
		prev = left; left = 0;
		for (int k = 0; k < nalias; k++) {
			if (!alias_of[k].target) continue;
			int i = alias_of[k].sym, j = sym_find(alias_of[k].target);
			if (j < 0 || !syms[j].defined) { left++; continue; }
			syms[i].sec = syms[j].sec; syms[i].value = syms[j].value; syms[i].defined = 1;
			if (!syms[i].type) syms[i].type = syms[j].type;
			alias_done = grow(alias_done, nalias_done, &donecap, sizeof *alias_done);
			alias_done[nalias_done].sym = i; alias_done[nalias_done].target = j; nalias_done++;
			alias_of[k].target = NULL;
		}
	}
	for (int k = 0; k < nalias; k++) if (alias_of[k].target) die(".set: alias target '%s' undefined", alias_of[k].target);
}
/* After every .size is known: an alias without its own .size takes its target's (GAS copies it, like the type).
 * Resolution order puts a chain's inner links first, so a -> b -> c sees c's size through b. */
static void alias_sizes(void) {
	for (int k = 0; k < nalias_done; k++)
		if (!syms[alias_done[k].sym].sized) syms[alias_done[k].sym].size = syms[alias_done[k].target].size;
}

static void parse_line(char *line) {
	parse_seq++;
	if (parse_seq >= seq_subcap) { seq_subcap = seq_subcap ? seq_subcap * 2 : 4096; seq_sub = realloc(seq_sub, seq_subcap * sizeof *seq_sub); }
	seq_sub[parse_seq] = sub_of(cursec);
	/* Peel leading labels ("_start:", "1:") — possibly several — then a directive or one instruction. */
	for (;;) {
		tokenize(line);
		if (ntok == 0) return;
		char *first = toks[0]; size_t n = strlen(first);
		if (n && first[n-1] == ':') {
			first[n-1] = 0; def_label(first);
			char *rest = strchr(line, ':'); line = rest ? rest + 1 : line + strlen(line);   /* re-tokenize remainder */
			continue;
		}
		break;
	}
	cur_stmt = line;
	if (ntok >= 3 && !strcmp(toks[1], ".req")) { md_req(toks[0], toks[2]); return; }   /* `alias .req r4` */
	if (!strcmp(toks[0], ".unreq")) { if (ntok != 2) die(".unreq: expected a name"); md_unreq(toks[1]); return; }
	if (toks[0][0] == '.') do_directive();
	else { map_pending = 1; md_assemble(toks, ntok); if (map_pending) { map_pending = 0; map_insn(); } }   /* the $a comes when the
	                                                    * instruction is EMITTED — after its operands' symbols exist (GAS order) */
}

static void check_fb_resolved(void) {   /* every referenced `Nf` must have been defined by the end */
	for (int i = 0; i < nsym; i++)
		if (syms[i].name && !strncmp(syms[i].name, ".Lfb", 4) && !syms[i].defined) {
			int n = atoi(syms[i].name + 4); die("unresolved forward local label %df", n); }
}

/* Find-or-create the STT_SECTION symbol for section `sec` — a local, value-0 marker for the section
 * itself, the reference a reduced relocation points at (see reduce_local_relocs). */
int section_symbol(int sec) {
	if (secs[sec].secsym < 0)                        /* local, value 0, defined here */
		secs[sec].secsym = sym_add((Sym){ .name = secs[sec].name, .sec = sec, .type = STT_SECTION, .defined = 1 });
	return secs[sec].secsym;
}

/* GNU as "reduces" a relocation against a LOCAL defined symbol to the section symbol + the symbol's
 * section-relative value folded into the in-place addend, so the .o's section bytes carry that offset
 * (e.g. a `.word .Llabel` jump-table entry stores .Llabel's offset, not 0). We do the same for the only
 * local-symbol relocation our backend emits — R_ARM_ABS32 from `.word <local>` (branches to locals are
 * already resolved in md_finish; ldr-literals too). This makes such .text/.rodata bytes match GNU as. */
static void reduce_local_relocs(void) {
	int w = 0;
	for (int i = 0; i < nrel; i++) {
		Reloc *r = &rels[i]; Sym *s = &syms[r->symidx];
		/* `.long start_offset` (made absolute by a later .set): a constant after all, no relocation */
		if (s->defined && s->sec == SEC_ABS && !s->global && !s->weak && r->type == md_r_abs32) {   /* (a global keeps its reloc when
		                                               * the reference came first, GAS: only a value known at parse time folds) */ patch32(r->sec, r->off, read32(r->sec, r->off) + s->value); continue; }
		int local_def = s->defined && !s->global && !s->weak && s->type != STT_SECTION && s->sec >= 0;
		if (local_def && (secs[s->sec].flags & SHF_MERGE) && (r->type == md_r_abs32 || r->type == md_r_rel32) && read32(r->sec, r->off))
			local_def = 0;   /* GAS write.c: a symbol in a MERGE section with a non-zero addend (`.LC0+2`) stays — as section+offset
			                  * the linker's string merging could no longer tell which string the reference points into */
		if (local_def && r->type == md_r_rel32 && s->sec == r->sec) {   /* `sym - .` within one section: a constant, no reloc */
			patch32(r->sec, r->off, read32(r->sec, r->off) + s->value - r->off); continue;
		}
		if (local_def && s->type == STT_FUNC) { rels[w++] = *r; continue; }   /* GAS arm_fix_adjustable: relocs vs FUNCTION symbols are kept (interworking) */
		if (local_def && (r->type == md_r_abs32 || r->type == md_r_rel32)) {
			patch32(r->sec, r->off, read32(r->sec, r->off) + s->value);
			r->symidx = section_symbol(s->sec);
		} else if (local_def && md_is_branch_reloc(r->type)) {   /* b/bl to a local in ANOTHER section: section sym + imm24 addend */
			u32 insn = read32(r->sec, r->off); int32_t A = (int32_t)(insn << 8) >> 6;   /* current addend (bytes) */
			patch32(r->sec, r->off, (insn & 0xff000000u) | ((((int32_t)s->value + A) >> 2) & 0xffffff));
			r->symidx = section_symbol(s->sec);
		}
		rels[w++] = *r;
	}
	nrel = w;
}

/* ------------------------------------------------------------------ GAS macros -------------------- */
/* A line-level preprocessing pass over the input: expand `.macro`/`.rept` and honour `.if`/`.else`, then
 * hand real lines to parse_line. Recursive (macros expand into feed_line), so nested macros/rept/if work. */
typedef struct { char name[64]; char params[32][32]; char *defs[32]; int req[32], vararg[32]; int nparams; char *body[2048]; int nbody; } Macro;
static Macro macros[256]; static int nmacros;
static Macro *macro_find(const char *n) { for (int i = 0; i < nmacros; i++) if (!strcmp(macros[i].name, n)) return &macros[i]; return NULL; }
static int macuid;                                   /* \@ — a unique id per macro expansion */
static struct { int active, taken; } ifs[64]; static int nifs;   /* .if stack */
static int emitting(void) { for (int i = 0; i < nifs; i++) if (!ifs[i].active) return 0; return 1; }
static char *xdup(const char *s) { char *p = malloc(strlen(s) + 1); strcpy(p, s); return p; }
/* Leading whitespace-delimited word of a line -> w; returns the pointer just past it. */
#define LEADMAX 1024   /* lead() buffers: long C-label symbols (.Ll_<func>_<label>) exceed 64 */
static const char *lead(const char *s, char *w) { while (*s==' '||*s=='\t') s++; int i=0; while (*s && *s!=' '&&*s!='\t'&&*s!=',') { if (i >= LEADMAX - 1) die("word too long (>%d chars): %.40s...", LEADMAX - 1, s - i); w[i++]=*s++; } w[i]=0; return s; }

/* .if / .rept take a constant expression — the SAME evaluator as everything else (a private one used to treat
 * every identifier as 0, so `.if SYMBOL` silently picked the wrong branch; /0 silently gave 0). */
static long eval_if(const char *s) { return eval_const_expr(s); }

/* A growable output string (macro substitution must not overflow a fixed buffer). */
typedef struct { char *b; size_t n, cap; } Buf;
static void bput(Buf *o, const char *s, size_t n) {
	if (o->n + n + 1 > o->cap) { o->cap = (o->n + n + 1) * 2 + 64; o->b = realloc(o->b, o->cap); }
	memcpy(o->b + o->n, s, n); o->n += n; o->b[o->n] = 0;
}
/* Substitute \param -> arg, \@ -> uid, \() -> "" in one body line. names/vals: the parameter bindings. */
static char *subst(const char *in, char names[][32], char **vals, int nv, int uid) {
	Buf o = {0}; bput(&o, "", 0);
	for (const char *p = in; *p; ) {
		if (*p != '\\') { bput(&o, p, 1); p++; continue; }
		p++;
		if (*p == '@') { char u[16]; int k = snprintf(u, sizeof u, "%d", uid); bput(&o, u, (size_t)k); p++; continue; }
		if (*p == '(' && p[1] == ')') { p += 2; continue; }
		const char *s0 = p; while (*p == '_' || isalnum((unsigned char)*p)) p++;
		size_t L = (size_t)(p - s0); int found = 0;
		for (int k = 0; k < nv; k++) if (strlen(names[k]) == L && !strncmp(names[k], s0, L)) { bput(&o, vals[k], strlen(vals[k])); found = 1; break; }
		if (!found) { bput(&o, "\\", 1); bput(&o, s0, L); }
	}
	return o.b;
}

static void feed_line(char *line);
static void include_text(char *buf) {   /* assemble another file's text in place (.include) */
	strip_comments(buf);
	char *line = buf, *nl;
	do { nl = strchr(line, '\n'); if (nl) *nl = 0; feed_line(line); line = nl ? nl + 1 : NULL; } while (line);
}
static int exitm;   /* .exitm seen: stop expanding the innermost macro */
/* Invocation: comma-separated args (parens/brackets/braces/quotes nest); `name=value` binds by keyword; a
 * :vararg parameter takes the rest of the line verbatim; missing args take the default, or error if :req. */
static void expand_macro(Macro *m, const char *argline) {
	char *vals[32] = {0}; int pos = 0;
	/* GAS scrubs whitespace first (app.c): a blank next to an operator or comma is dropped; one between two word
	 * characters survives and SEPARATES arguments (`m 1 2 3`, `m 4, 5 6`, but `m 11, 12 + 1` = 11, 13). Quotes kept. */
	char *scr = malloc(strlen(argline) + 1); size_t sn = 0;
	{ const char *q = argline; char inq = 0;
	  while (*q == ' ' || *q == '\t') q++;
	  for (; *q; q++) {
		if (inq) { scr[sn++] = *q; if (*q == '\\' && q[1]) scr[sn++] = *++q; else if (*q == inq) inq = 0; continue; }
		if (*q == '"') { inq = '"'; scr[sn++] = *q; continue; }
		if (*q == '\'') {                      /* a character constant: verbatim, blanks and commas included.
		                                       * GAS also takes a bare `'a` with no closing quote. */
			scr[sn++] = *q;
			if (q[1] == '\\' && q[2]) { scr[sn++] = q[1]; scr[sn++] = q[2]; q += 2; }
			else if (q[1]) { scr[sn++] = q[1]; q += 1; }
			if (q[1] == '\'') { scr[sn++] = q[1]; q += 1; }
			continue;
		}
		if (*q == ' ' || *q == '\t') {
			const char *n = q; while (*n == ' ' || *n == '\t') n++;
			char pv = sn ? scr[sn - 1] : 0, nx = *n;
			const char *ops = "+-*/%&|^<>=!~,()";
			if (pv && nx && !strchr(ops, pv) && !strchr(ops, nx)) scr[sn++] = ' ';
			q = n - 1; continue;
		}
		scr[sn++] = *q;
	  }
	  while (sn && scr[sn - 1] == ' ') sn--;
	  scr[sn] = 0; }
	const char *p = scr;
	while (*p) {
		if (pos < m->nparams && m->vararg[pos]) {   /* the rest, trimmed */
			const char *e = p + strlen(p); while (e > p && (e[-1] == ' ' || e[-1] == '\t')) e--;
			vals[pos] = strndup(p, (size_t)(e - p)); pos++; p = ""; break;
		}
		const char *s0 = p; int depth = 0; char q = 0;
		while (*p && (q || depth || (*p != ',' && *p != ' '))) {   /* a scrubbed blank separates too */
			if (!q && *p == '\'') {                /* a character constant: 'x', '\n', or a bare 'x */
				p++;
				if (*p == '\\' && p[1]) p += 2; else if (*p) p++;
				if (*p == '\'') p++;
				continue;
			}
			if (q) { if (*p == '\\' && p[1]) p++; else if (*p == q) q = 0; }
			else if (*p == '"') q = *p;
			else if (*p == '(' || *p == '[' || *p == '{') depth++;
			else if ((*p == ')' || *p == ']' || *p == '}') && depth) depth--;
			p++;
		}
		const char *e = p; while (e > s0 && (e[-1] == ' ' || e[-1] == '\t')) e--;
		char *a = strndup(s0, (size_t)(e - s0));
		char *eq = strchr(a, '='); int kw = -1;
		if (eq) { size_t kl = (size_t)(eq - a); for (int k = 0; k < m->nparams; k++) if (strlen(m->params[k]) == kl && !strncmp(m->params[k], a, kl)) { kw = k; break; } }
		{ size_t al = strlen(a); if (al >= 2 && a[0] == '"' && a[al - 1] == '"') { memmove(a, a + 1, al - 2); a[al - 2] = 0; eq = kw < 0 ? NULL : eq; } }   /* "arg" -> arg (GAS) */
		if (kw >= 0) { vals[kw] = strdup(eq + 1); free(a); }
		else { if (pos >= m->nparams) die("macro '%s': too many arguments", m->name); vals[pos++] = a; }
		if (*p == ',' || *p == ' ') { p++; while (*p == ' ' || *p == '\t') p++; }
	}
	free(scr);
	for (int k = 0; k < m->nparams; k++) if (vals[k] && !vals[k][0] && (m->defs[k] || m->req[k])) { free(vals[k]); vals[k] = NULL; }   /* blank = omitted (GAS) */
	for (int k = 0; k < m->nparams; k++) if (!vals[k]) {
		if (m->req[k]) die("macro '%s': missing required argument '%s'", m->name, m->params[k]);
		vals[k] = strdup(m->defs[k] ? m->defs[k] : "");
	}
	int uid = macuid++;
	for (int i = 0; i < m->nbody && !exitm; i++) { char *out = subst(m->body[i], m->params, vals, m->nparams, uid); feed_line(out); free(out); }
	exitm = 0;
	for (int k = 0; k < m->nparams; k++) free(vals[k]);
}

/* Collection state for the body of a .macro / .rept being read. */
static int coll_mode, coll_depth, coll_n; static char *coll_body[2048];   /* mode 1 .macro, 2 .rept, 3 .irp, 4 .irpc */
static char coll_name[64], coll_params_src[1024]; static long coll_reptn;

static void feed_line(char *line) {
	{ const char *q = line; while (*q==' '||*q=='\t') q++; if (*q == '#' && !coll_mode) return; }   /* a `#` line is a comment —
	                                                    * all of it: `#.type f, %function; .size f, .-f` runs nothing (GAS) */
	if (!coll_mode) {   /* GAS ';' statement separator: split one line into statements (quote-aware; '@' = comment) */
		int inq = 0; char qc = 0;
		for (char *p = line; *p; p++) {
			if (inq) { if (*p == qc) inq = 0; else if (*p == '\\' && p[1]) p++; }
			else if (*p == '"' || *p == '\'') { inq = 1; qc = *p; }
			else if (*p == '@' && !(p > line && p[-1] == '\\')) break;   /* `\@` is the macro counter */
			else if (*p == ';') { *p = 0; feed_line(line); feed_line(p + 1); return; }
		}
	}
	{ const char *q = line; while (*q==' '||*q=='\t') q++; if (*q == '#') return; }   /* cpp line marker / `#` comment */
	char w[LEADMAX]; const char *rest = lead(line, w);
	if (coll_mode) {                                 /* gathering a macro/rept body until the matching end */
		if (!strcmp(w,".macro")||!strcmp(w,".rept")||!strcmp(w,".rep")||!strcmp(w,".irp")||!strcmp(w,".irpc")) coll_depth++;
		if (!strcmp(w,".endm")||!strcmp(w,".endr")) { if (--coll_depth == 0) {
			if (coll_mode == 1) {                    /* finish a .macro definition */
				if (nmacros >= 256) die("too many .macro definitions (>256)");
				Macro *m = &macros[nmacros++]; memset(m, 0, sizeof *m); strncpy(m->name, coll_name, 63);
				const char *s = coll_params_src;     /* params: name[=default][:req|:vararg], comma/space separated */
				while (*s) {
					while (*s==' '||*s=='\t'||*s==',') s++;
					if (!*s) break;
					if (m->nparams >= 32) die("macro '%s': too many parameters (>32)", m->name);
					int k = m->nparams; char *d = m->params[k]; int i = 0;
					while (*s == '_' || isalnum((unsigned char)*s)) { if (i >= 31) die("macro parameter name too long"); d[i++] = *s++; }
					d[i] = 0; if (!i) die("macro '%s': bad parameter list '%s'", m->name, coll_params_src);
					if (*s == ':') { s++; if (!strncmp(s, "req", 3)) { m->req[k] = 1; s += 3; } else if (!strncmp(s, "vararg", 6)) { m->vararg[k] = 1; s += 6; } else die("macro '%s': unknown qualifier ':%s'", m->name, s); }
					{ const char *t2 = s; while (*t2 == ' ' || *t2 == '\t') t2++; if (*t2 == '=') s = t2; }   /* `offset = 0` (GAS allows the spaces) */
					if (*s == '=') { s++; while (*s == ' ' || *s == '\t') s++; const char *v = s; while (*s && *s != ',' && *s != ' ' && *s != '\t') s++; m->defs[k] = strndup(v, (size_t)(s - v)); }
					if (*s == ':') { s++; if (!strncmp(s, "req", 3)) { m->req[k] = 1; s += 3; } else if (!strncmp(s, "vararg", 6)) { m->vararg[k] = 1; s += 6; } else die("macro '%s': unknown qualifier ':%s'", m->name, s); }
					if (*s && *s != ',' && *s != ' ' && *s != '\t') die("macro '%s': bad parameter list '%s'", m->name, coll_params_src);
					m->nparams++;
				}
				for (int k = 0; k + 1 < m->nparams; k++) if (m->vararg[k]) die("macro '%s': :vararg must be the last parameter", m->name);
				m->nbody = coll_n; for (int i=0;i<coll_n;i++) m->body[i]=coll_body[i];
				coll_mode = 0; coll_n = 0;
			} else if (coll_mode >= 3) {             /* finish .irp var, v1, v2... / .irpc var, chars */
				char *bd[2048]; int bn = coll_n, irpc = coll_mode == 4; char var[1][32]; strncpy(var[0], coll_name, 31); var[0][31] = 0;
				char src[1024]; strncpy(src, coll_params_src, sizeof src - 1); src[sizeof src - 1] = 0;
				for (int i=0;i<bn;i++) bd[i]=coll_body[i];
				coll_mode = 0; coll_n = 0;
				const char *q = src; while (*q == ' ' || *q == '\t') q++;
				if (irpc) {
					for (; *q && *q != ' ' && *q != '\t'; q++) { char c[2] = { *q, 0 }; char *v[1] = { c };
						for (int i=0;i<bn;i++) { char *o = subst(bd[i], var, v, 1, macuid); feed_line(o); free(o); } }
				} else if (!*q) {   /* no values: one pass with the variable empty (GAS) */
					char e[1] = ""; char *v[1] = { e }; for (int i=0;i<bn;i++) { char *o = subst(bd[i], var, v, 1, macuid); feed_line(o); free(o); }
				} else {
					for (;;) {
						const char *s0 = q; while (*q && *q != ',') q++;
						const char *e = q; while (e > s0 && (e[-1] == ' ' || e[-1] == '\t')) e--;
						char *val = strndup(s0, (size_t)(e - s0)); char *v[1] = { val };
						for (int i=0;i<bn;i++) { char *o = subst(bd[i], var, v, 1, macuid); feed_line(o); free(o); }
						free(val);
						if (*q != ',') break;
						q++; while (*q == ' ' || *q == '\t') q++;
						if (!*q) break;                         /* a trailing comma adds no empty value (GAS) */
					}
				}
				for (int i=0;i<bn;i++) free(bd[i]);
			} else {                                 /* finish a .rept: SNAPSHOT + reset BEFORE expanding, so
			                                          * the body lines get processed rather than re-collected */
				char *bd[2048]; int bn = coll_n; long rn = coll_reptn;
				for (int i=0;i<bn;i++) bd[i]=coll_body[i];
				coll_mode = 0; coll_n = 0;
				for (long k=0;k<rn;k++) for (int i=0;i<bn;i++) { char *c=xdup(bd[i]); feed_line(c); free(c); }
				for (int i=0;i<bn;i++) free(bd[i]);
			}
			return;
		} }
		if (coll_n >= 2048) die("macro/.rept body too long (>2048 lines)");
		coll_body[coll_n++] = xdup(line); return;
	}
	if (!strcmp(w, ".macro")) { const char *r=rest; while(*r==' '||*r=='\t'||*r==',')r++; char nm[LEADMAX]; const char *a=lead(r,nm);
		if (strlen(a) >= sizeof coll_params_src) die(".macro: parameter list too long");
		if (strlen(nm) >= sizeof coll_name) die(".macro: name too long: %s", nm);
		strcpy(coll_name,nm); strcpy(coll_params_src,a); coll_mode=1; coll_depth=1; coll_n=0; return; }
	if (!strcmp(w, ".rept") || !strcmp(w, ".rep"))  { coll_reptn = emitting()?eval_if(rest):0; if (coll_reptn < 0) die(".rept: negative count"); coll_mode=2; coll_depth=1; coll_n=0; return; }
	if (!strcmp(w, ".irp") || !strcmp(w, ".irpc")) {   /* .irp var, v1, v2 ... / .irpc var, chars */
		const char *r = rest; while (*r == ' ' || *r == '\t') r++;
		char nm[LEADMAX]; const char *a = lead(r, nm); if (!nm[0]) die("%s: missing variable", w);
		while (*a == ' ' || *a == '\t') a++;
		if (*a == ',') a++;                           /* ONE separator: `.irp c,,eq,ne` — the first value is empty */
		while (*a == ' ' || *a == '\t') a++;
		if (strlen(nm) >= sizeof coll_name) die("%s: name too long: %s", w, nm);
		strcpy(coll_name, nm);
		if (strlen(a) >= sizeof coll_params_src) die("%s: value list too long", w);
		strcpy(coll_params_src, a);
		if (!emitting()) { coll_params_src[0] = 0; }
		coll_mode = !strcmp(w, ".irp") ? 3 : 4; coll_depth = 1; coll_n = 0; return;
	}
	if (!strcmp(w, ".exitm")) { if (emitting()) exitm = 1; return; }
	if (!strcmp(w, ".if"))     { int on = emitting() && eval_if(rest)!=0; if (nifs >= 64) die("too many nested .if (>64)"); ifs[nifs].active=on; ifs[nifs].taken=on; nifs++; return; }
	if (!strcmp(w, ".ifdef"))  { char nm[LEADMAX]; lead(rest,nm); int on = emitting() && sym_find(nm)>=0 && syms[sym_find(nm)].defined; if (nifs >= 64) die("too many nested .if (>64)"); ifs[nifs].active=on; ifs[nifs].taken=on; nifs++; return; }
	if (!strcmp(w, ".ifndef") || !strcmp(w, ".ifnotdef")) { char nm[LEADMAX]; lead(rest,nm); int on = emitting() && !(sym_find(nm)>=0 && syms[sym_find(nm)].defined); if (nifs >= 64) die("too many nested .if (>64)"); ifs[nifs].active=on; ifs[nifs].taken=on; nifs++; return; }
	if (!strcmp(w, ".ifc") || !strcmp(w, ".ifnc")) {   /* GAS string compare: .ifc a,b (same) / .ifnc a,b (differ) */
		const char *comma = strchr(rest, ','); char a[128] = "", b[128] = "";
		if (comma) { int la = (int)(comma - rest); if (la > 127) la = 127; memcpy(a, rest, la); a[la] = 0; strncpy(b, comma + 1, 127); }
		char *pa = a; while (*pa==' '||*pa=='\t') pa++; { char *e = pa + strlen(pa); while (e>pa && (e[-1]==' '||e[-1]=='\t'||e[-1]=='\n'||e[-1]=='\r')) *--e = 0; }
		char *pb = b; while (*pb==' '||*pb=='\t') pb++; { char *e = pb + strlen(pb); while (e>pb && (e[-1]==' '||e[-1]=='\t'||e[-1]=='\n'||e[-1]=='\r')) *--e = 0; }
		int same = !strcmp(pa, pb), on = emitting() && (!strcmp(w, ".ifc") ? same : !same);
		if (nifs >= 64) die("too many nested .if (>64)");
		ifs[nifs].active=on; ifs[nifs].taken=on; nifs++; return;
	}
	if (!strcmp(w, ".ifb") || !strcmp(w, ".ifnb")) {   /* blank-argument test (after macro substitution) */
		const char *r = rest; while (*r == ' ' || *r == '\t') r++;
		int blank = !*r, on = emitting() && (!strcmp(w, ".ifb") ? blank : !blank);
		if (nifs >= 64) die("too many nested .if (>64)");
		ifs[nifs].active = on; ifs[nifs].taken = on; nifs++; return;
	}
	{   /* .ifeq/.ifne/.ifgt/.ifge/.iflt/.ifle expr: its value against 0; .ifnotdef = .ifndef */
		static const char *const cmp0[] = { ".ifeq", ".ifne", ".ifgt", ".ifge", ".iflt", ".ifle", NULL };
		for (int k = 0; cmp0[k]; k++) if (!strcmp(w, cmp0[k])) {
			int on = 0;
			if (emitting()) { long v = eval_if(rest); on = k == 0 ? v == 0 : k == 1 ? v != 0 : k == 2 ? v > 0 : k == 3 ? v >= 0 : k == 4 ? v < 0 : v <= 0; }
			if (nifs >= 64) die("too many nested .if (>64)");
			ifs[nifs].active = on; ifs[nifs].taken = on; nifs++; return;
		}
	}
	if (!strcmp(w, ".ifeqs") || !strcmp(w, ".ifnes")) {   /* .ifeqs "a", "b": the two quoted strings (GAS: quotes required) */
		int on = 0;
		if (emitting()) {
			const char *r = rest; char a[512], b[512]; char *outs[2] = { a, b };
			for (int k = 0; k < 2; k++) {
				while (*r == ' ' || *r == '\t' || (k && *r == ',')) r++;
				if (*r != '"') die("%s: expected a quoted string", w);
				size_t n = 0; for (r++; *r && *r != '"'; r++) { if (n >= 511) die("%s: string too long", w); outs[k][n++] = *r; }
				if (*r != '"') die("%s: missing closing quote", w);
				outs[k][n] = 0; r++;
			}
			int same = !strcmp(a, b); on = !strcmp(w, ".ifeqs") ? same : !same;
		}
		if (nifs >= 64) die("too many nested .if (>64)");
		ifs[nifs].active = on; ifs[nifs].taken = on; nifs++; return;
	}
	if (!strcmp(w, ".elseif")) {
		if (!nifs) die(".elseif without a matching .if");
		int parent = 1; for (int i = 0; i < nifs - 1; i++) if (!ifs[i].active) parent = 0;
		if (!parent || ifs[nifs - 1].taken) ifs[nifs - 1].active = 0;
		else { int on = eval_if(rest) != 0; ifs[nifs - 1].active = on; ifs[nifs - 1].taken = on; }
		return;
	}
	if (!strcmp(w, ".else"))   { if (!nifs) die(".else without a matching .if"); { int parent=1; for(int i=0;i<nifs-1;i++) if(!ifs[i].active)parent=0; ifs[nifs-1].active = parent && !ifs[nifs-1].taken; if(ifs[nifs-1].active) ifs[nifs-1].taken=1; } return; }
	if (!strcmp(w, ".endif"))  { if (!nifs) die(".endif without a matching .if"); nifs--; return; }
	if (!strcmp(w, ".err"))    { if (emitting()) die(".err directive reached (assembly assertion failed)"); return; }
	if (!emitting() || exitm) return;                /* inside a false .if branch (or after .exitm) */
	Macro *m = macro_find(w);
	if (m) { expand_macro(m, rest); return; }
	{   /* `label: macro args` — the label here, then the rest of the line (a macro may follow a label) */
		const char *c = strchr(w, ':');
		if (c && c > w && c[1] != ':' && (c[1] || *rest)) {
			int ok = 1; for (const char *q = w; q < c; q++) if (!(isalnum((unsigned char)*q) || *q == '_' || *q == '.' || *q == '$')) ok = 0;
			char after[LEADMAX]; snprintf(after, sizeof after, "%s", c + 1);
			char *rest2 = NULL;
			if (ok) { size_t n = strlen(after) + strlen(rest) + 2; rest2 = malloc(n); snprintf(rest2, n, "%s%s", after, rest); }
			if (ok) { char wn[LEADMAX]; lead(rest2, wn);
				if (macro_find(wn)) { char lab[LEADMAX]; snprintf(lab, sizeof lab, "%.*s:", (int)(c - w), w); parse_line(lab); feed_line(rest2); free(rest2); return; } }
			free(rest2);
		}
	}
	parse_line(line);
}

/* After parsing: every subsection appended to its section, in number order — its bytes, then everything that points
 * into it (symbols, relocations, pending fixups) moved by where it landed. A subsection that begins with an .align is
 * padded to it there (as GAS lays the frag chains end to end: zero bytes to a word, then NOPs, in code); any other
 * alignment it needs must already hold at that spot (GAS would relax: we fail loud rather than guess). */
static void shift_refs(int from, int to, u32 d, u32 prepad) {   /* prepad: where its leading .align's padding begins */
	for (int k = 0; k < nsym; k++) if (syms[k].sec == from && syms[k].type != STT_SECTION) {
		syms[k].sec = to; syms[k].value += k == secs[from].lead ? prepad : d;   /* the leading .align's own symbol marks its padding */
	}
	int fs = secs[from].secsym;
	for (int k = 0; k < nrel; k++) {
		if (rels[k].sec == from) { rels[k].sec = to; rels[k].off += d; rels[k].sub = secs[from].subsec; }
		if (fs >= 0 && rels[k].symidx == fs) { rels[k].symidx = section_symbol(to); patch32(rels[k].sec, rels[k].off, read32(rels[k].sec, rels[k].off) + d); }
	}
	for (int k = 0; k < ndreloc; k++) if (fs >= 0 && dreloc[k].off.sym == fs) { dreloc[k].off.sym = section_symbol(to); dreloc[k].off.c += d; }
	if (fs >= 0) syms[fs].name = NULL;                  /* (the internal section had a symbol only for EHABI entries, `.reloc .`) */
	for (int k = 0; k < ndword; k++) if (dword[k].sec == from) { dword[k].sec = to; dword[k].off += d; }
	for (int k = 0; k < ndsmall; k++) if (dsmall[k].sec == from) { dsmall[k].sec = to; dsmall[k].off += d; }
	for (int k = 0; k < ndtext; k++) if (dtext[k].sec == from) { dtext[k].sec = to; dtext[k].off += d; }
	for (int k = 0; k < ndset; k++) if (dset[k].sec == from) { dset[k].sec = to; dset[k].at += d; }
	for (int k = 0; k < npadmap; k++) if (padmap[k].sec == from) { padmap[k].sec = to; padmap[k].d_at += d; padmap[k].a_at += d; }
	md_remap_section(from, to, d);
}
static void code_pad(u32 bytes, int thumb) {   /* code alignment (GAS arm_handle_align): zero bytes to an instruction
	* boundary, then NOPs — ARM: v6K `nop`; Thumb: one narrow `nop` to a word, then wide `nop.w`s (v6T2) — and of a pad
	* over 63 bytes only its low 6 bits' worth; the rest is zeros (GAS's frag repeats its zero fill pattern there) */
	u32 pad = bytes ? (bytes - (u32)(secs[cursec].len % bytes)) % bytes : 0, nop_end = pad > 63 ? pad & 63 : pad, z = nop_end & (thumb ? 1 : 3);
	if (z) {   /* sub-instruction zero fill: its $d + the NOPs' $a/$t are written after parsing */
		padmap = grow(padmap, npadmap, &padcap, sizeof *padmap);
		padmap[npadmap].sec = cursec; padmap[npadmap].d_at = (u32)secs[cursec].len; padmap[npadmap].a_at = (u32)secs[cursec].len + z;
		padmap[npadmap].state = thumb ? MAP_THUMB : MAP_ARM; npadmap++;
		for (u32 k = 0; k < z; k++) { u8 zb = 0; emit(&zb, 1); }
	}
	u32 k = z;
	if (thumb && ((nop_end - k) & 2)) { emit16(0xbf00u); k += 2; }
	for (; k < nop_end; k += 4) { if (thumb) { emit16(0xf3afu); emit16(0x8000u); } else emit32(0xe320f000u); }   /* (GAS, -march=armv7-a) */
	if (thumb && ((pad - k) & 2)) { emit16(0); k += 2; }
	for (; k < pad; k += 4) emit32(0);
}
/* GAS subsegs_finish_section: every subsection's frag chain ends in an alignment frag — none, except a code section's
 * last one (to min(4, the section's alignment)) and an SHF_MERGE section's (to its entry size). Its arm_init_frag puts
 * the section back in its natural state (ARM for code, data otherwise) at that spot: a mapping symbol there, and
 * from UNDEF (a code section that held only data so far) a $d at 0 first. */
static int debug_section(int s) {   /* SEC_DEBUGGING: no mapping symbols */
	const char *n = secs[s].name;
	return !strncmp(n, ".debug", 6) || !strncmp(n, ".zdebug", 7) || !strncmp(n, ".stab", 5) || !strcmp(n, ".line") || !strncmp(n, ".gnu.debuglto_", 14);
}
static void sub_end(int b, int sub, int last, u32 salign) {   /* sub: the subsection just finished (its frag's mode) */
	int code = (secs[b].flags & SHF_EXECINSTR) != 0, fm = frag_mode(sub), state = !code ? MAP_DATA : fm ? MAP_THUMB : MAP_ARM;
	u32 a = last && code ? (salign < 4 ? salign : 4) : 1;
	if ((secs[b].flags & SHF_MERGE) && secs[b].entsize) { u32 e = secs[b].entsize & -secs[b].entsize; if (e > a) a = e; }
	cursec = b;
	if (!debug_section(b) && secs[b].mapstate != state) {
		for (int k = 0; k < nsym; k++) if (is_mapsym(k) && syms[k].sec == b && syms[k].value == (u32)secs[b].len) syms[k].name = NULL;   /* in an earlier frag */
		map_to(state, 0);
	}
	if (a <= 1 || secs[b].len % a == 0) return;
	if (secs[b].type == SHT_NOBITS) secs[b].len += a - secs[b].len % a;
	else if (code) code_pad(a, fm);
	else while (secs[b].len % a) { u8 z = 0; emit(&z, 1); }
}
static void merge_subsections(void) {
	int save = cursec;
	for (int b = 0; b < nsec; b++) {
		if (secs[b].base >= 0) continue;
		/* Nothing to merge, and — the point of the counter — no reason to scan for it. This loop used to run
		 * two full nsec scans for EVERY base section, including the ones with no subsections, which is all of
		 * them in an ordinary object: 27,000 sections meant ~1.5 billion comparisons finding nothing.
		 * Equivalent to the general path below when there are no subsections (salign stays secs[b].align and
		 * the first `best` search returns -1). */
		if (!secs[b].nsub) { sub_end(b, b, 1, secs[b].align); continue; }
		u32 salign = secs[b].align;   /* the section's recorded alignment: every subsection's */
		for (int k = 0; k < nsec; k++) if (secs[k].base == b && secs[k].align > salign) salign = secs[k].align;
		for (int done = b;;) {   /* the next-numbered subsection of b */
			int best = -1; for (int k = 0; k < nsec; k++) if (secs[k].base == b && !secs[k].merged && (best < 0 || secs[k].subsec < secs[best].subsec)) best = k;
			sub_end(b, done, best < 0, salign);
			done = best;
			if (best < 0) break;
			Section *sub = &secs[best]; cursec = b;
			u32 prepad = (u32)secs[b].len;
			if (sub->first_align > 1 && secs[b].len % sub->first_align) {
				if (secs[b].type == SHT_NOBITS) secs[b].len += sub->first_align - secs[b].len % sub->first_align;
				else if (secs[b].flags & SHF_EXECINSTR) code_pad(sub->first_align, sub->first_align_mode);
				else while (secs[b].len % sub->first_align) { u8 z = 0; emit(&z, 1); }
			}
			if (sub->pad_align > 1 && secs[b].len % sub->pad_align)   /* (GAS lays its frags where they land: an instruction may
			                                                           * sit unaligned; an inner .align's padding would change) */
				die("subsection %d of %s lands at offset %zu: its .align %u padding was computed for an aligned start (not supported)", sub->subsec, secs[b].name, secs[b].len, sub->pad_align);
			u32 d = (u32)secs[b].len;
			/* check_mapping_symbols: a mapping symbol followed only by empty frags, then one that starts with its own,
			 * marks nothing — so the subsection's first one (its .align's, or one at its first byte right here) wins */
			int lead = sub->lead >= 0 && syms[sub->lead].name ? sub->lead : -1, at0 = -1;
			for (int k = 0; k < nsym; k++) if (k != lead && is_mapsym(k) && syms[k].sec == best && syms[k].value == 0) at0 = k;
			if (lead >= 0 && at0 >= 0 && prepad == d) { syms[lead].name = NULL; lead = -1; }   /* its padding came out empty */
			if (lead >= 0 || (at0 >= 0 && prepad == d))
				for (int k = 0; k < nsym; k++) if (is_mapsym(k) && syms[k].sec == b && syms[k].value == prepad) syms[k].name = NULL;
			if (secs[b].type == SHT_NOBITS) secs[b].len += sub->len; else if (sub->len) emit(sub->data, sub->len);
			if (sub->align > secs[b].align) secs[b].align = sub->align;
			shift_refs(best, b, d, prepad);
			sub->merged = 1; sub->len = 0;
		}
	}
	cursec = save;
}

/* ------------------------------------------------------------------ driver ------------------------ */
int noexecstack;   /* --noexecstack: an empty .note.GNU-stack (the object needs no executable stack) */
int main(int argc, char **argv) {
	const char *out = "a.out", *in = NULL;
	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		if (!strcmp(a, "-o") && i + 1 < argc) out = argv[++i];
		else if (!strncmp(a, "-I", 2)) { if (nincdirs == 64) die("too many -I directories"); incdirs[nincdirs++] = a[2] ? a + 2 : (i + 1 < argc ? argv[++i] : (die("-I needs a directory"), "")); }
		else if (!strcmp(a, "--version")) { printf("GNU assembler (os-as, a from-scratch GAS-compatible assembler) 2.42\n"); return 0; }   /* the kernel's
		                                         * as-version.sh accepts "GNU assembler ... <version>" (the syntax we implement) */
		else if (!strcmp(a, "-v") || !strcmp(a, "--verbose")) fprintf(stderr, "GNU assembler version 2.42 (os-as)\n");
		else if (!strcmp(a, "--noexecstack")) noexecstack = 1;
		else if (!strcmp(a, "--execstack")) die("--execstack is not supported (our code needs no executable stack)");
		else if (!strcmp(a, "--fatal-warnings") || !strcmp(a, "-W") || !strcmp(a, "--no-warn") || !strcmp(a, "--warn")) ;   /* we emit no warnings */
		else if (md_parse_option(a)) ;          /* -march= -mcpu= -mfpu= -mfloat-abi= -meabi= -EL ... */
		else if (a[0] != '-' || !strcmp(a, "-")) { if (in) die("more than one input file"); in = a; }
		else die("unknown option '%s'", a);
	}
	if (!in) die("usage: as [-o out.o] [-I dir] [-march= -mcpu= -mfpu= -mfloat-abi= -meabi=5 -EL] [--noexecstack] in.s|-");

	FILE *f = !strcmp(in, "-") ? stdin : fopen(in, "rb"); if (!f) die("cannot open %s", in);
	size_t sz = 0, cap = 1 << 16, r; char *buf = malloc(cap);
	while ((r = fread(buf + sz, 1, cap - sz - 1, f)) > 0) { sz += r; if (sz + 1 >= cap) buf = realloc(buf, cap *= 2); }
	buf[sz] = 0; if (f != stdin) fclose(f);

	/* GAS starts every object with .text/.data/.bss (in that order) and assembles into .text by default. */
	sec_get(".text", SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR); sec_get(".data", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE);
	sec_get(".bss", SHT_NOBITS, SHF_ALLOC | SHF_WRITE); cursec = sec_find(".text"); prevsec = cursec;
	strip_comments(buf);
	char *line = buf, *nl;
	do { nl = strchr(line, '\n'); if (nl) *nl = 0; feed_line(line); line = nl ? nl + 1 : NULL; } while (line);
	if (coll_mode) die("unterminated .macro/.rept");
	if (nifs) die("unterminated .if");

	md_flush_pools();       /* GAS dumps pending literal pools at the end of parsing (before fixups/write) */
	merge_subsections();    /* (pools first: each belongs to its own subsection) */
	check_fb_resolved();
	resolve_aliases();      /* .set name, target — targets may be defined after the .set */
	for (int k = 0; k < ndset; k++) {   /* .set with forward references, now every label is known */
		int save = cursec; cursec = dset[k].sec; dot_at = dset[k].at;
		XVal v = expr_x(dset[k].text); expr_fold(&v);
		if (v.dot == -1 && v.sym >= 0 && v.msym < 0 && syms[v.sym].defined && syms[v.sym].sec == dset[k].sec) { v.c += (long)syms[v.sym].value - dset[k].at; v.sym = -1; v.dot = 0; }
		if (v.dot == 1 && v.msym >= 0 && v.sym < 0 && syms[v.msym].defined && syms[v.msym].sec == dset[k].sec) { v.c += (long)dset[k].at - (long)syms[v.msym].value; v.msym = -1; v.dot = 0; }
		if (v.dot == 1 && v.sym < 0 && v.msym < 0) { v.sym = -2; }
		dot_at = -1; cursec = save;
		Sym *t = &syms[dset[k].sym];
		if (v.sym == -2) { t->sec = dset[k].sec; t->value = (u32)(dset[k].at + v.c); t->defined = 1; }   /* `. + k` */
		else if (!v.dot && v.sym < 0 && v.msym < 0) { t->sec = SEC_ABS; t->value = (u32)v.c; t->defined = 1; }
		else if (!v.dot && v.sym >= 0 && v.msym < 0 && syms[v.sym].defined) { t->sec = syms[v.sym].sec; t->value = (u32)((long)syms[v.sym].value + v.c); t->defined = 1; }
		else die(".set %s: can't be resolved (undefined or cross-section)", t->name);
	}
	for (int k = 0; k < ndreloc; k++) {   /* .reloc: at a label's place, against its symbol */
		XVal o = dreloc[k].off, sv = dreloc[k].sym; expr_fold(&o); expr_fold(&sv);
		int sec; u32 at;
		if (o.sym < 0 && o.msym < 0 && !o.dot) { sec = dreloc[k].sec; at = (u32)o.c; }   /* a constant: an offset in the section */
		else {
			if (o.sym < 0 || o.msym >= 0 || !syms[o.sym].defined || syms[o.sym].sec < 0) die(".reloc: the offset must be a place in a section");
			sec = syms[o.sym].sec; at = syms[o.sym].type == STT_SECTION ? (u32)o.c : syms[o.sym].value + (u32)o.c;
		}
		if (sv.msym >= 0 || sv.dot) die(".reloc: the symbol operand must be a symbol [+ constant]");
		if (sv.c) die(".reloc: an addend on the symbol is not supported (REL: it lives in the instruction)");
		reloc_seq = dreloc[k].seq; add_reloc(sec, at, sv.sym >= 0 ? sv.sym : section_symbol(sec), dreloc[k].type); reloc_seq = -1;
		rels[nrel - 1].dir = 1;
	}
	for (int k = 0; k < ndtext; k++) {   /* operands deferred as text: every label is known now */
		int save = cursec; cursec = dtext[k].sec; dot_at = dtext[k].off;
		const char *sv = ep; ep = dtext[k].text; RVal r = e_or(); ews(); if (*ep) die("expression: junk '%s' in '%s'", ep, dtext[k].text); ep = sv;
		r = fold_msym(r);
		if (r.dot == 1 && r.sym < 0 && r.msym < 0) { r.c += dot_at; r.dot = 0; r.sym = section_symbol(cursec); }   /* `. + k` */
		if (r.dot == -1 && r.sym >= 0 && syms[r.sym].defined && syms[r.sym].sec == cursec) { r.c += (long)syms[r.sym].value - dot_at; r.sym = -1; r.dot = 0; }
		if (r.sym >= 0 && r.msym < 0 && syms[r.sym].defined && syms[r.sym].sec == SEC_ABS) { r.c += (long)(int32_t)syms[r.sym].value; r.sym = -1; }
		int sz = dtext[k].size; u8 *d = secs[cursec].data + dtext[k].off;
		if (r.msym >= 0 || r.dot || (r.sym >= 0 && sz != 4)) die("'%s': not a constant (or a single relocatable symbol) after all", dtext[k].text);
		if (sz < 8) { long lim = 1L << (8 * sz); if (sz < 4 && (r.c >= lim || r.c < -(lim / 2))) die("'%s': value %ld doesn't fit in %d byte(s)", dtext[k].text, r.c, sz); }
		for (int b = 0; b < sz; b++) d[b] = (u8)((unsigned long)r.c >> (8 * b));
		if (r.sym >= 0) { reloc_seq = dtext[k].seq; add_reloc(cursec, dtext[k].off, r.sym, md_r_abs32); reloc_seq = -1; }
		dot_at = -1; cursec = save;
	}
	resolve_deferred();     /* data words / .size that referenced not-yet-defined labels */
	alias_sizes();          /* ...then aliases take their targets' sizes */
	md_finish();            /* let the arch backend resolve its own end-of-pass fixups (ldr literals) */
	reduce_local_relocs();  /* fold local-symbol relocs to section-symbol + in-place value (GNU parity) */
	flush_padmaps();
	for (int i = 0; i < nsec; i++) if (!secs[i].merged) section_symbol(i);   /* the sections still without one (.comment) get theirs now */
	if (noexecstack && sec_find(".note.GNU-stack") < 0) sec_get(".note.GNU-stack", SHT_PROGBITS, 0);
	dwline_emit();          /* .debug_line, now that every address is known */
	dwinfo_emit();          /* ...and the minimal CU that lets a consumer find it */
	md_emit_attributes();   /* GAS writes the build-attribute section last */
	drop_end_mapsyms();   /* a mapping symbol at a section's end marks nothing */
	obj_write(out);
	return 0;
}
