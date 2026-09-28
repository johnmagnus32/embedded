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
 * Not modelled (each an error, never silently ignored): OVERLAY, PHDRS, VERSION, INCLUDE, GROUP/INPUT, data
 * statements (BYTE/LONG/…), FILL, EXCLUDE_FILE, forward references to later sections, relocatable output.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ld.h"

OutSec os_discard;
OutSec **outsecs; int noutsec; static int outcap;
Seg *segs; int nseg; static int segcap;
Elf32_Phdr *phdrs; int nphdr; static int phcap;
u32 hdrsz;
static int headers_mapped, gnu_stack;
static int relro;                                         /* the default layout's RELRO region is on */
static const char *const relro_secs[] = { ".preinit_array", ".init_array", ".fini_array", ".data.rel.ro", ".got", 0 };
static const char *script_name;

/* ---- tokens -------------------------------------------------------------------------------------------- */
static char **tok; static int ntok, tokcap;
static int is_punct(char c) { return strchr("{}():;=+?,<>&|^%~!", c) != NULL; }   /* '*' '/' '-' are name chars:
                                                                                  * *(.text*), /DISCARD/, .note.GNU-stack */
static void push_tok(const char *b, int n) {
	tok = grow(tok, ntok, &tokcap, sizeof *tok);
	char *t = malloc(n + 1); memcpy(t, b, n); t[n] = 0; tok[ntok++] = t;
}
static void tokenize(const char *s) {
	static const char *const two[] = { "<<", ">>", "==", "!=", "<=", ">=", "&&", "||", "+=", "-=", 0 };
	while (*s) {
		if (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') { s++; continue; }
		if (s[0] == '/' && s[1] == '*') { s += 2; while (*s && !(s[0] == '*' && s[1] == '/')) s++; if (*s) s += 2; continue; }
		if (*s == '"') { const char *b = ++s; while (*s && *s != '"') s++; push_tok(b, (int)(s - b)); if (*s) s++; continue; }
		int k = 0; while (two[k] && strncmp(s, two[k], 2)) k++;
		if (two[k]) { push_tok(s, 2); s += 2; continue; }
		if (is_punct(*s)) { push_tok(s, 1); s++; continue; }
		const char *b = s;
		while (*s && !is_punct(*s) && *s != ' ' && *s != '\t' && *s != '\n' && *s != '\r' && *s != '"') s++;
		push_tok(b, (int)(s - b));
	}
}
static int p;                                          /* the parse cursor */
static const char *peek(int k) { return p + k < ntok ? tok[p + k] : ""; }
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
enum { I_INPUT, I_ASSIGN };
typedef struct {                                       /* an output-section body item */
	int kind;
	const char *file; char **pats; int npat, sort;     /* I_INPUT: file glob + section globs (SORT_BY_NAME)     */
	InSec *in; int nin, cap;                           /* the input sections it took                            */
	Assign a;                                          /* I_ASSIGN                                              */
} Item;
enum { S_ASSIGN, S_OUTSEC, S_ASSERT };
typedef struct {
	int kind;
	Assign a;                                          /* S_ASSIGN; S_ASSERT: a.e + a.sym = the message         */
	int ok;                                            /* S_ASSERT: held in the latest layout pass              */
	OutSec *os; int discard, noload;                   /* S_OUTSEC                                              */
	Item *items; int nitem, itemcap;
	Expr addr, at, align;                              /* `name addr :`, AT(lma), ALIGN(n)                      */
	int vregion, lregion;                              /* `> REGION`, `AT> REGION` (-1: none)                   */
} Stmt;
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
	die("linker script %s: '%s' is not supported (subset: no OVERLAY/PHDRS/VERSION/INCLUDE/GROUP/data statements/FILL)", script_name, what);
}
/* One input spec: FILEGLOB ( SECGLOB... ) with SORT_BY_NAME(...) inside, or KEEP( that ) around it. */
static void parse_input(Stmt *s) {
	int keep = is("KEEP");
	if (keep) { p++; expect("("); }
	Item *it = new_item(s, I_INPUT);
	it->file = tok[p++];
	if (!strcmp(it->file, "EXCLUDE_FILE")) unsupported("EXCLUDE_FILE");
	expect("(");
	while (!is(")")) {
		if (is("SORT") || is("SORT_BY_NAME")) {
			p++; expect("("); it->sort = 1;
			while (!is(")")) { it->pats = realloc(it->pats, (it->npat + 1) * sizeof *it->pats); it->pats[it->npat++] = tok[p++]; }
			expect(")");
			continue;
		}
		if (is("EXCLUDE_FILE") || is("SORT_BY_ALIGNMENT") || is("SORT_BY_INIT_PRIORITY") || is("SORT_NONE")) unsupported(peek(0));
		it->pats = realloc(it->pats, (it->npat + 1) * sizeof *it->pats); it->pats[it->npat++] = tok[p++];
	}
	expect(")");
	if (keep) expect(")");                              /* KEEP: we don't garbage-collect sections, so it's implied */
}
static void parse_outsec(void) {
	Stmt *s = new_stmt(S_OUTSEC);
	const char *name = tok[p++];
	s->discard = !strcmp(name, "/DISCARD/");
	if (!s->discard) { s->os = calloc(1, sizeof *s->os); s->os->name = name; }
	if (!is(":")) {                                     /* `name [addr] [(NOLOAD)] :` */
		int lo = p;
		while (p < ntok && !is(":")) {
			if (is("(") && !strcmp(peek(1), "NOLOAD") && !strcmp(peek(2), ")")) { s->noload = 1; break; }
			if (is("(") && (!strcmp(peek(1), "COPY") || !strcmp(peek(1), "INFO") || !strcmp(peek(1), "OVERLAY") || !strcmp(peek(1), "DSECT")))
				unsupported(peek(1));
			p++;
		}
		if (p > lo) s->addr = (Expr){ lo, p };
		if (s->noload) p += 3;
	}
	expect(":");
	while (!is("{")) {
		if (is("AT") && !strcmp(peek(1), "(")) { p += 2; s->at = parse_expr(); expect(")"); }
		else if (is("ALIGN") && !strcmp(peek(1), "(")) { p += 2; s->align = parse_expr(); expect(")"); }
		else unsupported(peek(0));
	}
	expect("{");
	while (!is("}")) {
		if (p >= ntok) die("linker script %s: unterminated output section '%s'", script_name, name);
		if (is(";")) { p++; continue; }
		if (at_assignment()) { Item *it = new_item(s, I_ASSIGN); parse_assignment(&it->a, 1); continue; }
		if (is("BYTE") || is("SHORT") || is("LONG") || is("QUAD") || is("SQUAD") || is("FILL") || is("CONSTRUCTORS") ||
		    is("CREATE_OBJECT_SYMBOLS") || is("INCLUDE") || is("ASSERT")) unsupported(peek(0));
		parse_input(s);
	}
	expect("}");
	for (;;) {
		if (is(">")) { p++; s->vregion = region_find(tok[p++]); }
		else if (is("AT") && !strcmp(peek(1), ">")) { p += 2; s->lregion = region_find(tok[p++]); }
		else if (is(":") || is("=")) unsupported(is(":") ? "PHDRS (:phdr)" : "section fill (=fill)");
		else if (is(",")) p++;
		else break;
	}
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
		if (is("OVERLAY") || is("INCLUDE") || is("INSERT")) unsupported(peek(0));
		parse_outsec();
	}
	expect("}");
}

static const char default_script[] =
	"ENTRY(_start)\n"
	"SECTIONS {\n"
	"  . = %#x + SIZEOF_HEADERS;\n"
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
	"  .rel.plt  : { *(.rel.plt) }\n"
	"  .rel.dyn  : { *(.rel.dyn) }\n"
	"  .dynamic  : { *(.dynamic) }\n"
	"  . = ALIGN(0x1000);\n"                               /* the writable segment starts on a fresh page: W^X */
	/* RELRO: written only while relocating, then write-protected — .init/.fini arrays, .data.rel.ro, the GOT
	 * (.got.plt too: our loader binds eagerly, as GNU ld's -z now); a dynamic output ends it on a page boundary */
	"  .preinit_array : { PROVIDE_HIDDEN(__preinit_array_start = .); KEEP(*(.preinit_array)) PROVIDE_HIDDEN(__preinit_array_end = .); }\n"
	"  .init_array : { PROVIDE_HIDDEN(__init_array_start = .); KEEP(*(SORT_BY_NAME(.init_array.*))) KEEP(*(.init_array))\n"
	"                  PROVIDE_HIDDEN(__init_array_end = .); }\n"
	"  .fini_array : { PROVIDE_HIDDEN(__fini_array_start = .); KEEP(*(SORT_BY_NAME(.fini_array.*))) KEEP(*(.fini_array))\n"
	"                  PROVIDE_HIDDEN(__fini_array_end = .); }\n"
	"  .data.rel.ro : { *(.data.rel.ro .data.rel.ro.*) }\n"
	"  .got      : { *(.got.plt) *(.got) }\n"
	"  %s\n"                                                /* `. = ALIGN(0x1000);` when RELRO applies */
	"  .data     : { *(.data .data.*) }\n"
	"  PROVIDE(_edata = .); PROVIDE(edata = .); PROVIDE(__bss_start = .); PROVIDE(__bss_start__ = .);\n"
	"  .bss      : { *(.dynbss) *(.bss .bss.*) *(COMMON) }\n"
	"  PROVIDE(__bss_end__ = .); PROVIDE(_end = .); PROVIDE(end = .); PROVIDE(__end__ = .);\n"
	"}\n";

void script_read(const char *path) {
	char *text;
	if (path) {
		FILE *f = fopen(path, "rb"); if (!f) die("cannot open linker script %s", path);
		fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
		text = malloc(n + 1); if (fread(text, 1, n, f) != (size_t)n) die("read %s failed", path);
		text[n] = 0; fclose(f); script_name = path;
	} else {
		relro = !norelro && (shared || pie || nshlib);     /* a loader will apply it: a dynamic output */
		text = malloc(sizeof default_script + 64); sprintf(text, default_script, load_base, relro ? ". = ALIGN(0x1000);" : "");
		script_name = "(default)";
	}
	tokenize(text);
	for (p = 0; p < ntok; ) {
		if (is(";")) { p++; continue; }
		if (is("ENTRY")) { p++; expect("("); if (!entry_sym) entry_sym = tok[p]; p++; expect(")"); continue; }   /* -e wins */
		if (is("MEMORY")) { p++; parse_memory(); continue; }
		if (is("SECTIONS")) { p++; parse_sections(); continue; }
		if (at_assignment()) { parse_assignment(&new_stmt(S_ASSIGN)->a, 0); continue; }
		if (is("ASSERT")) { parse_assert(); continue; }
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
static int placeable(Obj *o, int j) {
	Elf32_Shdr *s = &o->sh[j];
	if (!(s->sh_flags & SHF_ALLOC) || o->sec_out[j]) return 0;
	if (o == linker_obj && !s->sh_size) return 0;       /* a linker table this link doesn't need */
	if (s->sh_flags & SHF_TLS) die("%s: thread-local section %s is not supported", o->path, sec_name(o, j));
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

/* An orphan joins the output section of its name, else starts one after the last output section of its kind. */
static int kind_of(u32 flags, u32 type) { return (flags & (SHF_WRITE | SHF_EXECINSTR)) | (type == SHT_NOBITS ? 8 : 0); }
static int stmt_kind(const Stmt *s) {
	u32 flags = 0; int nobits = 1, any = 0;
	for (int k = 0; k < s->nitem; k++) for (int i = 0; i < s->items[k].nin; i++) {
		Elf32_Shdr *h = &s->items[k].in[i].obj->sh[s->items[k].in[i].shndx];
		flags |= h->sh_flags; nobits &= h->sh_type == SHT_NOBITS; any = 1;
	}
	return any ? kind_of(flags, nobits ? SHT_NOBITS : SHT_PROGBITS) : -1;
}
static void place_orphan(Obj *o, int j) {
	const char *name = sec_name(o, j);
	int at = -1;
	for (int i = 0; i < nst; i++) if (st[i].kind == S_OUTSEC && !st[i].discard && !strcmp(st[i].os->name, name)) at = i;
	if (at < 0) {
		int kind = kind_of(o->sh[j].sh_flags, o->sh[j].sh_type), same = -1, samew = -1;
		for (int i = 0; i < nst; i++) {
			if (st[i].kind != S_OUTSEC || st[i].discard) continue;
			int k = stmt_kind(&st[i]); if (k < 0) continue;
			if (k == kind) same = i;
			if ((k & SHF_WRITE) == (kind & SHF_WRITE)) samew = i;
		}
		at = (same >= 0 ? same : samew >= 0 ? samew : nst - 1) + 1;
		new_stmt(S_OUTSEC);                             /* make room, then shift the tail down by one */
		memmove(&st[at + 1], &st[at], (nst - 1 - at) * sizeof *st);
		Stmt *s = &st[at]; memset(s, 0, sizeof *s);
		s->kind = S_OUTSEC; s->addr.lo = s->at.lo = s->align.lo = -1; s->vregion = s->lregion = -1;
		s->os = calloc(1, sizeof *s->os); s->os->name = name;
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
static OutSec *placed_outsec(const char *name, const char *fn) {
	for (int i = 0; i < nst; i++)
		if (st[i].kind == S_OUTSEC && !st[i].discard && !strcmp(st[i].os->name, name)) {
			if (!st[i].os->done) die("linker script %s: %s(%s) before that section is laid out", script_name, fn, name);
			return st[i].os;
		}
	die("linker script %s: %s(%s): no such output section", script_name, fn, name);
}
static u32 align_to(u32 x, u32 a) { return a > 1 ? (x + a - 1) / a * a : x; }
static u32 sym_value(const char *name) {
	GSym *g = gsym_find(name);
	if (!g || !g->defined) die("linker script %s: undefined symbol '%s' in an expression", script_name, name);
	if (g->obj) {
		Elf32_Sym *s = &g->obj->sym[g->symidx];
		if (s->st_shndx != SHN_ABS && (!g->obj->sec_out[s->st_shndx] || !g->obj->sec_out[s->st_shndx]->done))
			die("linker script %s: '%s' used before its section is laid out", script_name, name);
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
	if (t[0] >= '0' && t[0] <= '9') {
		char *end; unsigned long v = strtoul(t, &end, 0);
		if (*end == 'K' || *end == 'k') v <<= 10, end++; else if (*end == 'M' || *end == 'm') v <<= 20, end++;
		if (*end) die("linker script %s: bad number '%s'", script_name, t);
		return (u32)v;
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
	ep++; u32 a = e_cond(); eexpect(":"); u32 b = e_cond();
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
	if (s->align.lo >= 0) { u32 a = eval(s->align, *dot); if (a > align) align = a; }
	u32 *vc = s->vregion >= 0 ? &regions[s->vregion].cur : dot;
	if (s->addr.lo >= 0) *vc = eval(s->addr, *dot);
	*vc = align_to(*vc, align);
	os->vaddr = *vc;
	os->lma = s->at.lo >= 0 ? eval(s->at, *dot) : s->lregion >= 0 ? align_to(regions[s->lregion].cur, align) : os->vaddr;
	u32 delta = os->lma - os->vaddr;
	for (int k = 0; k < s->nitem; k++) {
		Item *it = &s->items[k];
		if (it->kind == I_ASSIGN) { assign(&it->a, vc, os); continue; }
		for (int i = 0; i < it->nin; i++) {             /* a link-order section sorts by its linked section's address */
			Obj *o = it->in[i].obj; int j = it->in[i].shndx;
			if (!(o->sh[j].sh_flags & SHF_LINK_ORDER)) continue;
			int l = (int)o->sh[j].sh_link;
			if (!l || !o->sec_out[l] || !o->sec_out[l]->done)
				die("%s: %s is laid out before the section it describes", o->path, sec_name(o, j));
		}
		for (int i = 1; i < it->nin; i++) for (int b = i; b > 0; b--) {   /* stable insertion sort */
			InSec x = it->in[b - 1], y = it->in[b];
			int swap;
			if (x.obj->sh[x.shndx].sh_flags & SHF_LINK_ORDER)
				swap = x.obj->sec_vaddr[x.obj->sh[x.shndx].sh_link] > y.obj->sec_vaddr[y.obj->sh[y.shndx].sh_link];
			else swap = it->sort && strcmp(sec_name(x.obj, x.shndx), sec_name(y.obj, y.shndx)) > 0;
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
	*dot = *vc;
	os->done = 1; last_os = os;
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
static void build_segments(void) {
	nseg = 0; noutsec = 0; headers_mapped = 0;
	for (int i = 0; i < nst; i++) {
		if (st[i].kind != S_OUTSEC || st[i].discard || !st[i].os->size) continue;
		OutSec *os = st[i].os;
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
	for (int i = 0; i < noutsec; i++) outsecs[i]->off = segs[outsecs[i]->seg].off + (outsecs[i]->vaddr - segs[outsecs[i]->seg].vaddr);

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

void layout_run(void) {
	for (int i = 0; i < nobj; i++) {
		Obj *o = objs[i];
		if (o->active) for (int j = 1; j < o->nsh; j++) if (placeable(o, j)) place_orphan(o, j);
	}
	for (int i = 0; i < nst; i++) {                     /* each output section's type/flags from its inputs */
		if (st[i].kind != S_OUTSEC || st[i].discard) continue;
		OutSec *os = st[i].os; int first = 1, nobits = 1;
		for (int k = 0; k < st[i].nitem; k++) for (int n = 0; n < st[i].items[k].nin; n++) {
			Elf32_Shdr *h = &st[i].items[k].in[n].obj->sh[st[i].items[k].in[n].shndx];
			os->flags |= h->sh_flags & (SHF_ALLOC | SHF_WRITE | SHF_EXECINSTR);
			nobits &= h->sh_type == SHT_NOBITS;
			if (first) { os->type = h->sh_type; os->entsize = h->sh_entsize; first = 0; }
			else { if (os->type != h->sh_type) os->type = SHT_PROGBITS; if (os->entsize != h->sh_entsize) os->entsize = 0; }
		}
		if (first) os->flags = SHF_ALLOC | SHF_WRITE;   /* only `. = . + N`: reserved space */
		if (st[i].noload || nobits) os->type = SHT_NOBITS;
		else if (os->type == SHT_NOBITS) os->type = SHT_PROGBITS;   /* .bss inside a PROGBITS section: file zeros */
	}
	decide_stack();
	int want = 2;                                       /* program headers: a guess, refined until stable */
	for (int pass = 0; ; pass++) {
		hdrsz = sizeof(Elf32_Ehdr) + (u32)want * sizeof(Elf32_Phdr);
		assign_addresses();
		build_segments();
		if (nphdr == want) break;
		if (pass == 8) die("internal: the program header count does not settle");
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
		if (a->vaddr < b->vaddr + b->size && b->vaddr < a->vaddr + a->size)
			die("sections %s [%#x,%#x) and %s [%#x,%#x) overlap", a->name, a->vaddr, a->vaddr + a->size, b->name, b->vaddr, b->vaddr + b->size);
		if (a->type != SHT_NOBITS && b->type != SHT_NOBITS && a->lma < b->lma + b->size && b->lma < a->lma + a->size)
			die("sections %s and %s overlap at their load addresses [%#x,%#x) / [%#x,%#x)", a->name, b->name, a->lma, a->lma + a->size, b->lma, b->lma + b->size);
	}
}
