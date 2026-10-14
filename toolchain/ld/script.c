/*
 * script.c — the ONE layout engine. Every link is driven by a linker script: the -T file, or (without -T) the
 * built-in DEFAULT script below — the same machinery either way, as in GNU ld.
 *
 *   read    (script_read)  tokens -> statements: MEMORY regions, ENTRY, symbol assignments (PROVIDE /
 *                          PROVIDE_HIDDEN), ASSERT, and SECTIONS — output-section statements whose bodies list
 *                          input-section patterns and assignments. Every symbol the script defines is DECLARED
 *                          now (so the relocation scan knows it resolves locally); its value comes in layout.
 *   match   (layout_match) each allocatable input section goes to the first pattern that names it (/DISCARD/
 *                          drops it); within one pattern, sections keep command-line order.
 *   layout  (layout_run)   unmatched sections become ORPHANS: into the output section of their name, else a new
 *                          one after the last output section of the same kind (code / read-only / data / bss).
 *                          Then addresses: the statements run in order, moving `.` (or a MEMORY region's cursor)
 *                          and assigning each input section a run address (VMA) and a load address (LMA:
 *                          AT(expr) / AT> region, else the VMA). Output sections group into PT_LOAD segments —
 *                          a new segment where the load delta changes, on a page gap, after a .bss tail, or where
 *                          read-only turns writable on a fresh page (so W^X can hold). The headers map into the
 *                          first segment when the script leaves room (SIZEOF_HEADERS); file offsets are congruent
 *                          to addresses mod the page size, as the ELF loader requires. SIZEOF_HEADERS depends on
 *                          the program-header count, so the layout reruns until that count is stable.
 *
 * Not modelled (each an error, never silently ignored): GROUP/INPUT, INSERT, CREATE_OBJECT_SYMBOLS, EXCLUDE_FILE,
 * SORT_BY_ALIGNMENT/SORT_BY_INIT_PRIORITY/SORT_NONE, an ASSERT in an output section, compound assignments (+= -=).
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "ld.h"
#include "armattrs.h"

OutSec os_discard;
OutSec **outsecs; int noutsec; static int outcap;
Seg *segs; int nseg; static int segcap;
Elf32_Phdr *phdrs; int nphdr; static int phcap;
u32 hdrsz;
static int headers_mapped, gnu_stack;
static int relro;                                         /* the default layout's RELRO region is on */
static const char *const relro_secs[] = { ".tdata", ".tbss", ".preinit_array", ".init_array", ".fini_array", ".data.rel.ro", ".got", 0 };
u32 tls_vaddr, tls_align;                                 /* the TLS template (PT_TLS): its address + alignment (align 0: none) */
static int tls_bss(const OutSec *os) { return (os->flags & SHF_TLS) && os->type == SHT_NOBITS; }   /* .tbss */
static const char *script_name;

/* ---- tokens -------------------------------------------------------------------------------------------- */
static char **tok; static int ntok, tokcap;
static int is_punct(char c) { return strchr("{}():;=+?,<>&|^%~!", c) != NULL; }   /* '*' '/' '-' are name chars:
                                                                                  * *(.text*), /DISCARD/, .note.GNU-stack */
typedef struct { const char *b, *e; int file; } Span;
static Span *span; static int spancap;                 /* each token's source text (quotes included); file: which
                                                        * script text (the -T one 0, an INCLUDEd one 1, 2, …) */
static int tok_file, nfile;
static void push_tok(const char *b, int n) {
	tok = grow(tok, ntok, &tokcap, sizeof *tok);
	char *t = malloc(n + 1); memcpy(t, b, n); t[n] = 0; tok[ntok++] = t;
}
static void push_span(const char *b, const char *e) { span = grow(span, ntok - 1, &spancap, sizeof *span); span[ntok - 1] = (Span){ b, e, tok_file }; }
static char *span_text(int lo, int hi) {   /* the source text of tokens [lo, hi) */
	if (lo >= hi) return strdup("");
	if (span[lo].file != span[hi - 1].file) die("linker script %s: an INCLUDE inside a VERSION block is not supported", script_name);
	size_t n = (size_t)(span[hi - 1].e - span[lo].b); char *t = malloc(n + 1); memcpy(t, span[lo].b, n); t[n] = 0;
	return t;
}
/* A script file by name: as given, else in the -L directories that came before the -T (GNU ld reads a -T script as it
 * parses the command line; INCLUDE and -T search alike). NULL: not found. */
char *script_find(const char *name) {
	FILE *f = fopen(name, "rb"); char path[4096];
	for (int d = 0; !f && name[0] != '/' && d < script_ndirs; d++) { snprintf(path, sizeof path, "%s/%s", libdirs[d], name); f = fopen(path, "rb"); name = f ? path : name; }
	if (!f) return NULL;
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
	char *text = malloc(n + 1); if (fread(text, 1, n, f) != (size_t)n) die("read %s failed", name);
	text[n] = 0; fclose(f); return text;
}
static void tokenize(const char *s);
static void include_file(const char *name) {   /* INCLUDE FILE: its tokens in place of the directive */
	static int depth;
	if (++depth > 32) die("linker script %s: INCLUDE nested over 32 deep (a loop?)", script_name);
	char *text = script_find(name);
	if (!text) die("linker script %s: INCLUDE %s: cannot find it (searched . and %d -L dir(s))", script_name, name, script_ndirs);
	int saved = tok_file; tok_file = ++nfile;
	tokenize(text);                                    /* (kept: tokens' spans point into it) */
	tok_file = saved; depth--;
}
static void tokenize(const char *s) {
	static const char *const two[] = { "<<", ">>", "==", "!=", "<=", ">=", "&&", "||", "+=", "-=", 0 };
	while (*s) {
		if (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') { s++; continue; }
		if (s[0] == '/' && s[1] == '*') { s += 2; while (*s && !(s[0] == '*' && s[1] == '/')) s++; if (*s) s += 2; continue; }
		if (*s == '"') { const char *q = s, *b = ++s; while (*s && *s != '"') s++; push_tok(b, (int)(s - b)); if (*s) s++; push_span(q, s); continue; }
		int k = 0; while (two[k] && strncmp(s, two[k], 2)) k++;
		if (two[k]) { push_tok(s, 2); push_span(s, s + 2); s += 2; continue; }
		if (is_punct(*s)) { push_tok(s, 1); push_span(s, s + 1); s++; continue; }
		const char *b = s;
		while (*s && !is_punct(*s) && *s != ' ' && *s != '\t' && *s != '\n' && *s != '\r' && *s != '"') s++;
		if (s - b == 7 && !strncmp(b, "INCLUDE", 7)) {   /* INCLUDE FILE (anywhere: top level, MEMORY, SECTIONS, an output section) */
			while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
			const char *f = s; char name[4096];
			if (*s == '"') { f = ++s; while (*s && *s != '"') s++; }
			else while (*s && !strchr(" \t\n\r;", *s)) s++;
			if (s == f || s - f >= (long)sizeof name) die("linker script %s: INCLUDE: expected a file name", script_name);
			memcpy(name, f, s - f); name[s - f] = 0;
			if (*s == '"') s++;
			include_file(name);
			continue;
		}
		push_tok(b, (int)(s - b)); push_span(b, s);
	}
}
static int p;                                          /* the parse cursor */
static int nreal;                                      /* the script's own tokens (synthesized ones follow) */
static const char *peek(int k) { return p + k < nreal ? tok[p + k] : ""; }
static int is(const char *t) { return !strcmp(peek(0), t); }
static void expect(const char *t) {
	if (!is(t)) die("linker script %s: expected '%s', found '%s'", script_name, t, p < ntok ? tok[p] : "end of file");
	p++;
}

/* ---- statements -------------------------------------------------------------------------------------- */
typedef struct { int lo, hi; } Expr;                   /* a token range, evaluated during layout (lo < 0: absent) */
typedef struct {                                       /* a symbol assignment ("." = the location counter) */
	const char *sym; Expr e;
	int live;                                          /* defines the symbol (a PROVIDE nobody needs doesn't)   */
	int in_sections;                                   /* inside SECTIONS: an image address, else an absolute value */
} Assign;
enum { I_INPUT, I_ASSIGN, I_DATA };
typedef struct {                                       /* an output-section body item */
	int kind;
	const char *file; char **pats; int npat, sort;     /* I_INPUT: file glob + section globs; sort: an S_ kind   */
	int keep;                                          /* I_INPUT: KEEP(...) — a --gc-sections root             */
	InSec *in; int nin, cap;                           /* the input sections it took                            */
	Assign a;                                          /* I_ASSIGN                                              */
	int dsize, dsigned; Expr de; u32 dva;              /* I_DATA: BYTE/SHORT/LONG/QUAD/SQUAD (1/2/4/8 bytes) of de, at dva */
} Item;
enum { S_ASSIGN, S_OUTSEC, S_ASSERT };
typedef struct {
	int kind;
	Assign a;                                          /* S_ASSIGN; S_ASSERT: a.e + a.sym = the message         */
	int ok;                                            /* S_ASSERT: held in the latest layout pass              */
	OutSec *os; int discard, noload, noalloc;          /* S_OUTSEC; noalloc: (COPY/INFO/OVERLAY/DSECT) — GNU's
	                                                    * noalloc_section: its inputs lose SHF_ALLOC           */
	Item *items; int nitem, itemcap;
	Expr addr, at, align;                              /* `name addr :`, AT(lma), ALIGN(n)                      */
	int vregion, lregion;                              /* `> REGION`, `AT> REGION` (-1: none)                   */
	const char *phname[8]; int nphname;                /* `:PHDR ...` (NONE: in no segment); none: the previous section's */
} Stmt;
/* PHDRS { NAME TYPE [FILEHDR] [PHDRS] [AT(addr)] [FLAGS(n)] ; ... }: the program headers, exactly (GNU adds only
 * PT_ARM_EXIDX); an allocated output section goes in the segments its `:NAME`s list, or the previous section's. */
typedef struct { const char *name; u32 type; int filehdr, phdrs; Expr at, flags; } PhDef;
static PhDef phdef[32]; static int nphdef, have_phdrs;
static Stmt *st; static int nst, stcap;
static struct { const char *name; u32 origin, length, cur; } regions[16]; static int nregion;
GSym **declared; int ndeclared; static int declcap;  /* the script's symbols (their values reset each pass) */

static Stmt *new_stmt(int kind) {
	st = grow(st, nst, &stcap, sizeof *st);
	Stmt *s = &st[nst++]; memset(s, 0, sizeof *s);
	s->kind = kind; s->addr.lo = s->at.lo = s->align.lo = -1; s->vregion = s->lregion = -1;
	return s;
}
static Item *new_item(Stmt *s, int kind) {
	s->items = grow(s->items, s->nitem, &s->itemcap, sizeof *s->items);
	Item *it = &s->items[s->nitem++]; memset(it, 0, sizeof *it); it->kind = kind;
	return it;
}
static int region_find(const char *n) {
	for (int i = 0; i < nregion; i++) if (!strcmp(regions[i].name, n)) return i;
	die("linker script %s: no MEMORY region '%s'", script_name, n);
}

/* Declare a script symbol when the script is read (after symbol resolution). A plain assignment defines it (an
 * object defining it too is an error); PROVIDE defines it only if it's referenced and nothing else defines it. */
static void declare(Assign *a, int provide, int hidden, int in_sections) {
	a->live = 1; a->in_sections = in_sections;
	if (!strcmp(a->sym, ".")) {
		if (!in_sections) die("linker script %s: '.' assigned outside SECTIONS", script_name);
		return;
	}
	GSym *g = provide ? gsym_find(a->sym) : gsym_get(a->sym);
	if (provide && (!g || g->defined || g->linker)) { a->live = 0; return; }
	if (g->defined) die("'%s' is defined both in %s and by linker script %s", a->sym, g->obj ? g->obj->path : "the link", script_name);
	if (!g->linker) { declared = grow(declared, ndeclared, &declcap, sizeof *declared); declared[ndeclared++] = g; }
	g->linker = 1; g->abs = !in_sections; g->hidden |= hidden;
}
/* An expression runs to `;` (or an unbalanced `)` / `}`) at paren depth 0. */
static Expr parse_expr(void) {
	Expr e = { p, p };
	for (int depth = 0; p < ntok; p++) {
		if (is("(")) depth++;
		else if (is(")")) { if (!depth) break; depth--; }
		else if ((is(";") || is("}") || is(",")) && !depth) break;
	}
	e.hi = p;
	if (e.lo == e.hi) die("linker script %s: empty expression", script_name);
	return e;
}
/* SYM = expr ;  |  PROVIDE(SYM = expr) ;  |  PROVIDE_HIDDEN(...) ;  |  HIDDEN(...) ;  — at p. */
static int at_assignment(void) {
	return !strcmp(peek(1), "=") || !strcmp(peek(1), "+=") || !strcmp(peek(1), "-=") ||
	       ((is("PROVIDE") || is("PROVIDE_HIDDEN") || is("HIDDEN")) && !strcmp(peek(1), "("));
}
static void parse_assignment(Assign *a, int in_sections) {
	int provide = is("PROVIDE") || is("PROVIDE_HIDDEN"), hidden = is("PROVIDE_HIDDEN") || is("HIDDEN"), wrapped = hidden || provide;
	if (wrapped) { p++; expect("("); }
	a->sym = tok[p++];
	if (is("+=") || is("-=")) die("linker script %s: compound assignment to '%s' is not supported", script_name, a->sym);
	expect("=");
	a->e = parse_expr();
	if (wrapped) expect(")");
	if (is(";")) p++;
	declare(a, provide, hidden, in_sections);
}
static void parse_memory(void) {
	expect("{");
	while (!is("}")) {
		if (nregion == 16) die("linker script %s: too many MEMORY regions (>16)", script_name);
		regions[nregion].name = tok[p++];
		if (is("(")) { while (p < ntok && !is(")")) p++; expect(")"); }   /* (rwx) attributes */
		expect(":");
		for (int k = 0; k < 2; k++) {
			const char *key = tok[p++]; expect("=");
			char *end; unsigned long v = strtoul(peek(0), &end, 0);
			if (*end == 'K' || *end == 'k') v <<= 10, end++; else if (*end == 'M' || *end == 'm') v <<= 20, end++;
			if (*end) die("linker script %s: MEMORY %s: '%s' is not a number", script_name, key, peek(0));
			p++;
			if (!strcmp(key, "ORIGIN") || !strcmp(key, "org") || !strcmp(key, "o")) regions[nregion].origin = (u32)v;
			else if (!strcmp(key, "LENGTH") || !strcmp(key, "len") || !strcmp(key, "l")) regions[nregion].length = (u32)v;
			else die("linker script %s: MEMORY: unknown attribute '%s'", script_name, key);
			if (k == 0) expect(",");
		}
		nregion++;
	}
	expect("}");
}
static void unsupported(const char *what) {
	die("linker script %s: '%s' is not supported", script_name, what);
}
/* One input spec: FILEGLOB ( SECGLOB... ) with SORT_BY_NAME(...) inside, or KEEP( that ) around it. */
/* A section/file name pattern: the tokens that touch in the source, joined — GNU's lexer takes `___ksymtab+*`, `.text.*`
 * as one name where our tokenizer split them at the '+'. */
static char *pattern_tok(void) {
	int lo = p++;
	while (p < nreal && span[p].b == span[p - 1].e && strcmp(tok[p], "(") && strcmp(tok[p], ")") && strcmp(tok[p - 1], "(")) p++;
	return p - lo == 1 ? (char *)tok[lo] : span_text(lo, p);
}
/* GNU's sort kinds: SORT/SORT_BY_NAME, SORT_BY_ALIGNMENT (larger first), and the two nestings (outer key first). */
enum { S_NONE, S_NAME, S_ALIGN, S_NAME_ALIGN, S_ALIGN_NAME };
static int sort_word(void) { return is("SORT") || is("SORT_BY_NAME") ? S_NAME : is("SORT_BY_ALIGNMENT") ? S_ALIGN : S_NONE; }
static void parse_input(Stmt *s) {
	int keep = is("KEEP");
	if (keep) { p++; expect("("); }
	Item *it = new_item(s, I_INPUT);
	it->keep = keep;
	it->file = tok[p++];
	if (!strcmp(it->file, "EXCLUDE_FILE")) unsupported("EXCLUDE_FILE");
	expect("(");
	int kinds = 0;                                      /* (each pattern its own sort, in GNU; one per input spec here) */
	while (!is(")")) {
		int outer = sort_word(), kind = S_NONE, depth = 0;
		if (outer) {
			p++; expect("("); depth = 1; kind = outer;
			int inner = sort_word();
			if (inner) { p++; expect("("); depth = 2; kind = outer == inner ? outer : outer == S_NAME ? S_NAME_ALIGN : S_ALIGN_NAME; }
		}
		if (is("EXCLUDE_FILE") || is("SORT_BY_INIT_PRIORITY") || is("SORT_NONE") || is("REVERSE")) unsupported(peek(0));
		if (depth) while (!is(")")) { it->pats = realloc(it->pats, (it->npat + 1) * sizeof *it->pats); it->pats[it->npat++] = pattern_tok(); }
		else { it->pats = realloc(it->pats, (it->npat + 1) * sizeof *it->pats); it->pats[it->npat++] = pattern_tok(); }
		while (depth--) expect(")");
		if (kinds++ && kind != it->sort) die("linker script %s: an input spec mixing sorted and unsorted (or differently sorted) patterns: unsupported", script_name);
		it->sort = kind;
	}
	expect(")");
	if (keep) expect(")");
}
static void parse_fill(Stmt *s);
static void parse_outsec_body(Stmt *s, const char *name);
static void parse_outsec_trailer(Stmt *s);
static void parse_outsec(void) {
	Stmt *s = new_stmt(S_OUTSEC);
	const char *name = tok[p++];
	s->discard = !strcmp(name, "/DISCARD/");
	if (!s->discard) { s->os = calloc(1, sizeof *s->os); s->os->name = name; }
	if (!is(":")) {                                     /* `name [addr] [(NOLOAD)] :` */
		int lo = p;
		while (p < ntok && !is(":")) {
			if (is("(") && !strcmp(peek(1), "NOLOAD") && !strcmp(peek(2), ")")) { s->noload = 1; break; }
			if (is("(") && (!strcmp(peek(1), "COPY") || !strcmp(peek(1), "INFO") || !strcmp(peek(1), "OVERLAY") || !strcmp(peek(1), "DSECT"))
			    && !strcmp(peek(2), ")")) { s->noalloc = 1; break; }
			p++;
		}
		if (p > lo) s->addr = (Expr){ lo, p };
		if (s->noload || s->noalloc) p += 3;
	}
	expect(":");
	while (!is("{")) {
		if (is("AT") && !strcmp(peek(1), "(")) { p += 2; s->at = parse_expr(); expect(")"); }
		else if (is("ALIGN") && !strcmp(peek(1), "(")) { p += 2; s->align = parse_expr(); expect(")"); }
		else unsupported(peek(0));
	}
	parse_outsec_body(s, name);
	parse_outsec_trailer(s);
}
static void parse_outsec_body(Stmt *s, const char *name) {   /* `{ inputs and assignments }` */
	expect("{");
	while (!is("}")) {
		if (p >= nreal) die("linker script %s: unterminated output section '%s'", script_name, name);
		if (is(";")) { p++; continue; }
		if (at_assignment()) { Item *it = new_item(s, I_ASSIGN); parse_assignment(&it->a, 1); continue; }
		if (is("CONSTRUCTORS")) { p++; continue; }   /* (a.out's; ELF: nothing — GNU ignores it too) */
		if ((is("BYTE") || is("SHORT") || is("LONG") || is("QUAD") || is("SQUAD")) && !strcmp(peek(1), "(")) {
			Item *it = new_item(s, I_DATA); it->dsize = is("BYTE") ? 1 : is("SHORT") ? 2 : is("LONG") ? 4 : 8; it->dsigned = is("SQUAD");
			p += 2; it->de = parse_expr(); expect(")"); continue;
		}
		if (is("FILL") && !strcmp(peek(1), "(")) {   /* the gaps' pattern (GNU: from here on; here: the whole section's) */
			p += 2; if (!s->os) die("linker script %s: FILL in /DISCARD/", script_name);
			parse_fill(s); expect(")"); continue;
		}
		if (is("CREATE_OBJECT_SYMBOLS") || is("ASSERT")) unsupported(peek(0));
		parse_input(s);
	}
	expect("}");
}
static void parse_outsec_trailer(Stmt *s) {   /* `> REGION`, `AT> REGION`, `:PHDR`..., `=FILL`, `,` */
	for (;;) {
		if (is(">")) { p++; s->vregion = region_find(tok[p++]); }
		else if (is("AT") && !strcmp(peek(1), ">")) { p += 2; s->lregion = region_find(tok[p++]); }
		else if (is(":")) { p++; if (s->nphname == 8) die("linker script %s: more than 8 :PHDRs", script_name); s->phname[s->nphname++] = tok[p++]; }
		else if (is("=")) { p++; parse_fill(s); }
		else if (is(",")) p++;
		else break;
	}
}
/* An expression the linker writes itself: its tokens go past the script's own (nreal), where the parser never reads. */
static Expr synth(const char *fmt, ...) {
	char buf[512]; va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
	int lo = ntok; tokenize(strdup(buf));
	return (Expr){ lo, ntok };
}
/* OVERLAY [START] : [NOCROSSREFS] [AT(LDADDR)] { SEC { ... } [:PHDR...] [=FILL] ... } [:PHDR...] [=FILL]: the sections
 * share START as their address and follow each other in load memory from LDADDR; each gets __load_start_SEC and
 * __load_stop_SEC (SEC's non-identifier characters dropped); after it `.` is START + the largest one's size. */
static void parse_overlay(void) {
	p++;
	Expr start = { -1, -1 }, at = { -1, -1 };
	if (!is(":")) { int lo = p; while (p < nreal && !is(":")) p++; start = (Expr){ lo, p }; }
	expect(":");
	while (!is("{")) {
		if (is("NOCROSSREFS")) p++;
		else if (is("AT") && !strcmp(peek(1), "(")) { p += 2; at = parse_expr(); expect(")"); }
		else unsupported(peek(0));
	}
	expect("{");
	const char *names[32]; int n = 0, first = nst; static int noverlay; noverlay++;
	while (!is("}")) {
		if (p >= nreal) die("linker script %s: unterminated OVERLAY", script_name);
		if (n == 32) die("linker script %s: more than 32 sections in an OVERLAY", script_name);
		Stmt *s = new_stmt(S_OUTSEC); const char *name = tok[p++];
		s->os = calloc(1, sizeof *s->os); s->os->name = name; s->os->overlay = noverlay;
		if (!n) { s->addr = start; s->at = at; }
		else { s->addr = synth("ADDR(%s)", names[0]); s->at = synth("LOADADDR(%s) + SIZEOF(%s)", names[n - 1], names[n - 1]); }
		parse_outsec_body(s, name); parse_outsec_trailer(s);
		char id[256]; int k = 0; for (const char *c = name; *c && k < 255; c++) if (isalnum((unsigned char)*c) || *c == '_') id[k++] = *c; id[k] = 0;
		for (int stop = 0; stop < 2; stop++) {
			Stmt *a = new_stmt(S_ASSIGN); char *sym = malloc(strlen(id) + 16);
			sprintf(sym, stop ? "__load_stop_%s" : "__load_start_%s", id); a->a.sym = sym; a->a.in_sections = 1;
			a->a.e = stop ? synth("LOADADDR(%s) + SIZEOF(%s)", name, name) : synth("LOADADDR(%s)", name);
			declare(&a->a, 0, 0, 1);
		}
		names[n++] = name;
	}
	expect("}");
	Stmt trailer; memset(&trailer, 0, sizeof trailer); trailer.vregion = trailer.lregion = -1;
	trailer.os = &(OutSec){ 0 };
	parse_outsec_trailer(&trailer);
	if (trailer.vregion >= 0 || trailer.lregion >= 0) unsupported("a MEMORY region for an OVERLAY");
	for (int i = first; i < nst; i++) if (st[i].kind == S_OUTSEC) {   /* the overlay's :PHDRs and fill: each section's */
		for (int k = 0; k < trailer.nphname && st[i].nphname < 8; k++) st[i].phname[st[i].nphname++] = trailer.phname[k];
		if (trailer.os->nfill && !st[i].os->nfill) { memcpy(st[i].os->fill, trailer.os->fill, sizeof trailer.os->fill); st[i].os->nfill = trailer.os->nfill; }
	}
	char mx[400] = "0"; for (int i = 0; i < n; i++) { char t[400]; snprintf(t, sizeof t, "MAX(%s, SIZEOF(%s))", mx, names[i]); strcpy(mx, t); }
	Stmt *d = new_stmt(S_ASSIGN); d->a.sym = "."; d->a.e = synth("ADDR(%s) + %s", names[0], mx); declare(&d->a, 0, 0, 1);
}
static u32 eval(Expr e, u32 dot);
static int hexdigit(char c) { return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10; }
static void parse_fill(Stmt *s) {   /* GNU's exp_get_fill: a hex literal is its bytes, as written; else a 4-byte big-endian value */
	const char *t = tok[p]; OutSec *os = s->os;
	if (!os) die("linker script %s: a fill for /DISCARD/", script_name);
	int hex = t[0] == '0' && (t[1] == 'x' || t[1] == 'X') && t[2] && strspn(t + 2, "0123456789abcdefABCDEF") == strlen(t + 2);
	const char *nx = peek(1);
	if (hex && strlen(t + 2) <= 2 * sizeof os->fill && !(strchr("+-*/%&|^", nx[0]) && nx[0]) && strcmp(nx, "<<") && strcmp(nx, ">>")) {
		const char *h = t + 2; size_t n = strlen(h), i = 0; int k = 0;
		if (n & 1) os->fill[k++] = (u8)hexdigit(h[i++]);   /* an odd count: the first digit alone */
		for (; i < n; i += 2) os->fill[k++] = (u8)(hexdigit(h[i]) << 4 | hexdigit(h[i + 1]));
		os->nfill = k; p++;
		return;
	}
	u32 v = eval(parse_expr(), 0);
	os->fill[0] = (u8)(v >> 24); os->fill[1] = (u8)(v >> 16); os->fill[2] = (u8)(v >> 8); os->fill[3] = (u8)v; os->nfill = 4;
}
static void parse_phdrs(void) {
	static const struct { const char *n; u32 v; } types[] = { {"PT_NULL", 0}, {"PT_LOAD", 1}, {"PT_DYNAMIC", 2}, {"PT_INTERP", 3},
		{"PT_NOTE", 4}, {"PT_SHLIB", 5}, {"PT_PHDR", 6}, {"PT_TLS", 7}, {"PT_GNU_EH_FRAME", 0x6474e550u}, {"PT_GNU_STACK", 0x6474e551u},
		{"PT_GNU_RELRO", 0x6474e552u}, {"PT_GNU_PROPERTY", 0x6474e553u} };
	expect("{"); have_phdrs = 1;
	while (!is("}")) {
		if (p >= ntok) die("linker script %s: unterminated PHDRS", script_name);
		if (is(";")) { p++; continue; }
		if (nphdef == 32) die("linker script %s: more than 32 PHDRS", script_name);
		PhDef *d = &phdef[nphdef++]; memset(d, 0, sizeof *d); d->at.lo = d->flags.lo = -1;
		d->name = tok[p++];
		const char *t = tok[p++]; int k;
		for (k = 0; k < (int)(sizeof types / sizeof *types); k++) if (!strcmp(types[k].n, t)) break;
		if (k < (int)(sizeof types / sizeof *types)) d->type = types[k].v;
		else { char *e; d->type = (u32)strtoul(t, &e, 0); if (*e) die("linker script %s: PHDRS: unknown type %s", script_name, t); }
		while (!is(";")) {
			if (is("FILEHDR")) { d->filehdr = 1; p++; }
			else if (is("PHDRS")) { d->phdrs = 1; p++; }
			else if (is("AT") && !strcmp(peek(1), "(")) { p += 2; d->at = parse_expr(); expect(")"); }
			else if (is("FLAGS") && !strcmp(peek(1), "(")) { p += 2; d->flags = parse_expr(); expect(")"); }
			else die("linker script %s: PHDRS: unexpected '%s'", script_name, peek(0));
		}
		p++;
	}
	expect("}");
}
static void parse_assert(void) {                       /* ASSERT(expr, "message") — checked on the final layout */
	Stmt *s = new_stmt(S_ASSERT);
	p++; expect("("); s->a.e = parse_expr(); expect(","); s->a.sym = tok[p++]; expect(")");
	if (is(";")) p++;
}
static void parse_sections(void) {
	expect("{");
	while (!is("}")) {
		if (p >= ntok) die("linker script %s: unterminated SECTIONS", script_name);
		if (is(";")) { p++; continue; }
		if (at_assignment()) { parse_assignment(&new_stmt(S_ASSIGN)->a, 1); continue; }
		if (is("ASSERT")) { parse_assert(); continue; }
		if (is("OVERLAY")) { parse_overlay(); continue; }
		if (is("INSERT")) unsupported(peek(0));
		parse_outsec();
	}
	expect("}");
}

/* GNU armelf's dynamic relocation statements: -z combreloc's one .rel.dyn (sorted), or -z nocombreloc's one per kind. */
static const char rel_combined[] =
	"  .rel.dyn  : { *(.rel.init) *(.rel.text .rel.text.* .rel.gnu.linkonce.t.*) *(.rel.fini)\n"
	"                *(.rel.rodata .rel.rodata.* .rel.gnu.linkonce.r.*) *(.rel.data.rel.ro .rel.data.rel.ro.* .rel.gnu.linkonce.d.rel.ro.*)\n"
	"                *(.rel.data .rel.data.* .rel.gnu.linkonce.d.*) *(.rel.tdata .rel.tdata.* .rel.gnu.linkonce.td.*)\n"
	"                *(.rel.tbss .rel.tbss.* .rel.gnu.linkonce.tb.*) *(.rel.ctors) *(.rel.dtors) *(.rel.got)\n"
	"                *(.rel.bss .rel.bss.* .rel.gnu.linkonce.b.*) PROVIDE_HIDDEN(__rel_iplt_start = .); *(.rel.iplt)\n"
	"                PROVIDE_HIDDEN(__rel_iplt_end = .); }\n";
static const char rel_separate[] =
	"  .rel.init : { *(.rel.init) }\n  .rel.text : { *(.rel.text .rel.text.* .rel.gnu.linkonce.t.*) }\n  .rel.fini : { *(.rel.fini) }\n"
	"  .rel.rodata : { *(.rel.rodata .rel.rodata.* .rel.gnu.linkonce.r.*) }\n"
	"  .rel.data.rel.ro : { *(.rel.data.rel.ro .rel.data.rel.ro.* .rel.gnu.linkonce.d.rel.ro.*) }\n"
	"  .rel.data : { *(.rel.data .rel.data.* .rel.gnu.linkonce.d.*) }\n  .rel.tdata : { *(.rel.tdata .rel.tdata.* .rel.gnu.linkonce.td.*) }\n"
	"  .rel.tbss : { *(.rel.tbss .rel.tbss.* .rel.gnu.linkonce.tb.*) }\n  .rel.ctors : { *(.rel.ctors) }\n  .rel.dtors : { *(.rel.dtors) }\n"
	"  .rel.got : { *(.rel.got) }\n  .rel.bss : { *(.rel.bss .rel.bss.* .rel.gnu.linkonce.b.*) }\n"
	"  .rel.iplt : { PROVIDE_HIDDEN(__rel_iplt_start = .); *(.rel.iplt) PROVIDE_HIDDEN(__rel_iplt_end = .); }\n";
static const char default_script[] =
	"ENTRY(_start)\n"
	"SECTIONS {\n"
	"  . = %#x + SIZEOF_HEADERS;\n"
	"  .note.gnu.build-id : { *(.note.gnu.build-id) }\n"   /* (only with --build-id) */
	"  .init     : { KEEP(*(.init)) }\n"
	"  .text     : { *(.text .text.*) }\n"
	"  .fini     : { KEEP(*(.fini)) }\n"
	"  PROVIDE(__etext = .); PROVIDE(_etext = .); PROVIDE(etext = .);\n"
	"  .rodata   : { *(.rodata .rodata.*) }\n"
	"  .ARM.exidx : { PROVIDE_HIDDEN(__exidx_start = .); *(.ARM.exidx .ARM.exidx.*) PROVIDE_HIDDEN(__exidx_end = .); }\n"
	"  .interp   : { *(.interp) }\n"
	"  .plt      : { *(.plt) }\n"
	"  .hash     : { *(.hash) }\n"
	"  .dynsym   : { *(.dynsym) }\n"
	"  .dynstr   : { *(.dynstr) }\n"
	"%s"                                                  /* the dynamic relocations: rel_combined / rel_separate */
	"  .rel.plt  : { *(.rel.plt) }\n"
	"  .dynamic  : { *(.dynamic) }\n"
	"  . = ALIGN(0x1000);\n"                               /* the writable segment starts on a fresh page: W^X */
	/* RELRO: written only while relocating, then write-protected — the TLS template, .init/.fini arrays, .data.rel.ro,
	 * the GOT (.got.plt too: our loader binds eagerly, as GNU ld's -z now); a dynamic output ends it on a page */
	"  .tdata    : { *(.tdata .tdata.*) }\n"               /* the TLS template: initialized, then (.tbss) zeroed */
	"  .tbss     : { *(.tbss .tbss.*) }\n"                 /* (takes no address space: what follows overlaps it) */
	"  .preinit_array : { PROVIDE_HIDDEN(__preinit_array_start = .); KEEP(*(.preinit_array)) PROVIDE_HIDDEN(__preinit_array_end = .); }\n"
	"  .init_array : { PROVIDE_HIDDEN(__init_array_start = .); KEEP(*(SORT_BY_NAME(.init_array.*))) KEEP(*(.init_array))\n"
	"                  PROVIDE_HIDDEN(__init_array_end = .); }\n"
	"  .fini_array : { PROVIDE_HIDDEN(__fini_array_start = .); KEEP(*(SORT_BY_NAME(.fini_array.*))) KEEP(*(.fini_array))\n"
	"                  PROVIDE_HIDDEN(__fini_array_end = .); }\n"
	"  .data.rel.ro : { *(.data.rel.ro .data.rel.ro.*) }\n"
	"  .got      : { *(.got.plt) *(.got) }\n"
	"  %s\n"                                                /* `. = ALIGN(0x1000);` when RELRO applies */
	"  .data     : { *(.data .data.*) }\n"
	"  _edata = .; PROVIDE(edata = .); __bss_start = .; __bss_start__ = .;\n"   /* (GNU armelf: unconditional) */
	"  .bss      : { *(.dynbss) *(.bss .bss.*) *(COMMON) }\n"
	"  _bss_end__ = .; __bss_end__ = .; __end__ = .; _end = .; PROVIDE(end = .);\n"
	/* NON-ALLOCATED, after everything allocated, each at address 0 — the shape of GNU's own default script.
	 * These take file space but no memory, and placing them explicitly (rather than leaving them orphans)
	 * is what keeps them at the END: an orphan .debug_info anchors after .text, which puts debug data in the
	 * middle of the section-header list where GNU has it last. Only the sections an ARM build actually emits
	 * are listed; GNU's script also names the stabs and DWARF-1 families, which nothing here produces.
	 * .ARM.attributes carries the EABI build attributes (float ABI, CPU arch) a consumer checks. */
	"  .comment 0 : { *(.comment) }\n"
	"  .ARM.attributes 0 : { *(.ARM.attributes) }\n"
	"  .note.GNU-stack 0 : { *(.note.GNU-stack) }\n"
	"  .debug_aranges 0 : { *(.debug_aranges) }\n"   /* GNU's order: aranges, then info, then abbrev */
	"  .debug_info 0 : { *(.debug_info .gnu.linkonce.wi.*) }\n"
	"  .debug_abbrev 0 : { *(.debug_abbrev) }\n"
	"  .debug_line 0 : { *(.debug_line .debug_line.* .debug_line_end) }\n"
	"  .debug_line_str 0 : { *(.debug_line_str) }\n"
	"  .debug_frame 0 : { *(.debug_frame) }\n"
	"  .debug_str 0 : { *(.debug_str) }\n"
	"  .debug_loc 0 : { *(.debug_loc) }\n"
	"  .debug_loclists 0 : { *(.debug_loclists) }\n"
	"  .debug_ranges 0 : { *(.debug_ranges) }\n"
	"  .debug_rnglists 0 : { *(.debug_rnglists) }\n"
	"  .debug_macro 0 : { *(.debug_macro) }\n"
	"  .debug_addr 0 : { *(.debug_addr) }\n"
	"  .debug_names 0 : { *(.debug_names) }\n"
	"  .debug_str_offsets 0 : { *(.debug_str_offsets) }\n"
	"}\n";

void script_read(const char *path) {
	char *text;
	if (path) {
		script_name = path;
		text = script_find(path); if (!text) die("cannot open linker script %s (searched . and %d -L dir(s))", path, script_ndirs);
	} else {
		relro = !norelro && (shared || pie || nshlib);     /* a loader will apply it: a dynamic output */
		text = malloc(sizeof default_script + sizeof rel_combined + sizeof rel_separate + 64);
		sprintf(text, default_script, load_base, combreloc ? rel_combined : rel_separate, relro ? ". = ALIGN(0x1000);" : "");
		script_name = "(default)";
	}
	if (ndefsym) {   /* --defsym SYM=EXPR: assignments ahead of the script (absolute, as GNU's) */
		size_t n = strlen(text) + 1; for (int k = 0; k < ndefsym; k++) n += strlen(defsyms[k]) + 3;
		char *all = malloc(n), *q = all;
		for (int k = 0; k < ndefsym; k++) q += sprintf(q, "%s;\n", defsyms[k]);
		strcpy(q, text); text = all;
	}
	tokenize(text); nreal = ntok;
	for (p = 0; p < nreal; ) {
		if (is(";")) { p++; continue; }
		if (is("ENTRY")) { p++; expect("("); if (!entry_sym) entry_sym = tok[p]; p++; expect(")"); continue; }   /* -e wins */
		if (is("MEMORY")) { p++; parse_memory(); continue; }
		if (is("SECTIONS")) { p++; parse_sections(); continue; }
		if (is("PHDRS")) { p++; parse_phdrs(); continue; }
		if (at_assignment()) { parse_assignment(&new_stmt(S_ASSIGN)->a, 0); continue; }
		if (is("ASSERT")) { parse_assert(); continue; }
		if (is("VERSION")) {   /* the version script, between the braces (version.c reads it from the source text) */
			p++; if (!is("{")) die("linker script %s: VERSION needs a { block }", script_name);
			int depth = 0, lo = p;
			do { if (is("{")) depth++; else if (is("}")) depth--; p++; } while (p < ntok && depth);
			if (depth) die("linker script %s: unterminated VERSION block", script_name);
			version_script(span_text(lo + 1, p - 1), script_name);
			continue;
		}
		if (is("OUTPUT_ARCH") || is("OUTPUT_FORMAT") || is("OUTPUT") || is("TARGET") || is("SEARCH_DIR")) {   /* no effect here */
			p++; expect("("); while (p < ntok && !is(")")) p++; expect(")"); continue;
		}
		unsupported(peek(0));
	}
}

/* ---- matching input sections to output sections ------------------------------------------------------- */
static int glob(const char *pat, const char *s) {       /* '*' and '?' wildcards */
	for (; *pat; pat++, s++) {
		if (*pat == '*') { for (pat++; ; s++) { if (glob(pat, s)) return 1; if (!*s) return 0; } }
		if (!*s || (*pat != '?' && *pat != *s)) return 0;
	}
	return !*s;
}
static const char *file_name(const Obj *o) {           /* "lib.a(member.o)" matches as "member.o"; paths by basename */
	const char *b = strrchr(o->path, '('), *slash = strrchr(o->path, '/');
	static char buf[256];
	if (b) { snprintf(buf, sizeof buf, "%s", b + 1); buf[strcspn(buf, ")")] = 0; return buf; }
	return slash ? slash + 1 : o->path;
}
static void take(Stmt *s, Item *it, Obj *o, int j) {
	if (s->discard) { o->sec_out[j] = &os_discard; return; }
	it->in = grow(it->in, it->nin, &it->cap, sizeof *it->in);
	it->in[it->nin++] = (InSec){ o, j };
	if (!s->os->first.obj) s->os->first = (InSec){ o, j };
	o->sec_out[j] = s->os;
}
static int match_one(Obj *o, int j) {
	const char *name = sec_name(o, j), *file = file_name(o);
	for (int i = 0; i < nst; i++) {
		if (st[i].kind != S_OUTSEC) continue;
		for (int k = 0; k < st[i].nitem; k++) {
			Item *it = &st[i].items[k];
			if (it->kind != I_INPUT || !glob(it->file, file)) continue;
			for (int q = 0; q < it->npat; q++) if (glob(it->pats[q], name)) { take(&st[i], it, o, j); return 1; }
		}
	}
	return 0;
}
/* Does a KEEP() pattern anywhere in the script name this input section? (GNU lang_gc_sections_1: every statement on
 * its own — whichever one the section ends up in, even /DISCARD/.) */
int script_keeps(Obj *o, int j) {
	const char *name = sec_name(o, j), *file = file_name(o);
	for (int i = 0; i < nst; i++) {
		if (st[i].kind != S_OUTSEC) continue;
		for (int k = 0; k < st[i].nitem; k++) {
			Item *it = &st[i].items[k];
			if (it->kind != I_INPUT || !it->keep || !glob(it->file, file)) continue;
			for (int q = 0; q < it->npat; q++) if (glob(it->pats[q], name)) return 1;
		}
	}
	return 0;
}
/* --section-start=NAME=ADDR, -Ttext/-Tdata/-Tbss with a -T script: the output section's address, over the script's
 * own (GNU: the address statement sets its addr_tree). */
void section_start(const char *name, u32 addr) {
	for (int i = 0; i < nst; i++)
		if (st[i].kind == S_OUTSEC && !st[i].discard && !strcmp(st[i].os->name, name)) { st[i].addr = synth("%#x", addr); return; }
	die("--section-start %s: the script has no output section of that name (a new one: unsupported)", name);
}
/* NON-ALLOCATED sections are placeable too. Requiring SHF_ALLOC here meant they never reached match_one OR
 * place_orphan, so every one of them was dropped from the output even when the script named it explicitly:
 * a `gcc -c -g` object carrying .comment, .ARM.attributes and nine .debug_* sections linked to a file with
 * .text and .data and nothing else, where GNU ld keeps all eleven. That silently cost three things at once —
 * the producer identity in .comment, the float-ABI attributes a consumer reads, and any possibility of debug
 * information surviving a link. They take file space but no memory, so they get no address and no PT_LOAD
 * (see sec_bflags: no O_ALLOC, hence no O_LOAD). */
static int placeable(Obj *o, int j) {
	static int attrs_seen;                              /* one link per process, so never reset */
	Elf32_Shdr *s = &o->sh[j];
	if (o->sec_out[j]) return 0;
	/* .ARM.attributes must be MERGED, tag by tag, not concatenated: two sections run together are not a valid
	 * attribute section at all (GNU readelf: "Bad attribute length"). Until the merge exists, keep the FIRST
	 * one and drop the rest, exactly as a relocatable link already does (reloc.c:73). The result is a valid
	 * section describing the first input rather than the whole link — a declared divergence from GNU, which
	 * merges, and the reason the float-ABI check cannot be built on this yet. */
	if (s->sh_type == SHT_ARM_ATTRIBUTES) { if (attrs_seen) return 0; attrs_seen = 1; }
	/* An INPUT's symbol/string/relocation tables are the linker's business, not output content — it builds its
	 * own. The test is on the type AND on being non-allocated, because the sections the linker generates for a
	 * dynamic image have these same types and DO get placed: .dynsym is SHT_SYMTAB-like, .dynstr is SHT_STRTAB,
	 * .rel.dyn is SHT_REL, and all three are SHF_ALLOC. Excluding by type alone dropped .dynstr and .rel.dyn
	 * from every PIE, which left .dynsym/.dynamic with sh_link = 0 and no dynamic relocations at all. */
	if (!(s->sh_flags & SHF_ALLOC)
	    && (s->sh_type == SHT_NULL || s->sh_type == SHT_SYMTAB || s->sh_type == SHT_STRTAB
	        || s->sh_type == SHT_REL || s->sh_type == SHT_RELA)) return 0;
	if (o == linker_obj && !s->sh_size) return 0;       /* a linker table this link doesn't need */
	return 1;
}
void layout_match(Obj *o) {
	for (int j = 1; j < o->nsh; j++)
		if (placeable(o, j) && !(o->sh[j].sh_flags & SHF_LINK_ORDER)) match_one(o, j);
	for (int j = 1; j < o->nsh; j++)                    /* .ARM.exidx follows its code: discarded with it */
		if (placeable(o, j) && (o->sh[j].sh_flags & SHF_LINK_ORDER)) {
			if (o->sh[j].sh_link && o->sec_out[o->sh[j].sh_link] == &os_discard) o->sec_out[j] = &os_discard;
			else match_one(o, j);
		}
}

/* An orphan, as GNU ld places it (ldelf_place_orphan): it joins the output section of its name; else a new one goes
 * after the statement GNU anchors its kind to — notes after .interp, NOBITS after .bss, TLS after .tdata, writable
 * data after .data, relocations where output_rel_find puts them, read-only data after .rodata, code after .text — even an empty
 * one, behind the orphans already put there; with no such statement, after the last non-empty output section whose
 * flags match (lang_output_section_find_by_flags), else first. */
enum { O_CONTENTS = 1, O_ALLOC = 2, O_LOAD = 4, O_READONLY = 8, O_CODE = 16, O_TLS = 32 };
static int sec_bflags(const Elf32_Shdr *h) {
	int f = 0;
	if (h->sh_type != SHT_NOBITS) f |= O_CONTENTS;
	if (h->sh_flags & SHF_ALLOC) { f |= O_ALLOC; if (h->sh_type != SHT_NOBITS) f |= O_LOAD; }
	if (!(h->sh_flags & SHF_WRITE)) f |= O_READONLY;
	if (h->sh_flags & SHF_EXECINSTR) f |= O_CODE;
	if (h->sh_flags & SHF_TLS) f |= O_TLS;
	return f;
}
static int stmt_bflags(const Stmt *s) {   /* its inputs' (the first one's, as a BFD output section's), -1: none yet */
	for (int k = 0; k < s->nitem; k++) if (s->items[k].nin) return sec_bflags(&s->items[k].in[0].obj->sh[s->items[k].in[0].shndx]);
	return -1;
}
static const char *const anchor_name[] = { ".interp", ".bss", ".tdata", ".data", NULL /* rel_after */, ".rodata", ".text" };
static const char *anchor_last[7];   /* the orphan last put after each anchor (by name: statements move) */
static int stmt_named(const char *n) { for (int i = 0; i < nst; i++) if (st[i].kind == S_OUTSEC && !st[i].discard && !strcmp(st[i].os->name, n)) return i; return -1; }
static int by_flags(int f) {   /* lang_output_section_find_by_flags: the last statement with these flags, then looser */
	static const int masks[] = { O_CONTENTS | O_ALLOC | O_LOAD | O_READONLY | O_CODE | O_TLS };
	int found = -1;
	for (int i = 0; i < nst; i++) { if (st[i].kind != S_OUTSEC || st[i].discard) continue; int g = stmt_bflags(&st[i]); if (g >= 0 && !((g ^ f) & masks[0])) found = i; }
	if (found >= 0) return found;
	int m = (f & O_CODE) && (f & O_ALLOC) ? O_CONTENTS | O_ALLOC | O_LOAD | O_CODE | O_TLS
	      : (f & O_READONLY) && (f & O_ALLOC) ? O_CONTENTS | O_ALLOC | O_LOAD | O_READONLY | O_TLS
	      : (f & O_TLS) && (f & O_ALLOC) ? O_CONTENTS | O_ALLOC | O_LOAD | O_TLS
	      : (f & O_CONTENTS) && (f & O_ALLOC) ? O_CONTENTS | O_ALLOC | O_LOAD | O_TLS
	      : (f & O_ALLOC) ? O_ALLOC : 0;
	for (int i = 0; i < nst; i++) { if (st[i].kind != S_OUTSEC || st[i].discard) continue; int g = stmt_bflags(&st[i]); if (g >= 0 && !((g ^ f) & m)) found = i; }
	return found;
}
/* GNU output_rel_find: where a relocation section orphan goes with no anchor yet — a dynamic one (-z combreloc: a
 * .rel.dyn) before every other .rel statement, after the last allocated (read-only first) one ahead of them; another
 * after the last .rel statement short of .rel.plt (allocated first), else the same fallbacks. */
static int rel_after(int isdyn) {
	int last = -1, last_alloc = -1, last_ro_alloc = -1, last_rel = -1, last_rel_alloc = -1;
	for (int i = 0; i < nst; i++) {
		if (st[i].kind != S_OUTSEC || st[i].discard) continue;
		const char *n = st[i].os->name; int g = stmt_bflags(&st[i]);
		if (!strncmp(n, ".rel", 4)) {
			int lookrela = n[4] == 'a';
			if (isdyn || !strcmp(n + 4 + lookrela, ".plt")) break;
			if (!lookrela || last_rel < 0) last_rel = i;
			if ((!lookrela || last_rel_alloc < 0) && g >= 0 && (g & O_ALLOC)) last_rel_alloc = i;
		}
		last = i;
		if (g >= 0 && (g & O_ALLOC)) { last_alloc = i; if (g & O_READONLY) last_ro_alloc = i; }
	}
	return last_rel_alloc >= 0 ? last_rel_alloc : last_rel >= 0 ? last_rel : last_ro_alloc >= 0 ? last_ro_alloc : last_alloc >= 0 ? last_alloc : last;
}
static void place_orphan(Obj *o, int j) {
	const char *name = sec_name(o, j);
	const Elf32_Shdr *h = &o->sh[j];
	int isdyn = combreloc && h->sh_type == SHT_REL && (h->sh_flags & SHF_ALLOC);
	if (isdyn) name = ".rel.dyn";                       /* (GNU: every dynamic relocation orphan joins one .rel.dyn) */
	int at = stmt_named(name);
	if (at < 0) {
		int f = sec_bflags(h), a;
		a = (f & O_LOAD) && h->sh_type == 7 /* NOTE */ ? 0 : !(f & (O_LOAD | O_CONTENTS | O_TLS)) ? 1 : f & O_TLS ? 2 : !(f & O_READONLY) ? 3
		  : (f & O_LOAD) && h->sh_type == SHT_REL ? 4 : !(f & O_CODE) ? 5 : 6;
		int after = anchor_last[a] ? stmt_named(anchor_last[a]) : -1;
		if (after < 0) after = a == 4 ? rel_after(isdyn) : stmt_named(anchor_name[a]);
		if (after < 0 && a != 4) after = by_flags(f);
		at = after + 1;                                 /* (none: first) */
		new_stmt(S_OUTSEC);                             /* make room, then shift the tail down by one */
		memmove(&st[at + 1], &st[at], (nst - 1 - at) * sizeof *st);
		Stmt *s = &st[at]; memset(s, 0, sizeof *s);
		s->kind = S_OUTSEC; s->addr.lo = s->at.lo = s->align.lo = -1; s->vregion = s->lregion = -1;
		s->os = calloc(1, sizeof *s->os); s->os->name = name;
		anchor_last[a] = name;
	}
	Stmt *s = &st[at];
	Item *it = NULL;
	for (int k = s->nitem - 1; k >= 0 && !it; k--) if (s->items[k].kind == I_INPUT) it = &s->items[k];
	if (!it) { it = new_item(s, I_INPUT); it->file = "*"; }
	take(s, it, o, j);
}

/* ---- expressions ---------------------------------------------------------------------------------------- */
static int ep, eend; static u32 edot;
static u32 e_cond(void);
static const char *etok(void) { return ep < eend ? tok[ep] : ""; }
static int eis(const char *t) { return !strcmp(etok(), t); }
static void eexpect(const char *t) { if (!eis(t)) die("linker script %s: expected '%s' in expression, found '%s'", script_name, t, etok()); ep++; }
static int lo_forward;   /* this pass used an address from a section laid out after it (a link-order sort, a symbol) */
static int align_power(const InSec *a) { u32 v = a->obj->sh[a->shndx].sh_addralign, n = 0; while (v > 1) v >>= 1, n++; return (int)n; }
static int sort_after(int kind, const InSec *x, const InSec *y) {   /* x must follow y (GNU compare_section > 0) */
	int byname = strcmp(sec_name(x->obj, x->shndx), sec_name(y->obj, y->shndx)), byalign = align_power(y) - align_power(x);
	switch (kind) {
	case S_NAME: return byname > 0;
	case S_ALIGN: return byalign > 0;
	case S_NAME_ALIGN: return byname ? byname > 0 : byalign > 0;
	case S_ALIGN_NAME: return byalign ? byalign > 0 : byname > 0;
	default: return 0;
	}
}
static OutSec *placed_outsec(const char *name, const char *fn) {
	for (int i = 0; i < nst; i++)
		if (st[i].kind == S_OUTSEC && !st[i].discard && !strcmp(st[i].os->name, name)) {
			if (!st[i].os->done) lo_forward = 1;   /* laid out later: the previous pass's, then a pass more (as GNU's lazy values) */
			return st[i].os;
		}
	die("linker script %s: %s(%s): no such output section", script_name, fn, name);
}
static u32 align_to(u32 x, u32 a) { return a > 1 ? (x + a - 1) / a * a : x; }
/* The arm of a ?: that was NOT taken still has to be PARSED (one pass over the tokens), but its symbols need
 * not exist — GNU's exp_fold_tree folds the condition and then only the taken branch. `LOAD_ADDR =
 * DEFINED(LOAD_ADDR) ? LOAD_ADDR : 0x41000000;` (our kernel's script) is the whole point: the reference in the
 * untaken arm is undefined by construction. */
static int e_skip;
static u32 sym_value(const char *name) {
	GSym *g = gsym_find(name);
	if ((!g || !g->defined) && e_skip) return 0;
	if (!g || !g->defined) die("linker script %s: undefined symbol '%s' in an expression", script_name, name);
	if (g->obj) {
		Elf32_Sym *s = &g->obj->sym[g->symidx];
		if (s->st_shndx != SHN_ABS && (!g->obj->sec_out[s->st_shndx] || g->obj->sec_out[s->st_shndx] == &os_discard))
			die("linker script %s: '%s' is in no output section", script_name, name);
		if (s->st_shndx != SHN_ABS && !g->obj->sec_out[s->st_shndx]->done) lo_forward = 1;   /* laid out later (GNU allows it): the
		                                                                                     * previous pass's address, then a pass more */
		return sym_addr(g->obj, g->symidx);
	}
	return g->vaddr;
}
static u32 e_prim(void) {
	const char *t = etok(); ep++;
	if (!*t) die("linker script %s: truncated expression", script_name);
	if (!strcmp(t, "(")) { u32 v = e_cond(); eexpect(")"); return v; }
	if (!strcmp(t, "-")) return -e_prim();
	if (!strcmp(t, "~")) return ~e_prim();
	if (!strcmp(t, "!")) return !e_prim();
	if (!strcmp(t, ".")) return edot;
	if (!strcmp(t, "SIZEOF_HEADERS")) return hdrsz;
	int neg = t[0] == '-' && t[1] >= '0' && t[1] <= '9';   /* `-2`: one token here ('-' is a name character), a negative number */
	if ((t[0] >= '0' && t[0] <= '9') || neg) {
		char *end; unsigned long v = strtoul(t + neg, &end, 0);
		if (*end == 'K' || *end == 'k') v <<= 10, end++; else if (*end == 'M' || *end == 'm') v <<= 20, end++;
		if (*end) die("linker script %s: bad number '%s'", script_name, t);
		return neg ? -(u32)v : (u32)v;
	}
	if (eis("(")) {                                     /* a builtin function */
		ep++;
		u32 v = 0;
		if (!strcmp(t, "ALIGN")) { u32 a = e_cond(); if (eis(",")) { ep++; v = align_to(a, e_cond()); } else v = align_to(edot, a); }
		else if (!strcmp(t, "ABSOLUTE")) v = e_cond();
		else if (!strcmp(t, "MAX") || !strcmp(t, "MIN")) { u32 a = e_cond(); eexpect(","); u32 b = e_cond(); v = (!strcmp(t, "MAX")) == (a > b) ? a : b; }
		else if (!strcmp(t, "DEFINED")) { GSym *g = gsym_find(etok()); ep++; v = g && g->defined; }
		else if (!strcmp(t, "CONSTANT")) { ep++; v = PAGE; }   /* MAXPAGESIZE / COMMONPAGESIZE */
		else if (!strcmp(t, "ORIGIN") || !strcmp(t, "LENGTH")) { int r = region_find(etok()); ep++; v = !strcmp(t, "ORIGIN") ? regions[r].origin : regions[r].length; }
		else if (!strcmp(t, "ADDR") || !strcmp(t, "SIZEOF") || !strcmp(t, "LOADADDR")) {
			OutSec *os = placed_outsec(etok(), t); ep++;
			v = !strcmp(t, "ADDR") ? os->vaddr : !strcmp(t, "SIZEOF") ? os->size : os->lma;
		} else die("linker script %s: unknown function %s()", script_name, t);
		eexpect(")");
		return v;
	}
	return sym_value(t);
}
static int prec(const char *o) {
	static const struct { const char *op; int prec; } ops[] = {
		{ "*", 10 }, { "/", 10 }, { "%", 10 }, { "+", 9 }, { "-", 9 }, { "<<", 8 }, { ">>", 8 },
		{ "<", 7 }, { ">", 7 }, { "<=", 7 }, { ">=", 7 }, { "==", 6 }, { "!=", 6 }, { "&", 5 }, { "^", 4 }, { "|", 3 },
		{ "&&", 2 }, { "||", 1 }, { 0, 0 } };
	for (int i = 0; ops[i].op; i++) if (!strcmp(o, ops[i].op)) return ops[i].prec;
	return 0;
}
static u32 e_bin(int min) {
	u32 l = e_prim();
	for (int pr; (pr = prec(etok())) >= min && pr; ) {
		const char *o = tok[ep++]; u32 r = e_bin(pr + 1);
		switch (o[0]) {
		case '*': l *= r; break;
		case '/': if (!r) die("linker script %s: division by zero", script_name); l /= r; break;
		case '%': if (!r) die("linker script %s: division by zero", script_name); l %= r; break;
		case '+': l += r; break;
		case '-': l -= r; break;
		case '<': l = o[1] == '<' ? l << r : o[1] == '=' ? l <= r : l < r; break;
		case '>': l = o[1] == '>' ? l >> r : o[1] == '=' ? l >= r : l > r; break;
		case '=': l = l == r; break;
		case '!': l = l != r; break;
		case '&': l = o[1] ? (l && r) : (l & r); break;
		case '|': l = o[1] ? (l || r) : (l | r); break;
		case '^': l ^= r; break;
		}
	}
	return l;
}
static u32 e_cond(void) {
	u32 c = e_bin(1);
	if (!eis("?")) return c;
	ep++;
	e_skip += !c; u32 a = e_cond(); e_skip -= !c;       /* parse both arms; only the taken one must resolve */
	eexpect(":");
	e_skip += !!c; u32 b = e_cond(); e_skip -= !!c;
	return c ? a : b;
}
static u32 eval(Expr e, u32 dot) {
	ep = e.lo; eend = e.hi; edot = dot;
	u32 v = e_cond();
	if (ep != eend) die("linker script %s: unexpected '%s' in expression", script_name, etok());
	return v;
}

/* ---- addresses ------------------------------------------------------------------------------------------- */
static OutSec *last_os;                                 /* the output section a top-level symbol follows */
static void assign(Assign *a, u32 *dot, OutSec *os) {
	if (!a->live) return;
	u32 v = eval(a->e, *dot);
	if (!strcmp(a->sym, ".")) {
		if (os && v < *dot) die("linker script %s: '. = ...' moves backwards inside %s", script_name, os->name);
		*dot = v;
		return;
	}
	GSym *g = gsym_find(a->sym);
	g->defined = 1; g->vaddr = v; g->obj = NULL; g->os = os ? os : last_os;
}
static void lay_out(Stmt *s, u32 *dot) {
	OutSec *os = s->os;
	u32 align = 1;
	for (int k = 0; k < s->nitem; k++) for (int i = 0; i < s->items[k].nin; i++) {
		u32 a = s->items[k].in[i].obj->sh[s->items[k].in[i].shndx].sh_addralign;
		if (a > align) align = a;
	}
	u32 explicit_align = s->align.lo >= 0 ? eval(s->align, *dot) : 1;
	if (explicit_align > align) align = explicit_align;
	/* A NON-ALLOCATED output section (.comment, .ARM.attributes, every .debug_*) takes file space but no
	 * memory: GNU gives it vma 0 and it must not disturb the allocated layout. So lay it out against a counter
	 * of its own, starting at 0, and leave the location counter alone at the end. (GNU's default script writes
	 * `.comment 0 :` to the same effect, but not depending on the script means an orphan lands correctly too.) */
	u32 nalloc_dot = 0;
	int nonalloc = !(os->flags & SHF_ALLOC);
	u32 *vc = nonalloc ? &nalloc_dot : s->vregion >= 0 ? &regions[s->vregion].cur : dot, before = *vc;
	if (s->addr.lo >= 0) *vc = align_to(eval(s->addr, *dot), explicit_align);   /* (GNU: an address given, only ALIGN() moves it —
	                                                                          * the inputs align inside) */
	else *vc = align_to(*vc, align);
	os->vaddr = *vc;
	os->lma = s->at.lo >= 0 ? eval(s->at, *dot) : s->lregion >= 0 ? align_to(regions[s->lregion].cur, align) : os->vaddr;
	u32 delta = os->lma - os->vaddr;
	for (int k = 0; k < s->nitem; k++) {
		Item *it = &s->items[k];
		if (it->kind == I_ASSIGN) { assign(&it->a, vc, os); continue; }
		if (it->kind == I_DATA) { it->dva = *vc; *vc += (u32)it->dsize; continue; }
		for (int i = 0; i < it->nin; i++) {             /* a link-order section sorts by its linked section's address */
			Obj *o = it->in[i].obj; int j = it->in[i].shndx;
			if (!(o->sh[j].sh_flags & SHF_LINK_ORDER)) continue;
			int l = (int)o->sh[j].sh_link;
			if (!l || !o->sec_out[l] || o->sec_out[l] == &os_discard) die("%s: %s describes no placed section", o->path, sec_name(o, j));
			if (!o->sec_out[l]->done) lo_forward = 1;   /* laid out later (GNU allows it): the previous pass's address sorts it */
		}
		for (int i = 1; i < it->nin; i++) for (int b = i; b > 0; b--) {   /* stable insertion sort */
			InSec x = it->in[b - 1], y = it->in[b];
			int swap;
			if (x.obj->sh[x.shndx].sh_flags & SHF_LINK_ORDER)
				swap = x.obj->sec_vaddr[x.obj->sh[x.shndx].sh_link] > y.obj->sec_vaddr[y.obj->sh[y.shndx].sh_link];
			else swap = sort_after(it->sort, &x, &y);
			if (!swap) break;
			it->in[b - 1] = y; it->in[b] = x;
		}
		for (int i = 0; i < it->nin; i++) {
			Obj *o = it->in[i].obj; int j = it->in[i].shndx;
			*vc = align_to(*vc, o->sh[j].sh_addralign);
			o->sec_vaddr[j] = *vc; o->sec_lma[j] = *vc + delta;
			*vc += o->sh[j].sh_size;
		}
	}
	os->size = *vc - os->vaddr;
	if (s->lregion >= 0) regions[s->lregion].cur = os->lma + os->size;
	for (int r = 0; r < 2; r++) {                       /* a MEMORY region must hold what's placed in it */
		int reg = r ? s->lregion : s->vregion; u32 end = (r ? os->lma : os->vaddr) + os->size;
		if (reg >= 0 && regions[reg].length && end > regions[reg].origin + regions[reg].length)
			die("section %s overflows MEMORY region %s by %u bytes", os->name, regions[reg].name, end - regions[reg].origin - regions[reg].length);
	}
	if (tls_bss(os)) *vc = before;                      /* .tbss is a template's zero tail: the next section reuses its addresses */
	if (!nonalloc) *dot = *vc;                          /* a non-alloc section never moves the location counter */
	os->done = 1;
	/* Only an ALLOCATED section can be "the section a top-level symbol follows": a symbol's value is an address
	 * in allocated space, so attributing it to .ARM.attributes or a .debug_* is meaningless. GNU puts `_end`
	 * in the last allocated section; letting a non-alloc one become last_os moved _end/__end__/_bss_end__ there
	 * and nm reported them as 'N' instead of 'D'. */
	if (!nonalloc) last_os = os;
}
static void assign_addresses(void) {
	u32 dot = 0;
	last_os = NULL;
	for (int r = 0; r < nregion; r++) regions[r].cur = regions[r].origin;
	for (int i = 0; i < ndeclared; i++) declared[i]->defined = 0;   /* DEFINED() sees only earlier assignments */
	for (int i = 0; i < nst; i++) if (st[i].kind == S_OUTSEC && !st[i].discard) st[i].os->done = 0;
	for (int i = 0; i < nst; i++) {
		Stmt *s = &st[i];
		if (s->kind == S_ASSIGN) assign(&s->a, &dot, NULL);
		else if (s->kind == S_ASSERT) s->ok = eval(s->a.e, dot) != 0;
		else if (!s->discard) lay_out(s, &dot);
	}
}

/* ---- segments + program headers -------------------------------------------------------------------------- */
static void add_phdr(Elf32_Phdr ph) { phdrs = grow(phdrs, nphdr, &phcap, sizeof *phdrs); phdrs[nphdr++] = ph; }
static int piece(int l, Elf32_Phdr *ph, u32 type) {    /* a phdr covering linker section l, if present */
	OutSec *os = linker_obj->sec_out[l];
	if (!os || os == &os_discard || !linker_obj->sh[l].sh_size) return 0;
	u32 va = linker_obj->sec_vaddr[l];
	*ph = (Elf32_Phdr){ type, os->off + (va - os->vaddr), va, linker_obj->sec_lma[l], linker_obj->sh[l].sh_size,
	                    linker_obj->sh[l].sh_size, segs[os->seg].flags & (PF_R | PF_W), 4 };
	return 1;
}
/* The output sections in no segment (not allocated): file bytes after the loaded ones, each at its alignment. */
static void nonalloc_offsets(void) {
	u32 fend = hdrsz;
	for (int i = 0; i < noutsec; i++) if ((outsecs[i]->flags & SHF_ALLOC) && outsecs[i]->type != SHT_NOBITS && outsecs[i]->off + outsecs[i]->size > fend)
		fend = outsecs[i]->off + outsecs[i]->size;
	for (int i = 0; i < noutsec; i++) {
		OutSec *os = outsecs[i]; u32 align = 1;
		if (os->flags & SHF_ALLOC) continue;
		for (int k = 0; k < nobj; k++) for (int j = 1; j < objs[k]->nsh; j++)
			if (objs[k]->sec_out && objs[k]->sec_out[j] == os && objs[k]->sh[j].sh_addralign > align) align = objs[k]->sh[j].sh_addralign;
		os->off = alignup(fend, align); fend = os->off + os->size;
	}
}
static void build_segments(void) {
	nseg = 0; noutsec = 0; headers_mapped = 0;
	for (int i = 0; i < nst; i++) {
		if (st[i].kind != S_OUTSEC || st[i].discard || !st[i].os->size) continue;
		OutSec *os = st[i].os;
		if (tls_bss(os) || !(os->flags & SHF_ALLOC)) {   /* in no PT_LOAD (TLS bss: only in PT_TLS); a header all the same */
			os->seg = nseg ? nseg - 1 : 0;
			outsecs = grow(outsecs, noutsec, &outcap, sizeof *outsecs);
			outsecs[noutsec++] = os; os->index = noutsec;
			continue;
		}
		int prog = os->type != SHT_NOBITS;
		Seg *g = nseg ? &segs[nseg - 1] : NULL;
		u32 end = g ? g->vaddr + g->memsz : 0;
		int fresh = !g
		    || (prog && g->memsz > g->filesz)                              /* file bytes can't follow a .bss tail */
		    || (prog && os->lma - os->vaddr != g->lma - g->vaddr)          /* a different load delta             */
		    || os->vaddr < end                                             /* goes backwards                     */
		    || align_to(end, PAGE) < (os->vaddr & ~(PAGE - 1))             /* a page gap                         */
		    || (!(g->flags & PF_W) && (os->flags & SHF_WRITE) && ((end - 1) & ~(PAGE - 1)) != (os->vaddr & ~(PAGE - 1)));   /* W^X */
		if (fresh) {
			segs = grow(segs, nseg, &segcap, sizeof *segs);
			g = &segs[nseg++]; *g = (Seg){ .vaddr = os->vaddr, .lma = prog ? os->lma : os->vaddr, .flags = PF_R };
		}
		g->memsz = os->vaddr + os->size - g->vaddr;
		if (prog) g->filesz = g->memsz;
		if (os->flags & SHF_WRITE) g->flags |= PF_W;
		if (os->flags & SHF_EXECINSTR) g->flags |= PF_X;
		os->seg = nseg - 1;
		outsecs = grow(outsecs, noutsec, &outcap, sizeof *outsecs);
		outsecs[noutsec++] = os; os->index = noutsec;
	}
	if (nseg && segs[0].filesz && segs[0].lma == segs[0].vaddr && (segs[0].vaddr & (PAGE - 1)) >= hdrsz) {
		u32 below = segs[0].vaddr & (PAGE - 1);          /* room before the first section: the headers map there */
		segs[0].vaddr -= below; segs[0].lma -= below; segs[0].filesz += below; segs[0].memsz += below;
		headers_mapped = 1;
	}
	u32 fend = hdrsz;                                   /* file offsets: in order, each congruent to its address */
	for (int i = 0; i < nseg; i++) {
		segs[i].off = (i == 0 && headers_mapped) ? 0 : fend + ((segs[i].vaddr - fend) & (PAGE - 1));
		if (segs[i].filesz) fend = segs[i].off + segs[i].filesz;
	}
	for (int i = 0; i < noutsec; i++) outsecs[i]->off = nseg ? segs[outsecs[i]->seg].off + (outsecs[i]->vaddr - segs[outsecs[i]->seg].vaddr) : hdrsz;
	nonalloc_offsets();

	nphdr = 0;                                          /* the program header table: the ONE list the writer emits */
	Elf32_Phdr ph;
	int interp = piece(L_INTERP, &ph, PT_INTERP);
	if (interp && headers_mapped) {
		u32 n = (hdrsz - sizeof(Elf32_Ehdr));
		add_phdr((Elf32_Phdr){ PT_PHDR, sizeof(Elf32_Ehdr), segs[0].vaddr + sizeof(Elf32_Ehdr), segs[0].lma + sizeof(Elf32_Ehdr), n, n, PF_R, 4 });
	}
	if (interp) { ph.p_align = 1; add_phdr(ph); }
	for (int i = 0; i < nseg; i++)
		add_phdr((Elf32_Phdr){ PT_LOAD, segs[i].off, segs[i].vaddr, segs[i].lma, segs[i].filesz, segs[i].memsz, segs[i].flags, PAGE });
	if (piece(L_DYNAMIC, &ph, PT_DYNAMIC)) add_phdr(ph);
	OutSec *t0 = NULL, *t1 = NULL, *td = NULL;           /* PT_TLS: the template, .tdata (file bytes) then .tbss */
	tls_vaddr = tls_align = 0;
	for (int i = 0; i < noutsec; i++) if (outsecs[i]->flags & SHF_TLS) { if (!t0) t0 = outsecs[i]; t1 = outsecs[i]; if (!tls_bss(outsecs[i])) td = outsecs[i]; }
	if (t0) {
		for (int i = 0; i < nobj; i++) for (int j = 1; j < objs[i]->nsh; j++)
			if (objs[i]->sec_out && objs[i]->sec_out[j] && (objs[i]->sec_out[j]->flags & SHF_TLS) && objs[i]->sh[j].sh_addralign > tls_align)
				tls_align = objs[i]->sh[j].sh_addralign;
		if (!tls_align) tls_align = 1;
		tls_vaddr = t0->vaddr;
		u32 filesz = td ? td->vaddr + td->size - t0->vaddr : 0, memsz = t1->vaddr + t1->size - t0->vaddr;
		add_phdr((Elf32_Phdr){ PT_TLS, t0->off, t0->vaddr, t0->lma, filesz, memsz, PF_R, tls_align });
	}
	OutSec *ex = outsec_named(".ARM.exidx");
	if (ex) add_phdr((Elf32_Phdr){ PT_ARM_EXIDX, ex->off, ex->vaddr, ex->lma, ex->size, ex->size, PF_R, 4 });
	if (gnu_stack) add_phdr((Elf32_Phdr){ PT_GNU_STACK, 0, 0, 0, 0, 0, (u32)gnu_stack, 16 });
	OutSec *r0 = NULL, *r1 = NULL;                       /* RELRO: from the first to the last relro section present */
	for (int i = 0; relro && relro_secs[i]; i++) { OutSec *os = outsec_named(relro_secs[i]); if (os) { if (!r0) r0 = os; r1 = os; } }
	if (r0) {
		u32 end = align_to(r1->vaddr + r1->size, PAGE);  /* the script ended it on a page boundary */
		add_phdr((Elf32_Phdr){ PT_GNU_RELRO, r0->off, r0->vaddr, r0->lma, end - r0->vaddr, end - r0->vaddr, PF_R, 1 });
	}
}
/* With PHDRS: exactly the listed program headers (+ PT_ARM_EXIDX, which GNU's ARM backend adds first when there is
 * an .ARM.exidx and no PHDRS line for it). Each allocated section goes in the segments of its `:NAME`s — or the
 * previous section's — and must be in exactly one PT_LOAD; a non-LOAD header spans its sections (none: all zero). */
static u32 phmask_of(Stmt *s) {
	u32 m = 0;
	for (int k = 0; k < s->nphname; k++) {
		if (!strcmp(s->phname[k], "NONE")) continue;
		int d; for (d = 0; d < nphdef; d++) if (!strcmp(phdef[d].name, s->phname[k])) break;
		if (d == nphdef) die("linker script %s: section %s: no PHDRS line named %s", script_name, s->os->name, s->phname[k]);
		m |= 1u << d;
	}
	return m;
}
static void build_segments_phdrs(void) {
	nseg = 0; noutsec = 0; headers_mapped = 0;
	u32 cur = 0, *mask = calloc(nst + 1, sizeof *mask);
	int *seg_of = calloc(nphdef, sizeof *seg_of);
	for (int d = 0; d < nphdef; d++) {
		seg_of[d] = -1;
		if (phdef[d].type != PT_LOAD) continue;
		segs = grow(segs, nseg, &segcap, sizeof *segs); segs[nseg] = (Seg){ 0 }; seg_of[d] = nseg++;
	}
	int *used = calloc(nseg + 1, sizeof *used);
	for (int i = 0; i < nst; i++)   /* sections before the first `:phdr` take ITS headers (GNU lang_record_phdrs) */
		if (st[i].kind == S_OUTSEC && !st[i].discard && st[i].nphname) { cur = phmask_of(&st[i]); break; }
	for (int i = 0; i < nst; i++) {
		if (st[i].kind != S_OUTSEC || st[i].discard) continue;
		if (st[i].nphname) cur = phmask_of(&st[i]);
		OutSec *os = st[i].os;
		mask[i] = os->flags & SHF_ALLOC ? cur : 0;
		if (!st[i].nphname) for (int d = 0; d < nphdef; d++) if (phdef[d].type == PT_INTERP) mask[i] &= ~(1u << d);   /* (GNU: not an inherited PT_INTERP) */
		if (!os->size) continue;
		outsecs = grow(outsecs, noutsec, &outcap, sizeof *outsecs);
		outsecs[noutsec++] = os; os->index = noutsec;
		if (!(os->flags & SHF_ALLOC)) continue;
		int load = -1;
		for (int d = 0; d < nphdef; d++) if (mask[i] & 1u << d && seg_of[d] >= 0) { if (load >= 0) die("section %s is in two PT_LOADs: unsupported", os->name); load = d; }
		if (load < 0) die("linker script %s: allocated section %s is in no PT_LOAD", script_name, os->name);
		Seg *g = &segs[seg_of[load]]; int prog = os->type != SHT_NOBITS;
		if (!used[seg_of[load]]++) { g->vaddr = os->vaddr; g->lma = prog ? os->lma : os->vaddr; g->flags = PF_R; }
		else if (os->vaddr < g->vaddr + g->memsz) die("section %s goes backwards in its segment", os->name);
		g->memsz = os->vaddr + os->size - g->vaddr;
		if (prog) g->filesz = g->memsz;
		if (os->flags & SHF_WRITE) g->flags |= PF_W;
		if (os->flags & SHF_EXECINSTR) g->flags |= PF_X;
		os->seg = seg_of[load];
	}
	for (int d = 0; d < nphdef; d++)   /* FILEHDR [PHDRS]: the segment starts at the ELF header */
		if (seg_of[d] >= 0 && phdef[d].filehdr) {
			Seg *g = &segs[seg_of[d]];
			if (seg_of[d] || (g->vaddr & (PAGE - 1)) < hdrsz) die("PHDRS %s: FILEHDR needs the first segment, with SIZEOF_HEADERS of room", phdef[d].name);
			u32 below = g->vaddr & (PAGE - 1);
			g->vaddr -= below; g->lma -= below; g->filesz += below; g->memsz += below; headers_mapped = 1;
		}
	u32 fend = hdrsz;
	for (int i = 0; i < nseg; i++) {
		segs[i].off = (i == 0 && headers_mapped) ? 0 : fend + ((segs[i].vaddr - fend) & (PAGE - 1));
		if (segs[i].filesz) fend = segs[i].off + segs[i].filesz;
	}
	for (int i = 0; i < noutsec; i++) outsecs[i]->off = outsecs[i]->flags & SHF_ALLOC ? segs[outsecs[i]->seg].off + (outsecs[i]->vaddr - segs[outsecs[i]->seg].vaddr) : hdrsz;
	nonalloc_offsets();

	nphdr = 0;
	OutSec *ex = outsec_named(".ARM.exidx"); int has_exidx = 0;
	for (int d = 0; d < nphdef; d++) has_exidx |= phdef[d].type == PT_ARM_EXIDX;
	if (ex && !has_exidx) add_phdr((Elf32_Phdr){ PT_ARM_EXIDX, ex->off, ex->vaddr, ex->lma, ex->size, ex->size, PF_R, 4 });
	for (int d = 0; d < nphdef; d++) {
		PhDef *pd = &phdef[d]; Elf32_Phdr ph = { .p_type = pd->type };
		if (seg_of[d] >= 0) { Seg *g = &segs[seg_of[d]]; ph = (Elf32_Phdr){ PT_LOAD, g->off, g->vaddr, g->lma, g->filesz, g->memsz, g->flags, PAGE }; }
		else if (pd->type == PT_PHDR) {
			u32 n = hdrsz - sizeof(Elf32_Ehdr);
			ph = (Elf32_Phdr){ PT_PHDR, sizeof(Elf32_Ehdr), segs[0].vaddr + sizeof(Elf32_Ehdr), segs[0].lma + sizeof(Elf32_Ehdr), n, n, PF_R, 4 };
		} else {
			OutSec *f = NULL, *l = NULL; u32 flags = 0, align = 4;
			for (int i = 0; i < nst; i++) {
				if (st[i].kind != S_OUTSEC || st[i].discard) continue;
				OutSec *os = st[i].os;
				if (!(mask[i] & 1u << d) || !os->size) continue;
				if (!f) f = os;
				l = os;
				flags |= PF_R | (os->flags & SHF_WRITE ? PF_W : 0) | (os->flags & SHF_EXECINSTR ? PF_X : 0);
			}
			if (f) {
				u32 fs = l->type == SHT_NOBITS ? l->off - f->off : l->off + l->size - f->off;
				ph = (Elf32_Phdr){ pd->type, f->off, f->vaddr, f->lma, fs, l->vaddr + l->size - f->vaddr, flags, align };
			} else ph.p_align = 4;
		}
		if (pd->flags.lo >= 0) ph.p_flags = eval(pd->flags, 0);
		if (pd->at.lo >= 0) ph.p_paddr = eval(pd->at, 0);
		add_phdr(ph);
	}
	tls_vaddr = tls_align = 0;
	free(mask); free(seg_of); free(used);
}
/* The data statements' bytes (little-endian), now that every value is known: into the output image `buf`. */
void layout_data(u8 *buf) {
	for (int i = 0; i < nst; i++) {
		if (st[i].kind != S_OUTSEC || st[i].discard || !st[i].os->index || st[i].os->type == SHT_NOBITS) continue;
		for (int k = 0; k < st[i].nitem; k++) {
			Item *it = &st[i].items[k]; if (it->kind != I_DATA) continue;
			u32 v = eval(it->de, it->dva); u8 *q = buf + st[i].os->off + (it->dva - st[i].os->vaddr);   /* (values are 32-bit here) */
			for (int b = 0; b < it->dsize; b++) q[b] = b < 4 ? (u8)(v >> (8 * b)) : it->dsigned && (v & 0x80000000u) ? 0xff : 0;
		}
	}
}
OutSec *outsec_named(const char *name) {
	for (int i = 0; i < noutsec; i++) if (!strcmp(outsecs[i]->name, name)) return outsecs[i];
	return NULL;
}

/* PT_GNU_STACK as GNU ld decides it: present when any input carries a .note.GNU-stack; executable when one of those
 * says so or an input has none (the ELF default is an executable stack). */
static void decide_stack(void) {
	int notes = 0, exec = 0;
	for (int i = 0; i < nobj; i++) {
		Obj *o = objs[i]; int has = 0;
		if (!o->active || o == linker_obj) continue;
		for (int j = 1; j < o->nsh; j++) if (!strcmp(sec_name(o, j), ".note.GNU-stack")) { has = 1; exec |= (o->sh[j].sh_flags & SHF_EXECINSTR) != 0; }
		notes |= has; exec |= !has;
	}
	gnu_stack = stack_override ? stack_override : notes ? PF_R | PF_W | (exec ? PF_X : 0) : 0;
}

/* Put section j of o (a veneer stub section) right after (after_o, after_j), in the same output section. */
void layout_insert_after(Obj *after_o, int after_j, Obj *o, int j) {
	for (int i = 0; i < nst; i++) {
		if (st[i].kind != S_OUTSEC || st[i].discard) continue;
		for (int k = 0; k < st[i].nitem; k++) {
			Item *it = &st[i].items[k];
			for (int n = 0; n < it->nin; n++) if (it->in[n].obj == after_o && it->in[n].shndx == after_j) {
				it->in = grow(it->in, it->nin, &it->cap, sizeof *it->in);
				memmove(&it->in[n + 2], &it->in[n + 1], (it->nin - n - 1) * sizeof *it->in); it->nin++;
				it->in[n + 1] = (InSec){ o, j }; o->sec_out[j] = st[i].os;
				return;
			}
		}
	}
	die("internal: %s(%s) is in no output section", after_o->path, sec_name(after_o, after_j));
}
void layout_run(void) {
	int unplaced = 0;
	for (int i = 0; i < nobj; i++) {   /* the orphans: --orphan-handling decides (GNU's messages) */
		Obj *o = objs[i];
		if (o->active) for (int j = 1; j < o->nsh; j++) {
			if (!placeable(o, j)) continue;
			const char *from = o == linker_obj ? "linker stubs" : o->path;
			if (orphan_mode == 1) { o->sec_out[j] = &os_discard; continue; }
			if (orphan_mode == 3) { fprintf(stderr, "ld: error: unplaced orphan section `%s' from `%s'\n", sec_name(o, j), from); unplaced = 1; continue; }
			if (orphan_mode == 2) fprintf(stderr, "ld: warning: orphan section `%s' from `%s' being placed in section `%s'\n", sec_name(o, j), from, sec_name(o, j));
			place_orphan(o, j);
		}
	}
	if (unplaced) die("unplaced orphan sections (--orphan-handling=error)");
	for (int i = 0; i < nst; i++) {                     /* each output section's type/flags from its inputs */
		if (st[i].kind != S_OUTSEC || st[i].discard) continue;
		OutSec *os = st[i].os; int first = 1, nobits = 1, has_data = 0;
		for (int k = 0; k < st[i].nitem; k++) {
			if (st[i].items[k].kind == I_DATA) has_data = 1;   /* BYTE/SHORT/LONG/QUAD: bytes of its own */
			for (int n = 0; n < st[i].items[k].nin; n++) {
				Elf32_Shdr *h = &st[i].items[k].in[n].obj->sh[st[i].items[k].in[n].shndx];
				os->flags |= h->sh_flags & (SHF_ALLOC | SHF_WRITE | SHF_EXECINSTR | SHF_TLS | SHF_LINK_ORDER);   /* (.ARM.exidx stays "AL", as GNU's) */
				nobits &= h->sh_type == SHT_NOBITS;
				if (first) { os->type = h->sh_type; os->entsize = h->sh_entsize; first = 0; }
				else { if (os->type != h->sh_type) os->type = SHT_PROGBITS; if (os->entsize != h->sh_entsize) os->entsize = 0; }
			}
		}
		/* No input section placed anything here. A data statement still gives the section CONTENTS, which GNU
		 * marks A/PROGBITS; only a bare `. = . + N` is reserved space (AW/NOBITS). Getting this wrong loses the
		 * bytes: `objcopy -O binary` rightly writes nothing for a NOBITS section, which is how the mainline
		 * zImage came out 4 bytes shorter than its own header said (.pad is `BYTE(0); . = ALIGN(8);`). */
		if (first) {                                /* no input section placed anything: the type comes from here */
			os->flags = has_data ? SHF_ALLOC : (SHF_ALLOC | SHF_WRITE);
			os->type = has_data ? SHT_PROGBITS : SHT_NOBITS;
		}
		if (has_data) nobits = 0;
		if (st[i].noalloc) { os->flags &= ~SHF_ALLOC; os->type = SHT_PROGBITS; }   /* (GNU: no ALLOC, so no NOBITS either — file zeros) */
		else if (st[i].noload || nobits) os->type = SHT_NOBITS;
		else if (os->type == SHT_NOBITS) os->type = SHT_PROGBITS;   /* .bss inside a PROGBITS section: file zeros */
	}
	decide_stack();
	dyn_index_sections();                               /* (.dynsym's size depends on the output's sections) */
	layout_addresses();
}
/* The output sections in statement order (= section-header order), for dyn_index_sections: the one after `os`. Only
 * those with bytes: GNU strips the empty ones (strip_excluded_output_sections) before it chooses. */
OutSec *script_next_outsec(OutSec *os) {
	int i = 0;
	if (os) { while (i < nst && !(st[i].kind == S_OUTSEC && st[i].os == os)) i++; i++; }
	for (; i < nst; i++) if (st[i].kind == S_OUTSEC && !st[i].discard) {
		int has = 0;
		for (int k = 0; k < st[i].nitem && !has; k++) {
			Item *it = &st[i].items[k];
			if (it->kind == I_DATA) has = 1;
			for (int n = 0; n < it->nin && !has; n++) has = it->in[n].obj->sh[it->in[n].shndx].sh_size > 0;
		}
		if (has) return st[i].os;
	}
	return NULL;
}
/* Addresses + segments (again after veneers grew): passes until the program header count settles; then the checks. */
void layout_addresses(void) {
	static int want = 2;                                /* program headers: a guess, refined until stable */
	u32 sig = 0, prev = 1;
	for (int pass = 0; ; pass++) {
		hdrsz = sizeof(Elf32_Ehdr) + (u32)want * sizeof(Elf32_Phdr);
		lo_forward = 0;
		assign_addresses();
		if (have_phdrs) build_segments_phdrs(); else build_segments();
		sig = 0; for (int i = 0; i < nst; i++) if (st[i].kind == S_OUTSEC && !st[i].discard) sig = sig * 31 + st[i].os->vaddr * 7 + st[i].os->size + st[i].os->lma;
		if (nphdr == want && (!lo_forward || sig == prev)) break;   /* a forward reference: again, until the layout stops moving */
		prev = sig;
		if (pass == 8) die("internal: the layout does not settle (program headers, or a forward reference)");
		want = nphdr;
	}
	for (int i = 0; i < nst; i++)
		if (st[i].kind == S_ASSERT && !st[i].ok) die("linker script %s: ASSERT failed: %s", script_name, st[i].a.sym);
	for (int i = 0; i < nseg && warn_rwx; i++)          /* as GNU ld: code and writable data sharing a page defeat W^X */
		if ((segs[i].flags & (PF_W | PF_X)) == (PF_W | PF_X))
			fprintf(stderr, "ld: warning: LOAD segment at %#x has RWX permissions (%s puts code and writable data on one page)\n",
			        segs[i].vaddr, script_name);
	for (int i = 0; i < noutsec; i++) for (int k = i + 1; k < noutsec; k++) {   /* sections may not share addresses */
		OutSec *a = outsecs[i], *b = outsecs[k];
		if (tls_bss(a) || tls_bss(b)) continue;         /* .tbss overlaps what follows it, by design */
		if (!(a->flags & SHF_ALLOC) || !(b->flags & SHF_ALLOC)) continue;   /* (GNU checks allocated sections only) */
		if (a->overlay && a->overlay == b->overlay) continue;   /* (an OVERLAY's, by design: their load addresses differ) */
		if (a->vaddr < b->vaddr + b->size && b->vaddr < a->vaddr + a->size)
			die("sections %s [%#x,%#x) and %s [%#x,%#x) overlap", a->name, a->vaddr, a->vaddr + a->size, b->name, b->vaddr, b->vaddr + b->size);
		if (a->type != SHT_NOBITS && b->type != SHT_NOBITS && a->lma < b->lma + b->size && b->lma < a->lma + a->size)
			die("sections %s and %s overlap at their load addresses [%#x,%#x) / [%#x,%#x)", a->name, b->name, a->lma, a->lma + a->size, b->lma, b->lma + b->size);
	}
}
