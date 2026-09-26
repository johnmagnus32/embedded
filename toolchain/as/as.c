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
#include "as.h"

/* ------------------------------------------------------------------ tables (the shared model) ----- */
Section secs[MAXSEC]; int nsec; int cursec = -1;
/* GAS section model: `prevsec` = the section before the most recent switch (what `.previous` returns to);
 * .pushsection saves the (current, previous) PAIR and .popsection restores both. */
static int secstack[32][2], secsp, prevsec = -1;
Sym syms[MAXSYM]; int nsym;
Reloc rels[MAXREL]; int nrel;
/* Numeric local labels (GAS: ANY decimal number, e.g. the kernel's `9998:`) — GAS's design: definition k of N:
 * is a hidden local symbol `.Lfb<N>$<k>`; `Nb` names instance k (the latest), `Nf` instance k+1 (created now,
 * defined when the next N: appears). Everything else — branches, adr, ldr, data words, expressions — then uses
 * ordinary symbols, so forward refs, cross-section refs and `1f - 1b` all just work. */
#define MAXLOCAL 4096
static struct { int num, count; } fbtab[MAXLOCAL]; static int nfbtab;
static int fb_slot(int n) {
	for (int i = 0; i < nfbtab; i++) if (fbtab[i].num == n) return i;
	if (nfbtab >= MAXLOCAL) die("too many distinct numeric local labels (>%d)", MAXLOCAL);
	fbtab[nfbtab].num = n; fbtab[nfbtab].count = 0; return nfbtab++;
}

void die(const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	fputs("as: ", stderr); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap); exit(1);
}

int sec_find(const char *name) { for (int i = 0; i < nsec; i++) if (!strcmp(secs[i].name, name)) return i; return -1; }
/* ARM mapping symbols ($a = ARM code starts here, $d = data), placed with GAS's rules (tc-arm.c mapping_state /
 * arm_init_frag / check_mapping_symbols): per-section state UNDEF/ARM/DATA. An instruction enters ARM (and if the
 * section was UNDEF with bytes already in it, a $d at 0 first). A data directive enters DATA, but from UNDEF
 * that's DEFERRED (no symbol). An alignment/fill "frag" (.align/.space/...) marks DATA at its start even from
 * UNDEF; code-section alignment pads with NOPs ($a) after any sub-word zero bytes ($d). Two mapping symbols at
 * one address: the later wins; one at the very end of a section is dropped. objdump and ld rely on these. */
enum { MAP_UNDEF, MAP_ARM, MAP_DATA };
static int mapstate[MAXSEC];
static void add_mapsym(int state, u32 value) {
	for (int i = nsym - 1; i >= 0; i--)   /* same address as this section's latest mapping symbol: replace it */
		if (syms[i].name && syms[i].name[0] == '$' && syms[i].sec == cursec) { if (syms[i].value == value) syms[i].name = NULL; break; }
	if (nsym >= MAXSYM) die("too many symbols");
	syms[nsym++] = (Sym){ state == MAP_ARM ? "$a" : "$d", cursec, value, 0, 0, STT_NOTYPE, 1, 0 };
}
static void map_to(int state, int deferred_ok) {   /* deferred_ok: a plain data directive (UNDEF->DATA waits) */
	if (cursec < 0) return;   /* NOBITS too: GAS marks .bss alignment/fill frags with $d */
	int *m = &mapstate[cursec]; u32 o = (u32)secs[cursec].len;
	if (*m == state) return;
	if (*m == MAP_UNDEF && state == MAP_DATA && deferred_ok) return;
	if (*m == MAP_UNDEF && state == MAP_ARM && o > 0) add_mapsym(MAP_DATA, 0);
	*m = state; add_mapsym(state, o);
}
static void sec_align_at_least(u32 a) { if (cursec >= 0 && secs[cursec].align < a) secs[cursec].align = a; }
void map_insn(void) { map_to(MAP_ARM, 0); sec_align_at_least(4); }
void map_pool_data(void) { mapstate[cursec] = MAP_DATA; add_mapsym(MAP_DATA, (u32)secs[cursec].len); }   /* s_ltorg: $d unconditionally */
/* GAS writes code-alignment padding (and its $d/$a pair) after parsing, so those symbols come LAST in the table. */
static struct { int sec; u32 d_at, a_at; } padmap[4096]; static int npadmap;
static void flush_padmaps(void) {
	for (int i = 0; i < npadmap; i++) {
		int sec = padmap[i].sec, save = cursec, have_a = 0; cursec = sec;
		for (int k = 0; k < nsym; k++) if (syms[k].name && syms[k].name[0] == '$' && syms[k].sec == sec) {
			if (syms[k].value == padmap[i].d_at) syms[k].name = NULL;          /* insert_data_mapping_symbol: replace */
			else if (syms[k].value == padmap[i].a_at) have_a = 1;              /* next frag already starts with one */
		}
		if (nsym >= MAXSYM) die("too many symbols");
		syms[nsym++] = (Sym){ "$d", sec, padmap[i].d_at, 0, 0, STT_NOTYPE, 1, 0 };
		if (!have_a) { if (nsym >= MAXSYM) die("too many symbols"); syms[nsym++] = (Sym){ "$a", sec, padmap[i].a_at, 0, 0, STT_NOTYPE, 1, 0 }; }
		cursec = save;
	}
}
static void map_data(void) { map_to(MAP_DATA, 1); }
static void map_frag_data(void) { map_to(MAP_DATA, 0); }
static void map_data_only_code_sections(void) {   /* GAS: a code section holding only data still gets $d at 0 */
	for (int i = 0; i < nsec; i++)
		if (mapstate[i] == MAP_UNDEF && (secs[i].flags & SHF_EXECINSTR) && secs[i].len > 0 && secs[i].type != SHT_NOBITS) {
			int save = cursec; cursec = i; add_mapsym(MAP_DATA, 0); mapstate[i] = MAP_DATA; cursec = save; }
}
static void drop_end_mapsyms(void) {
	for (int i = 0; i < nsym; i++)
		if (syms[i].name && syms[i].name[0] == '$' && (syms[i].name[1] == 'a' || syms[i].name[1] == 'd') && !syms[i].name[2]
		    && syms[i].sec >= 0 && syms[i].value == (u32)secs[syms[i].sec].len) syms[i].name = NULL;
}
int section_symbol(int sec);
int sec_get(const char *name, u32 type, u32 flags) {
	int i = sec_find(name); if (i >= 0) { cursec = i; return i; }
	if (nsec >= MAXSEC) die("too many sections");
	secs[nsec] = (Section){ strdup(name), type, flags, NULL, 0, 0, 0, 1, 0 };
	cursec = nsec++; section_symbol(cursec);   /* GAS makes the section symbol when the section is created */
	return cursec;
}
void emit(const void *p, size_t n) {
	Section *s = &secs[cursec];
	if (s->len + n > s->cap) { s->cap = (s->len + n) * 2 + 64; s->data = realloc(s->data, s->cap); }
	memcpy(s->data + s->len, p, n); s->len += n;
}
void emit32(u32 w) { if (cursec < 0) die("instruction outside any section"); u8 b[4] = { w, w >> 8, w >> 16, w >> 24 }; emit(b, 4); }
void patch32(int sec, u32 off, u32 w) { u8 *d = secs[sec].data + off; d[0]=w; d[1]=w>>8; d[2]=w>>16; d[3]=w>>24; }
u32  read32(int sec, u32 off) { u8 *d = secs[sec].data + off; return d[0] | d[1]<<8 | d[2]<<16 | (u32)d[3]<<24; }
u32  here(void) { if (cursec < 0) die("instruction/label outside any section"); return secs[cursec].len; }

int sym_find(const char *name) { for (int i = 0; i < nsym; i++) if (syms[i].name && !strcmp(syms[i].name, name)) return i; return -1; }
int sym_intern(const char *name) {
	int i = sym_find(name); if (i >= 0) return i;
	if (nsym >= MAXSYM) die("too many symbols");
	syms[nsym] = (Sym){ strdup(name), 0, 0, 0, 0, STT_NOTYPE, 0, 0 };   /* global=0 (local until .global'd) */
	return nsym++;
}
void add_reloc(int sec, u32 off, int symidx, u32 type) { if (nrel >= MAXREL) die("too many relocations"); rels[nrel++] = (Reloc){ sec, off, symidx, type }; }
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
#define MAXTOK 32   /* mnemonic + operands; register lists (push/pop) can be long */
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
		int inq = 0, isimm = (*p == '#' || *p == ':'), depth = 0;   /* `#expr` and `:pc_g0:(expr)` group-reloc operands */   /* inside "..." spaces/commas are part of the token (.ascii "a b") */
		while (*p && (inq || (*p != '\t' && *p != ',' && *p != '\n' && *p != ' ') || (isimm && depth > 0 && *p != '\n')
		              || (isimm && (*p == ' ' || *p == '\t') && !imm_ends_here(p)))) {
			/* a `#` immediate is an EXPRESSION: it keeps its spaces up to the next top-level comma
			 * (`#(. - bar - 8) & 0xff`) — was split at the first space and silently mis-parsed */
			if (*p == '\\' && p[1]) { p += 2; continue; }   /* skip an escaped char (incl. \") */
			if (*p == '"') inq = !inq;
			if (!inq && *p == '(') depth++; else if (!inq && *p == ')' && depth) depth--;
			p++;
		}
		if (isimm) { char *e = p; while (e > toks[ntok - 1] && (e[-1] == ' ' || e[-1] == '\t')) *--e = 0; }   /* trim */
		if (*p) *p++ = 0;
	}
}

/* ------------------------------------------------------------------ labels + directives ----------- */
static void def_label(const char *name) {
	if (cursec < 0) die("label '%s' outside any section", name);
	{ const char *q = name; while (isdigit((unsigned char)*q)) q++;
	  if (q != name && !*q) { long v = strtol(name, NULL, 10); if (v > 0x7fffffff) die("local label %s too large", name); fb_define((int)v, (u32)secs[cursec].len); return; } }
	int i = sym_intern(name);
	if (syms[i].defined) die("symbol '%s' is already defined", name);   /* GAS: a label may be defined once (.set may redefine) */
	syms[i].sec = cursec; syms[i].value = secs[cursec].len; syms[i].defined = 1;
}

/* Emit the bytes of a C-string token like "\"Unknown error\000\"" (quotes included), decoding escapes
 * (\ooo octal, \xHH.. hex, \n \t \r \b \f \\ \" \0). add_nul appends a terminating NUL (.asciz), else not (.ascii). */
static struct { int sym; char *target; } alias_of[4096]; static int nalias;   /* pending `.set name, target` */
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
	if      (!strncmp(nm, ".text",   5)) flags = SHF_ALLOC | SHF_EXECINSTR;
	else if (!strncmp(nm, ".rodata", 7)) flags = SHF_ALLOC;
	else if (!strncmp(nm, ".data",   5)) flags = SHF_ALLOC | SHF_WRITE;
	else if (!strncmp(nm, ".bss",    4)) { flags = SHF_ALLOC | SHF_WRITE; type = SHT_NOBITS; }
	else if (!strncmp(nm, ".tbss", 5)) { flags = SHF_ALLOC | SHF_WRITE | 0x400; type = SHT_NOBITS; }   /* SHF_TLS */
	else if (!strncmp(nm, ".tdata", 6)) flags = SHF_ALLOC | SHF_WRITE | 0x400;
	else if (!strncmp(nm, ".init_array", 11)) { flags = SHF_ALLOC | SHF_WRITE; type = 14; }
	else if (!strncmp(nm, ".fini_array", 11)) { flags = SHF_ALLOC | SHF_WRITE; type = 15; }
	else if (!strncmp(nm, ".note", 5)) type = 7;
	u32 entsize = 0;
	if (ntok >= 3 && toks[2][0] == '"') { flags = 0;   /* explicit "flags" overrides: GAS letters */
		for (const char *f = toks[2] + 1; *f && *f != '"'; f++) switch (*f) {
		case 'a': flags |= SHF_ALLOC; break;
		case 'w': flags |= SHF_WRITE; break;
		case 'x': flags |= SHF_EXECINSTR; break;
		case 'M': flags |= 0x10; break;    /* SHF_MERGE */
		case 'S': flags |= 0x20; break;    /* SHF_STRINGS */
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
		if (flags & 0x10) { if (ntok < 5) die(".section %s: M needs an entity size", nm); entsize = (u32)eval_const_expr(toks[4]); }
	}
	int explicit = ntok >= 3 && toks[2][0] == '"', old = sec_find(nm);
	if (explicit && old >= 0 && secs[old].flags != flags) die(".section %s: changed section attributes", nm);   /* GAS: an error, not a silent merge */
	int si = sec_get(nm, type, flags);
	if (entsize) secs[si].entsize = entsize;
}

static const char *cur_stmt;   /* raw text of the current statement (labels peeled) — for expression operands */

/* ---- data-directive expressions: `.word/.long e1, e2, ...` ---------------------------------------
 * A relocatable value is  c + addr(sym) - dot*P  (sym<0: none; P = the address of the word being emitted).
 * Enough for everything kernel asm puts in data: constants with ( ) and C operators (BUG's
 * `((0xe7f001f2) & 0xFFFFFFFF)`), `sym+k`, numeric local labels `1b` (in ANY section — they become the
 * label's section symbol + its offset), and PC-relative `9998b - .` (alternatives tables). */
typedef struct { long c; int sym, dot, msym; u32 rtype; } RVal;   /* c + sym - msym - dot*P; rtype: explicit reloc from `sym(OP)` */
static const char *ep;
static void ews(void) { while (*ep == ' ' || *ep == '\t') ep++; }
static RVal e_or(void);
static RVal rconst(long c) { RVal r = { c, -1, 0, -1, 0 }; return r; }
static void need_const(RVal a, const char *op) { if (a.sym >= 0 || a.dot || a.msym >= 0 || a.rtype) die("data expr: operator '%s' needs constant operands", op); }
static RVal e_prim(void) {
	ews();
	if (*ep == '(') { ep++; RVal r = e_or(); ews(); if (*ep != ')') die("data expr: expected ')' in '%s'", cur_stmt); ep++; return r; }
	if (*ep == '-') { ep++; RVal r = e_prim(); if (r.sym >= 0 || r.dot) die("data expr: can't negate a relocatable"); r.c = -r.c; return r; }
	if (*ep == '~') { ep++; RVal r = e_prim(); need_const(r, "~"); r.c = ~r.c; return r; }
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
static RVal e_mul(void) {
	RVal a = e_prim();
	for (;;) { ews();
		if (*ep == '*') { ep++; RVal b = e_prim(); need_const(a, "*"); need_const(b, "*"); a.c *= b.c; }
		else if (*ep == '/') { ep++; RVal b = e_prim(); need_const(a, "/"); need_const(b, "/"); if (!b.c) die("data expr: divide by 0"); a.c /= b.c; }
		else if (*ep == '%') { ep++; RVal b = e_prim(); need_const(a, "%"); need_const(b, "%"); if (!b.c) die("data expr: modulo by 0"); a.c %= b.c; }
		else return a; }
}
static RVal e_add(void) {
	RVal a = e_mul();
	for (;;) { ews();
		if (*ep == '+') { ep++; RVal b = e_mul(); if (a.sym >= 0 && b.sym >= 0) die("data expr: sym + sym");
			if (a.msym >= 0 && b.msym >= 0) die("data expr: too many symbol differences");
			a.c += b.c; if (b.sym >= 0) { a.sym = b.sym; a.rtype = b.rtype; } if (b.msym >= 0) a.msym = b.msym; a.dot += b.dot; }
		else if (*ep == '-') { ep++; RVal b = e_mul(); a.c -= b.c; a.dot -= b.dot;
			if (b.sym < 0 && a.sym >= 0 && a.dot == -1 && syms[a.sym].defined && syms[a.sym].sec == cursec) {   /* sym - . (same section): constant */
				a.c += (long)syms[a.sym].value - (long)secs[cursec].len; a.sym = -1; a.dot = 0; continue; }
			if (b.sym >= 0) {   /* sym - sym: fine when both resolve into the same section */
				if (a.sym < 0 && a.dot == 1 && syms[b.sym].defined && syms[b.sym].sec == cursec) {   /* (. + k) - sym, same section */
					a.c += (long)secs[cursec].len - (long)syms[b.sym].value; a.dot = 0; continue; }
				if (a.sym < 0) die("data expr: const - sym");
				if (b.msym >= 0) die("data expr: subtracting a symbol difference");
				Sym *x = &syms[a.sym], *y = &syms[b.sym];
				if (x->defined && y->defined && x->sec == y->sec) { a.c += (long)x->value - (long)y->value; a.sym = -1; }
				else if (a.msym < 0) a.msym = b.sym;   /* not resolvable yet (forward, e.g. `1f - 1b`) or cross-section: defer */
				else die("data expr: too many symbol differences");
			} }
		else return a; }
}
static RVal e_shift(void) {
	RVal a = e_add();
	for (;;) { ews();
		if (ep[0] == '<' && ep[1] == '<') { ep += 2; RVal b = e_add(); need_const(a, "<<"); need_const(b, "<<"); a.c <<= b.c; }
		else if (ep[0] == '>' && ep[1] == '>') { ep += 2; RVal b = e_add(); need_const(a, ">>"); need_const(b, ">>"); a.c = (long)((unsigned long)a.c >> b.c); }
		else return a; }
}
static RVal e_and(void) { RVal a = e_shift(); for (;;) { ews(); if (*ep == '&' && ep[1] != '&') { ep++; RVal b = e_shift(); need_const(a, "&"); need_const(b, "&"); a.c &= b.c; } else return a; } }
static RVal e_xor(void) { RVal a = e_and(); for (;;) { ews(); if (*ep == '^') { ep++; RVal b = e_and(); need_const(a, "^"); need_const(b, "^"); a.c ^= b.c; } else return a; } }
static RVal e_or(void)  { RVal a = e_xor(); for (;;) { ews(); if (*ep == '|' && ep[1] != '|') { ep++; RVal b = e_xor(); need_const(a, "|"); need_const(b, "|"); a.c |= b.c; } else return a; } }

/* Fold a pending `sym - msym` if both are now defined in one section. */
static RVal fold_msym(RVal r) {
	if (r.msym >= 0 && r.sym >= 0 && syms[r.sym].defined && syms[r.msym].defined && syms[r.sym].sec == syms[r.msym].sec) {
		r.c += (long)syms[r.sym].value - (long)syms[r.msym].value; r.sym = -1; r.msym = -1; }
	return r;
}
/* Data words whose value depends on symbols not yet defined (`.word 1f - 1b`): resolved after parsing. */
static struct { int sec; u32 off; RVal r; } dword[16384]; static int ndword;
static struct { int sym; RVal r; } dsize[4096]; static int ndsize;
static void resolve_deferred(void) {
	for (int i = 0; i < ndword; i++) {
		RVal r = fold_msym(dword[i].r); int sec = dword[i].sec; u32 off = dword[i].off;
		if (r.sym < 0 && r.msym < 0) { patch32(sec, off, (u32)r.c); continue; }
		if (r.msym >= 0 && syms[r.msym].defined && syms[r.msym].sec == sec && r.sym >= 0 && !r.dot) {
			/* S - M with M in this word's section: = S - P + (P - M) -> R_ARM_REL32, addend c + (P - M) */
			u32 got = md_data_reloc_for(syms[r.sym].name, 0);   /* _GLOBAL_OFFSET_TABLE_ - L: R_ARM_BASE_PREL (GAS), same addend */
			patch32(sec, off, (u32)(r.c + (long)off - (long)syms[r.msym].value)); add_reloc(sec, off, r.sym, got ? got : md_r_rel32); continue; }
		die("data expression: symbol difference across sections can't be represented");
	}
	for (int i = 0; i < ndsize; i++) {
		RVal r = fold_msym(dsize[i].r);
		if (r.sym >= 0 || r.msym >= 0 || r.dot) die(".size of '%s': not a constant", syms[dsize[i].sym].name);
		syms[dsize[i].sym].size = (u32)r.c;
	}
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
		if (ndword >= 16384) die("too many deferred data expressions");
		dword[ndword].sec = cursec; dword[ndword].off = off; dword[ndword].r = r; ndword++; emit32(0); return; }
	if (r.dot != 0 && r.dot != -1) die("data expr: '.' may only appear as '- .'");
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
		RVal r = e_or();
		if (r.sym >= 0 || r.dot) die("%s: operand must be a constant", toks[0]);
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
	for (;;) { RVal r = e_or(); ews();
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
	} else if (!strcmp(d, ".text")) { sec_get(".text", SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR); prevsec = was;
	} else if (!strcmp(d, ".data")) { sec_get(".data", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE); prevsec = was;
	} else if (!strcmp(d, ".global") || !strcmp(d, ".globl")) { syms[sym_intern(toks[1])].global = 1;
	} else if (!strcmp(d, ".weak")) { syms[sym_intern(toks[1])].weak = 1;   /* STB_WEAK binding (kernel COND_SYSCALL) */
	} else if (!strcmp(d, ".type")) { int i = sym_intern(toks[1]);
		if (toks[2] && strstr(toks[2], "function")) syms[i].type = STT_FUNC;
		else if (toks[2] && strstr(toks[2], "object")) syms[i].type = STT_OBJECT;
	} else if (!strcmp(d, ".size")) {
		/* .size <sym>, <expr> — any expression (was: only `. - label`; anything else was silently ignored) */
		int i = sym_intern(toks[1]);
		const char *q = strchr(cur_stmt, ','); if (!q) die(".size: expected 'sym, expr'");
		const char *save = ep; ep = q + 1; RVal r = e_or(); ews(); if (*ep) die(".size: junk '%s'", ep); ep = save;
		if (r.dot == 1 && r.msym >= 0 && r.sym < 0 && syms[r.msym].defined && syms[r.msym].sec == cursec) {   /* . - sym */
			r.c += (long)secs[cursec].len - (long)syms[r.msym].value; r.dot = 0; r.msym = -1; }
		r = fold_msym(r);
		if (r.sym < 0 && r.msym < 0 && !r.dot) syms[i].size = (u32)r.c;
		else { if (ndsize >= 4096) die("too many deferred .size"); dsize[ndsize].sym = i; dsize[ndsize].r = r; ndsize++; }
	} else if (!strcmp(d, ".align") || !strcmp(d, ".p2align") || !strcmp(d, ".balign")) {
		if (cursec < 0) return;
		long a = ntok >= 2 ? eval_const_expr(toks[1]) : 2;
		if (strcmp(d, ".balign") ? (a < 0 || a > 16) : (a < 0 || a > 65536 || (a & (a - 1)))) die("%s: bad alignment '%s'", d, toks[1]);
		u32 bytes = (!strcmp(d, ".balign")) ? (u32)a : (1u << a);
		sec_align_at_least(bytes ? bytes : 1);
		u32 pad = bytes ? (bytes - (u32)(secs[cursec].len % bytes)) % bytes : 0;
		if (secs[cursec].type == SHT_NOBITS) { map_frag_data(); secs[cursec].len += pad; return; }
		if (secs[cursec].flags & SHF_EXECINSTR) {   /* code: zero bytes to a word boundary ($d), then NOPs ($a) */
			u32 z = pad & 3;
			if (z) {   /* sub-word zero fill: its $d + the NOPs' $a are written after parsing (GAS arm_handle_align) */
				if (npadmap >= 4096) die("too many code alignments");
				padmap[npadmap].sec = cursec; padmap[npadmap].d_at = (u32)secs[cursec].len; padmap[npadmap].a_at = (u32)secs[cursec].len + z; npadmap++;
				map_insn();   /* rs_align_code: ARM state at parse time (may add $d@0 + $a here) */
				for (u32 k = 0; k < z; k++) { u8 zb = 0; emit(&zb, 1); }
			} else map_insn();
			for (u32 k = z; k < pad; k += 4) emit32(0xe320f000u);   /* ARMv6K+ nop (GAS with -march=armv7-a) */
		} else { map_frag_data(); for (u32 k = 0; k < pad; k++) { u8 zb = 0; emit(&zb, 1); } }
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
		if (ntok >= 3 && !strcmp(toks[2], ".")) {
			long addend = 0; if (ntok >= 5 && !strcmp(toks[3], "+")) addend = strtol(toks[4], NULL, 0);
			syms[i].sec = cursec; syms[i].value = secs[cursec].len + (u32)addend; syms[i].defined = 1;
		} else if (ntok >= 3 && !(isalpha((unsigned char)toks[2][0]) || toks[2][0] == '_' || toks[2][0] == '.')) {   /* `.equ N, 16` / `.set N, 4*4`: absolute */
			const char *q = strchr(cur_stmt, ','); if (!q) die(".set: expected 'name, value'");
			syms[i].sec = SEC_ABS; syms[i].value = (u32)eval_const_expr(q + 1); syms[i].defined = 1;
		} else if (ntok >= 3) {   /* alias: copy the target's location/type — at the END (GAS: the target may come later) */
			if (nalias >= (int)(sizeof alias_of / sizeof *alias_of)) die(".set: too many aliases");
			alias_of[nalias].sym = i; alias_of[nalias].target = strdup(toks[2]); nalias++;
		} else die(".set: expected 'name, . [+ N]' or 'name, target'");
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
			alias_of[k].target = NULL;
		}
	}
	for (int k = 0; k < nalias; k++) if (alias_of[k].target) die(".set: alias target '%s' undefined", alias_of[k].target);
}

static void parse_line(char *line) {
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
	else { map_insn(); md_assemble(toks, ntok); }
}

static void check_fb_resolved(void) {   /* every referenced `Nf` must have been defined by the end */
	for (int i = 0; i < nsym; i++)
		if (syms[i].name && !strncmp(syms[i].name, ".Lfb", 4) && !syms[i].defined) {
			int n = atoi(syms[i].name + 4); die("unresolved forward local label %df", n); }
}

/* Find-or-create the STT_SECTION symbol for section `sec` — a local, value-0 marker for the section
 * itself, the reference a reduced relocation points at (see reduce_local_relocs). */
int section_symbol(int sec) {
	for (int i = 0; i < nsym; i++)
		if (syms[i].type == STT_SECTION && syms[i].defined && syms[i].sec == sec) return i;
	if (nsym >= MAXSYM) die("too many symbols");
	syms[nsym] = (Sym){ secs[sec].name, sec, 0, 0, 0, STT_SECTION, 1, 0 };   /* local, value 0, defined here */
	return nsym++;
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
		int local_def = s->defined && !s->global && !s->weak && s->type != STT_SECTION && s->sec >= 0;
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
		if (!strcmp(w,".macro")||!strcmp(w,".rept")||!strcmp(w,".irp")||!strcmp(w,".irpc")) coll_depth++;
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
					if (*s == '=') { s++; const char *v = s; while (*s && *s != ',' && *s != ' ' && *s != '\t') s++; m->defs[k] = strndup(v, (size_t)(s - v)); }
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
					while (*q) {
						const char *s0 = q; while (*q && *q != ',') q++;
						const char *e = q; while (e > s0 && (e[-1] == ' ' || e[-1] == '\t')) e--;
						char *val = strndup(s0, (size_t)(e - s0)); char *v[1] = { val };
						for (int i=0;i<bn;i++) { char *o = subst(bd[i], var, v, 1, macuid); feed_line(o); free(o); }
						free(val); if (*q == ',') { q++; while (*q == ' ' || *q == '\t') q++; }
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
	if (!strcmp(w, ".rept"))  { coll_reptn = emitting()?eval_if(rest):0; if (coll_reptn < 0) die(".rept: negative count"); coll_mode=2; coll_depth=1; coll_n=0; return; }
	if (!strcmp(w, ".irp") || !strcmp(w, ".irpc")) {   /* .irp var, v1, v2 ... / .irpc var, chars */
		const char *r = rest; while (*r == ' ' || *r == '\t') r++;
		char nm[LEADMAX]; const char *a = lead(r, nm); if (!nm[0]) die("%s: missing variable", w);
		while (*a == ' ' || *a == '\t' || *a == ',') a++;
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
	if (!strcmp(w, ".ifndef")) { char nm[LEADMAX]; lead(rest,nm); int on = emitting() && !(sym_find(nm)>=0 && syms[sym_find(nm)].defined); if (nifs >= 64) die("too many nested .if (>64)"); ifs[nifs].active=on; ifs[nifs].taken=on; nifs++; return; }
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
	if (!strcmp(w, ".else"))   { if (nifs) { int parent=1; for(int i=0;i<nifs-1;i++) if(!ifs[i].active)parent=0; ifs[nifs-1].active = parent && !ifs[nifs-1].taken; if(ifs[nifs-1].active) ifs[nifs-1].taken=1; } return; }
	if (!strcmp(w, ".endif"))  { if (nifs) nifs--; return; }
	if (!strcmp(w, ".err"))    { if (emitting()) die(".err directive reached (assembly assertion failed)"); return; }
	if (!emitting() || exitm) return;                /* inside a false .if branch (or after .exitm) */
	Macro *m = macro_find(w);
	if (m) { expand_macro(m, rest); return; }
	parse_line(line);
}

/* ------------------------------------------------------------------ driver ------------------------ */
int main(int argc, char **argv) {
	const char *out = "a.out", *in = NULL;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
		else if (argv[i][0] != '-') in = argv[i];
		else die("unknown option '%s'", argv[i]);
	}
	if (!in) die("usage: as [-o out.o] in.s");

	FILE *f = fopen(in, "rb"); if (!f) die("cannot open %s", in);
	fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
	char *buf = malloc(sz + 1); if (fread(buf, 1, sz, f) != (size_t)sz) die("read failed"); buf[sz] = 0; fclose(f);

	/* GAS starts every object with .text/.data/.bss (in that order) and assembles into .text by default. */
	sec_get(".text", SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR); sec_get(".data", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE);
	sec_get(".bss", SHT_NOBITS, SHF_ALLOC | SHF_WRITE); cursec = sec_find(".text"); prevsec = cursec;
	strip_comments(buf);
	char *line = buf, *nl;
	do { nl = strchr(line, '\n'); if (nl) *nl = 0; feed_line(line); line = nl ? nl + 1 : NULL; } while (line);
	if (coll_mode) die("unterminated .macro/.rept");
	if (nifs) die("unterminated .if");

	md_flush_pools();       /* GAS dumps pending literal pools at the end of parsing (before fixups/write) */
	check_fb_resolved();
	resolve_aliases();      /* .set name, target — targets may be defined after the .set */
	resolve_deferred();     /* data words / .size that referenced not-yet-defined labels */
	md_finish();            /* let the arch backend resolve its own end-of-pass fixups (ldr literals) */
	reduce_local_relocs();  /* fold local-symbol relocs to section-symbol + in-place value (GNU parity) */
	map_data_only_code_sections();
	flush_padmaps();
	md_emit_attributes();   /* GAS writes the build-attribute section last */
	drop_end_mapsyms();   /* a mapping symbol at a section's end marks nothing */
	obj_write(out);
	return 0;
}
