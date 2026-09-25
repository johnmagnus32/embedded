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
Fixup fixes[MAXFIX]; int nfix;
/* Numeric local labels (GAS: ANY decimal number, e.g. the kernel's `9998:`), each remembering its latest
 * definition's value + section. Forward refs are bound when the next definition appears (see local_define). */
#define MAXLOCAL 4096
static struct { int num; u32 value; int sec; } locals_tab[MAXLOCAL]; static int nlocals_tab;
static int local_find(int n) { for (int i = 0; i < nlocals_tab; i++) if (locals_tab[i].num == n) return i; return -1; }

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
void map_insn(void) { map_to(MAP_ARM, 0); }
/* GAS writes code-alignment padding (and its $d/$a pair) after parsing, so those symbols come LAST in the table. */
static struct { int sec; u32 d_at, a_at; } padmap[4096]; static int npadmap;
static void flush_padmaps(void) {
	for (int i = 0; i < npadmap; i++) { int save = cursec; cursec = padmap[i].sec; add_mapsym(MAP_DATA, padmap[i].d_at); add_mapsym(MAP_ARM, padmap[i].a_at); cursec = save; }
}
static void map_data(void) { map_to(MAP_DATA, 1); }
static void map_frag_data(void) { map_to(MAP_DATA, 0); }
static void drop_end_mapsyms(void) {
	for (int i = 0; i < nsym; i++)
		if (syms[i].name && syms[i].name[0] == '$' && (syms[i].name[1] == 'a' || syms[i].name[1] == 'd') && !syms[i].name[2]
		    && syms[i].sec >= 0 && syms[i].value == (u32)secs[syms[i].sec].len) syms[i].name = NULL;
}
int section_symbol(int sec);
int sec_get(const char *name, u32 type, u32 flags) {
	int i = sec_find(name); if (i >= 0) { cursec = i; return i; }
	if (nsec >= MAXSEC) die("too many sections");
	secs[nsec] = (Section){ strdup(name), type, flags, NULL, 0, 0, 0 };
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
void add_fixup(int sec, u32 off, int local_num) { if (nfix >= MAXFIX) die("too many fixups"); fixes[nfix++] = (Fixup){ sec, off, local_num, 0, 0, 0, 0 }; }
void add_fixup_kind(int sec, u32 off, int local_num, int kind) { if (nfix >= MAXFIX) die("too many fixups"); fixes[nfix++] = (Fixup){ sec, off, local_num, kind, 0, 0, 0 }; }
void local_define(int n, u32 value) {
	int i = local_find(n);
	if (i < 0) { if (nlocals_tab >= MAXLOCAL) die("too many distinct numeric local labels (>%d)", MAXLOCAL); i = nlocals_tab++; locals_tab[i].num = n; }
	locals_tab[i].value = value; locals_tab[i].sec = cursec;
	for (int f = 0; f < nfix; f++)   /* bind every still-pending `Nf` to THIS (the next) definition */
		if (!fixes[f].done && fixes[f].local_num == n) { fixes[f].target = value; fixes[f].tsec = cursec; fixes[f].done = 1; }
}
int  local_defined(int n) { return local_find(n) >= 0; }
u32  local_value(int n) { int i = local_find(n); return i < 0 ? 0 : locals_tab[i].value; }
int  local_sec(int n) { int i = local_find(n); return i < 0 ? -1 : locals_tab[i].sec; }
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
		if (p[0] == '@' || (p[0] == '/' && p[1] == '/')) { while (*p && *p != '\n') *p++ = ' '; if (!*p) break; }
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
		int inq = 0, isimm = (*p == '#'), depth = 0;   /* inside "..." spaces/commas are part of the token (.ascii "a b") */
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
	  if (q != name && !*q) { long v = strtol(name, NULL, 10); if (v > 0x7fffffff) die("local label %s too large", name); local_define((int)v, secs[cursec].len); return; } }
	int i = sym_intern(name);
	if (syms[i].defined) die("symbol '%s' is already defined", name);   /* GAS: a label may be defined once (.set may redefine) */
	syms[i].sec = cursec; syms[i].value = secs[cursec].len; syms[i].defined = 1;
}

/* Emit the bytes of a C-string token like "\"Unknown error\000\"" (quotes included), decoding escapes
 * (\ooo octal, \n \t \r \b \f \\ \" \0). add_nul appends a terminating NUL (.asciz), else not (.ascii). */
static void emit_string(const char *tok, int add_nul) {
	const char *p = tok; if (*p == '"') p++;
	while (*p && *p != '"') {
		u8 b;
		if (*p == '\\') {
			p++;
			if (*p >= '0' && *p <= '7') { int v = 0, n = 0; while (*p >= '0' && *p <= '7' && n < 3) { v = v*8 + (*p++ - '0'); n++; } b = (u8)v; }
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
	if (ntok >= 3 && toks[2][0] == '"') { const char *f = toks[2]; flags = 0;   /* explicit "flags" overrides */
		if (strchr(f, 'a')) flags |= SHF_ALLOC;
		if (strchr(f, 'x')) flags |= SHF_EXECINSTR;
		if (strchr(f, 'w')) flags |= SHF_WRITE; }
	sec_get(nm, type, flags);
}

static const char *cur_stmt;   /* raw text of the current statement (labels peeled) — for expression operands */

/* ---- data-directive expressions: `.word/.long e1, e2, ...` ---------------------------------------
 * A relocatable value is  c + addr(sym) - dot*P  (sym<0: none; P = the address of the word being emitted).
 * Enough for everything kernel asm puts in data: constants with ( ) and C operators (BUG's
 * `((0xe7f001f2) & 0xFFFFFFFF)`), `sym+k`, numeric local labels `1b` (in ANY section — they become the
 * label's section symbol + its offset), and PC-relative `9998b - .` (alternatives tables). */
typedef struct { long c; int sym, dot; } RVal;
static const char *ep;
static void ews(void) { while (*ep == ' ' || *ep == '\t') ep++; }
static RVal e_or(void);
static RVal rconst(long c) { RVal r = { c, -1, 0 }; return r; }
static void need_const(RVal a, const char *op) { if (a.sym >= 0 || a.dot) die("data expr: operator '%s' needs constant operands", op); }
static RVal e_prim(void) {
	ews();
	if (*ep == '(') { ep++; RVal r = e_or(); ews(); if (*ep != ')') die("data expr: expected ')' in '%s'", cur_stmt); ep++; return r; }
	if (*ep == '-') { ep++; RVal r = e_prim(); if (r.sym >= 0 || r.dot) die("data expr: can't negate a relocatable"); r.c = -r.c; return r; }
	if (*ep == '~') { ep++; RVal r = e_prim(); need_const(r, "~"); r.c = ~r.c; return r; }
	if (*ep == '+') { ep++; return e_prim(); }
	{ int n; char dir; int k = parse_local_ref(ep, &n, &dir); if (k) { ep += k;
		if (dir == 'f') die("data expr: forward local label %df in a data directive is unsupported", n);
		if (!local_defined(n)) die("data expr: undefined local label %db", n);
		RVal r = { (long)local_value(n), section_symbol(local_sec(n)), 0 }; return r; } }
	if (*ep == '\'' && ep[1]) { long c = (unsigned char)ep[1]; ep += 2; if (*ep == '\'') ep++; return rconst(c); }   /* GAS char constant 'c */
	if (isdigit((unsigned char)*ep)) { char *e; unsigned long long v = strtoull(ep, &e, 0); ep = e; while (*ep == 'u' || *ep == 'U' || *ep == 'l' || *ep == 'L') ep++; return rconst((long)v); }
	if (*ep == '.' && !(isalnum((unsigned char)ep[1]) || ep[1] == '_' || ep[1] == '.' || ep[1] == '$')) { ep++; RVal r = { 0, -1, 1 }; return r; }
	if (isalpha((unsigned char)*ep) || *ep == '_' || *ep == '.' || *ep == '$') {
		char nm[128]; int k = 0;
		while ((isalnum((unsigned char)*ep) || *ep == '_' || *ep == '.' || *ep == '$') && k < 127) nm[k++] = *ep++;
		nm[k] = 0; int si = sym_intern(nm);
		if (syms[si].defined && syms[si].sec == SEC_ABS) return rconst((long)(int32_t)syms[si].value);   /* `.equ N, 16` */
		RVal r = { 0, si, 0 }; return r;
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
		if (*ep == '+') { ep++; RVal b = e_mul(); if (a.sym >= 0 && b.sym >= 0) die("data expr: sym + sym"); a.c += b.c; if (b.sym >= 0) a.sym = b.sym; a.dot += b.dot; }
		else if (*ep == '-') { ep++; RVal b = e_mul(); a.c -= b.c; a.dot -= b.dot;
			if (b.sym < 0 && a.sym >= 0 && a.dot == -1 && syms[a.sym].defined && syms[a.sym].sec == cursec) {   /* sym - . (same section): constant */
				a.c += (long)syms[a.sym].value - (long)secs[cursec].len; a.sym = -1; a.dot = 0; continue; }
			if (b.sym >= 0) {   /* sym - sym: fine when both resolve into the same section */
				if (a.sym < 0 && a.dot == 1 && syms[b.sym].defined && syms[b.sym].sec == cursec) {   /* (. + k) - sym, same section */
					a.c += (long)secs[cursec].len - (long)syms[b.sym].value; a.dot = 0; continue; }
				if (a.sym < 0) die("data expr: const - sym");
				Sym *x = &syms[a.sym], *y = &syms[b.sym];
				if (!x->defined || !y->defined || x->sec != y->sec) die("data expr: difference of symbols in different sections");
				a.c += (long)x->value - (long)y->value; a.sym = -1;
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

/* A constant expression (instruction immediates, .equ): `.` is the current location, a same-section
 * `sym - .` folds, absolute symbols are constants. Anything relocatable or trailing junk is an error (was:
 * strtol, which silently turned `#(. - bar - 8)` into 0). */
long eval_const_expr(const char *s) {
	const char *save = ep; ep = s; RVal r = e_or(); ews();
	if (*ep) die("expression: junk '%s' in '%s'", ep, s);
	ep = save;
	if (r.sym >= 0 && r.dot == -1 && syms[r.sym].defined && syms[r.sym].sec == cursec) return r.c + (long)syms[r.sym].value - (long)secs[cursec].len;
	if (r.sym < 0 && r.dot == 1) return r.c + (long)secs[cursec].len;
	if (r.sym >= 0 || r.dot) die("expression '%s' is not a constant", s);
	return r.c;
}
/* Emit one 32-bit data word for a relocatable value placed at the current location. */
static void emit_word_rval(RVal r) {
	u32 off = secs[cursec].len;
	if (r.dot != 0 && r.dot != -1) die("data expr: '.' may only appear as '- .'");
	if (r.sym >= 0 && syms[r.sym].defined && syms[r.sym].sec == cursec && r.dot == -1) {   /* X - . in the same section: a constant */
		emit32((u32)(r.c + (long)syms[r.sym].value - (long)off)); return;
	}
	if (r.sym < 0) { if (r.dot) die("data expr: '- .' with no symbol"); emit32((u32)r.c); return; }
	emit32((u32)r.c);   /* REL-style: the addend lives in place */
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
	for (;;) { RVal r = e_or(); emit_word_rval(r); ews(); if (*ep == ',') { ep++; continue; } if (*ep) die("data expr: junk '%s' in '%s'", ep, cur_stmt); break; }
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
		/* .size <sym>, . - <label>  — the one expression form our startup asm needs. */
		int i = sym_intern(toks[1]);
		if (ntok >= 5 && !strcmp(toks[2], ".") && !strcmp(toks[3], "-")) {
			const char *l = toks[4]; u32 base;
			int ln; char ld; if (parse_local_ref(l, &ln, &ld) == (int)strlen(l) && ld == 'b') { if (!local_defined(ln)) die(".size: undefined %db", ln); base = local_value(ln); }
			else { int j = sym_find(l); if (j < 0 || !syms[j].defined) die(".size: undefined '%s'", l); base = syms[j].value; }
			syms[i].size = secs[cursec].len - base;
		}
	} else if (!strcmp(d, ".align") || !strcmp(d, ".p2align") || !strcmp(d, ".balign")) {
		if (cursec < 0) return;
		long a = ntok >= 2 ? eval_const_expr(toks[1]) : 2;
		if (strcmp(d, ".balign") ? (a < 0 || a > 16) : (a < 0 || a > 65536 || (a & (a - 1)))) die("%s: bad alignment '%s'", d, toks[1]);
		u32 bytes = (!strcmp(d, ".balign")) ? (u32)a : (1u << a);
		u32 pad = bytes ? (bytes - (u32)(secs[cursec].len % bytes)) % bytes : 0;
		if (secs[cursec].type == SHT_NOBITS) { map_frag_data(); secs[cursec].len += pad; return; }
		if (secs[cursec].flags & SHF_EXECINSTR) {   /* code: zero bytes to a word boundary ($d), then NOPs ($a) */
			u32 z = pad & 3;
			if (z) {   /* sub-word zero fill: its $d + the NOPs' $a are written after parsing (GAS arm_handle_align) */
				if (npadmap >= 4096) die("too many code alignments");
				padmap[npadmap].sec = cursec; padmap[npadmap].d_at = (u32)secs[cursec].len; padmap[npadmap].a_at = (u32)secs[cursec].len + z; npadmap++;
				for (u32 k = 0; k < z; k++) { u8 zb = 0; emit(&zb, 1); }
				mapstate[cursec] = MAP_ARM;
			} else map_insn();
			for (u32 k = z; k < pad; k += 4) emit32(0xe320f000u);   /* ARMv6K+ nop (GAS with -march=armv7-a) */
		} else { map_frag_data(); for (u32 k = 0; k < pad; k++) { u8 zb = 0; emit(&zb, 1); } }
	} else if (!strcmp(d, ".word") || !strcmp(d, ".4byte") || !strcmp(d, ".long") || !strcmp(d, ".inst")) {
		/* .word <number> emits the value; .word <symbol>[+addend] emits the addend in place + an
		 * absolute (R_ARM_ABS32) relocation the linker fills with the symbol's address. */
		if (!strcmp(d, ".inst")) map_insn(); else map_data();
		if (!strstr(cur_stmt, "(GOT)")) { data_words(); }   /* expressions, lists, local labels, `X - .` */
		else {
			/* `.word <sym>[+addend]` -> R_ARM_ABS32; `.word <sym>(GOT)` -> R_ARM_GOT_PREL (PIC: the
			 * linker fills it with the PC-relative offset to <sym>'s GOT slot). */
			char raw[160]; strncpy(raw, toks[1], sizeof raw - 1); raw[sizeof raw - 1] = 0;
			u32 rtype = md_r_abs32;
			char *got = strstr(raw, "(GOT)");
			if (got && got[5] == 0) { *got = 0; rtype = md_r_got_prel; }
			char name[128]; long addend = 0;
			char *plus = strpbrk(raw, "+-");
			if (plus) { addend = eval_const_expr(plus); size_t k = plus - raw; if (k >= sizeof name) k = sizeof name - 1; memcpy(name, raw, k); name[k] = 0; }
			else { strncpy(name, raw, sizeof name - 1); name[sizeof name - 1] = 0; }
			u32 off = secs[cursec].len; emit32((u32)addend);
			add_reloc(cursec, off, sym_intern(name), rtype);
		}
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
		} else if (ntok >= 3) {   /* alias: copy the target's location/type (target must be defined by now) */
			int j = sym_find(toks[2]);
			if (j < 0 || !syms[j].defined) die(".set: alias target '%s' undefined", toks[2]);
			syms[i].sec = syms[j].sec; syms[i].value = syms[j].value; syms[i].defined = 1;
			if (!syms[i].type) syms[i].type = syms[j].type;
		} else die(".set: expected 'name, . [+ N]' or 'name, target'");
	} else if (!md_directive(toks, ntok)) {
		die("unknown directive '%s'", d);
	}
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
	if (toks[0][0] == '.') do_directive();
	else { map_insn(); md_assemble(toks, ntok); }
}

static void resolve_fixups(void) {
	for (int i = 0; i < nfix; i++) {
		if (!fixes[i].done) die("unresolved forward local %df", fixes[i].local_num);
		if (fixes[i].tsec != fixes[i].sec) die("forward local %df crosses sections (unsupported)", fixes[i].local_num);
		md_apply_fix(&fixes[i]);
	}
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
	for (int i = 0; i < nrel; i++) {
		if (rels[i].type != md_r_abs32 && rels[i].type != md_r_rel32) continue;
		Sym *s = &syms[rels[i].symidx];
		if (!s->defined || s->global || s->type == STT_SECTION) continue;   /* only local, non-section defs */
		patch32(rels[i].sec, rels[i].off, read32(rels[i].sec, rels[i].off) + s->value);
		rels[i].symidx = section_symbol(s->sec);
	}
}

/* ------------------------------------------------------------------ GAS macros -------------------- */
/* A line-level preprocessing pass over the input: expand `.macro`/`.rept` and honour `.if`/`.else`, then
 * hand real lines to parse_line. Recursive (macros expand into feed_line), so nested macros/rept/if work. */
typedef struct { char name[64]; char params[16][32]; int nparams; char *body[2048]; int nbody; } Macro;
static Macro macros[256]; static int nmacros;
static Macro *macro_find(const char *n) { for (int i = 0; i < nmacros; i++) if (!strcmp(macros[i].name, n)) return &macros[i]; return NULL; }
static int macuid;                                   /* \@ — a unique id per macro expansion */
static struct { int active, taken; } ifs[64]; static int nifs;   /* .if stack */
static int emitting(void) { for (int i = 0; i < nifs; i++) if (!ifs[i].active) return 0; return 1; }
static char *xdup(const char *s) { char *p = malloc(strlen(s) + 1); strcpy(p, s); return p; }
/* Leading whitespace-delimited word of a line -> w; returns the pointer just past it. */
static const char *lead(const char *s, char *w) { while (*s==' '||*s=='\t') s++; int i=0; while (*s && *s!=' '&&*s!='\t'&&*s!=','&&i<63) w[i++]=*s++; w[i]=0; return s; }

/* Tiny constant-expression evaluator for .if / .rept (numbers + the usual C operators, precedence-climbing).
 * Identifiers that aren't numbers evaluate to 0 (macro args are substituted to numbers before we get here). */
static const char *ep;
static long ep_expr(int minp);
static long ep_primary(void) {
	while (*ep==' '||*ep=='\t') ep++;
	if (*ep=='(') { ep++; long v=ep_expr(0); while(*ep==' ')ep++; if(*ep==')')ep++; return v; }
	if (*ep=='!') { ep++; return !ep_primary(); }
	if (*ep=='-') { ep++; return -ep_primary(); }
	if (*ep=='~') { ep++; return ~ep_primary(); }
	if (*ep=='\'') { ep++; long c=(unsigned char)*ep; if(*ep=='\\'){ep++; c=*ep=='n'?'\n':*ep=='t'?'\t':*ep=='0'?0:(unsigned char)*ep;} ep++; if(*ep=='\'')ep++; return c; }
	if ((*ep>='0'&&*ep<='9')) { char *e; long v=strtol(ep,&e,0); ep=e; return v; }
	while (*ep && (*ep=='_'||(*ep>='a'&&*ep<='z')||(*ep>='A'&&*ep<='Z')||(*ep>='0'&&*ep<='9'))) ep++;   /* unknown ident -> 0 */
	return 0;
}
static int ep_op(int *prec, int *len) {   /* classify the operator at ep; returns an id, sets precedence+length */
	const char *o=ep; while(*o==' '||*o=='\t')o++; int adv=(int)(o-ep);
	struct { const char *s; int p; } t;
	if(!strncmp(o,"&&",2)){*prec=2;*len=adv+2;return 1;} if(!strncmp(o,"||",2)){*prec=1;*len=adv+2;return 2;}
	if(!strncmp(o,"==",2)){*prec=4;*len=adv+2;return 3;} if(!strncmp(o,"!=",2)){*prec=4;*len=adv+2;return 4;}
	if(!strncmp(o,"<=",2)){*prec=5;*len=adv+2;return 5;} if(!strncmp(o,">=",2)){*prec=5;*len=adv+2;return 6;}
	if(!strncmp(o,"<<",2)){*prec=6;*len=adv+2;return 9;} if(!strncmp(o,">>",2)){*prec=6;*len=adv+2;return 10;}
	if(*o=='<'){*prec=5;*len=adv+1;return 7;} if(*o=='>'){*prec=5;*len=adv+1;return 8;}
	if(*o=='+'){*prec=7;*len=adv+1;return 11;} if(*o=='-'){*prec=7;*len=adv+1;return 12;}
	if(*o=='*'){*prec=8;*len=adv+1;return 13;} if(*o=='/'){*prec=8;*len=adv+1;return 14;} if(*o=='%'){*prec=8;*len=adv+1;return 15;}
	if(*o=='&'){*prec=3;*len=adv+1;return 16;} if(*o=='|'){*prec=3;*len=adv+1;return 17;} if(*o=='^'){*prec=3;*len=adv+1;return 18;}
	(void)t; return 0;
}
static long ep_expr(int minp) {
	long l = ep_primary();
	for (;;) { int prec, len, op = ep_op(&prec, &len); if (!op || prec < minp) break; ep += len;
		long r = ep_expr(prec + 1);
		switch (op) { case 1:l=l&&r;break; case 2:l=l||r;break; case 3:l=l==r;break; case 4:l=l!=r;break;
			case 5:l=l<=r;break; case 6:l=l>=r;break; case 7:l=l<r;break; case 8:l=l>r;break; case 9:l=l<<r;break;
			case 10:l=l>>r;break; case 11:l+=r;break; case 12:l-=r;break; case 13:l*=r;break; case 14:l=r?l/r:0;break;
			case 15:l=r?l%r:0;break; case 16:l&=r;break; case 17:l|=r;break; case 18:l^=r;break; } }
	return l;
}
static long eval_if(const char *s) { ep = s; return ep_expr(0); }

/* Substitute \param -> arg, \@ -> uid, \() -> "" in a macro body line, into out. */
static void subst(const char *in, Macro *m, char args[][256], int na, int uid, char *out) {
	char *o = out;
	for (const char *p = in; *p; ) {
		if (*p == '\\') {
			p++;
			if (*p == '@') { p++; o += sprintf(o, "%d", uid); continue; }
			if (*p == '(' && p[1] == ')') { p += 2; continue; }
			char nm[32]; int i = 0; while (*p && (*p=='_'||(*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')) && i<31) nm[i++]=*p++; nm[i]=0;
			int found = 0;
			for (int k = 0; k < m->nparams; k++) if (!strcmp(nm, m->params[k])) { if (k < na) { strcpy(o, args[k]); o += strlen(o); } found = 1; break; }
			if (!found) { *o++ = '\\'; strcpy(o, nm); o += strlen(nm); }
		} else *o++ = *p++;
	}
	*o = 0;
}

static void feed_line(char *line);
static void expand_macro(Macro *m, const char *argline) {
	char args[16][256]; int na = 0;                  /* split argline on commas, trimming spaces */
	const char *p = argline; while (*p==' '||*p=='\t') p++;
	while (*p && na < 16) { char *d = args[na]; int depth = 0;
		while (*p && !(*p==',' && depth==0)) { if(*p=='(')depth++; else if(*p==')')depth--; *d++=*p++; }
		*d = 0; char *e = args[na] + strlen(args[na]); while (e>args[na] && (e[-1]==' '||e[-1]=='\t')) *--e=0;
		na++; if (*p==',') { p++; while(*p==' '||*p=='\t')p++; } }
	if (*p) die("macro invoked with too many args (>16)");
	int uid = macuid++;
	for (int i = 0; i < m->nbody; i++) { char out[1024]; subst(m->body[i], m, args, na, uid, out); feed_line(out); }
}

/* Collection state for the body of a .macro / .rept being read. */
static int coll_mode, coll_depth, coll_n; static char *coll_body[2048];
static char coll_name[64], coll_params_src[256]; static long coll_reptn;

static void feed_line(char *line) {
	if (!coll_mode) {   /* GAS ';' statement separator: split one line into statements (quote-aware; '@' = comment) */
		int inq = 0; char qc = 0;
		for (char *p = line; *p; p++) {
			if (inq) { if (*p == qc) inq = 0; else if (*p == '\\' && p[1]) p++; }
			else if (*p == '"' || *p == '\'') { inq = 1; qc = *p; }
			else if (*p == '@') break;
			else if (*p == ';') { *p = 0; feed_line(line); feed_line(p + 1); return; }
		}
	}
	{ const char *q = line; while (*q==' '||*q=='\t') q++; if (*q == '#') return; }   /* cpp line marker / `#` comment */
	char w[64]; const char *rest = lead(line, w);
	if (coll_mode) {                                 /* gathering a macro/rept body until the matching end */
		if (!strcmp(w,".macro")||!strcmp(w,".rept")||!strcmp(w,".irp")||!strcmp(w,".irpc")) coll_depth++;
		if (!strcmp(w,".endm")||!strcmp(w,".endr")) { if (--coll_depth == 0) {
			if (coll_mode == 1) {                    /* finish a .macro definition */
				if (nmacros >= 256) die("too many .macro definitions (>256)");
				Macro *m = &macros[nmacros++]; memset(m, 0, sizeof *m); strncpy(m->name, coll_name, 63);
				const char *s = coll_params_src;     /* params: comma/space separated */
				while (*s) {
					while (*s==' '||*s=='\t'||*s==',') s++;
					if (!*s) break;
					char *d = m->params[m->nparams]; int i = 0;
					while (*s && *s!=' '&&*s!='\t'&&*s!=',' && i<31) d[i++] = *s++;
					d[i] = 0;
					if (d[0]) m->nparams++;
				}
				m->nbody = coll_n; for (int i=0;i<coll_n;i++) m->body[i]=coll_body[i];
				coll_mode = 0; coll_n = 0;
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
	if (!strcmp(w, ".macro")) { const char *r=rest; while(*r==' '||*r=='\t'||*r==',')r++; char nm[64]; const char *a=lead(r,nm); strncpy(coll_name,nm,63); strncpy(coll_params_src,a,255); coll_mode=1; coll_depth=1; coll_n=0; return; }
	if (!strcmp(w, ".rept"))  { coll_reptn = emitting()?eval_if(rest):0; coll_mode=2; coll_depth=1; coll_n=0; return; }
	if (!strcmp(w, ".if"))     { int on = emitting() && eval_if(rest)!=0; if (nifs >= 64) die("too many nested .if (>64)"); ifs[nifs].active=on; ifs[nifs].taken=on; nifs++; return; }
	if (!strcmp(w, ".ifdef"))  { char nm[64]; lead(rest,nm); int on = emitting() && sym_find(nm)>=0 && syms[sym_find(nm)].defined; if (nifs >= 64) die("too many nested .if (>64)"); ifs[nifs].active=on; ifs[nifs].taken=on; nifs++; return; }
	if (!strcmp(w, ".ifndef")) { char nm[64]; lead(rest,nm); int on = emitting() && !(sym_find(nm)>=0 && syms[sym_find(nm)].defined); if (nifs >= 64) die("too many nested .if (>64)"); ifs[nifs].active=on; ifs[nifs].taken=on; nifs++; return; }
	if (!strcmp(w, ".ifc") || !strcmp(w, ".ifnc")) {   /* GAS string compare: .ifc a,b (same) / .ifnc a,b (differ) */
		const char *comma = strchr(rest, ','); char a[128] = "", b[128] = "";
		if (comma) { int la = (int)(comma - rest); if (la > 127) la = 127; memcpy(a, rest, la); a[la] = 0; strncpy(b, comma + 1, 127); }
		char *pa = a; while (*pa==' '||*pa=='\t') pa++; { char *e = pa + strlen(pa); while (e>pa && (e[-1]==' '||e[-1]=='\t'||e[-1]=='\n'||e[-1]=='\r')) *--e = 0; }
		char *pb = b; while (*pb==' '||*pb=='\t') pb++; { char *e = pb + strlen(pb); while (e>pb && (e[-1]==' '||e[-1]=='\t'||e[-1]=='\n'||e[-1]=='\r')) *--e = 0; }
		int same = !strcmp(pa, pb), on = emitting() && (!strcmp(w, ".ifc") ? same : !same);
		if (nifs >= 64) die("too many nested .if (>64)");
		ifs[nifs].active=on; ifs[nifs].taken=on; nifs++; return;
	}
	if (!strcmp(w, ".else"))   { if (nifs) { int parent=1; for(int i=0;i<nifs-1;i++) if(!ifs[i].active)parent=0; ifs[nifs-1].active = parent && !ifs[nifs-1].taken; if(ifs[nifs-1].active) ifs[nifs-1].taken=1; } return; }
	if (!strcmp(w, ".endif"))  { if (nifs) nifs--; return; }
	if (!strcmp(w, ".err"))    { if (emitting()) die(".err directive reached (assembly assertion failed)"); return; }
	if (!emitting()) return;                         /* inside a false .if branch */
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

	resolve_fixups();
	md_finish();            /* let the arch backend resolve its own end-of-pass fixups (ldr literals) */
	reduce_local_relocs();  /* fold local-symbol relocs to section-symbol + in-place value (GNU parity) */
	flush_padmaps();
	drop_end_mapsyms();   /* a mapping symbol at a section's end marks nothing */
	obj_write(out);
	return 0;
}
