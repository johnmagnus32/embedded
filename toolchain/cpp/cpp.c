/*
 * cpp.c — our from-scratch C preprocessor (the `gcc -E` / `cpp` stage), the last tool of the toolchain.
 * It runs BEFORE our cc: expand #include / #define / conditionals / macros, emit plain C text.
 *
 * Design: tokenize the whole translation unit into a stream of preprocessing tokens (each carrying
 * "beginning-of-line" and "leading-space" flags, plus a hide set for macro-recursion safety), then a
 * single driver walks the stream — handling directives at line starts, tracking a conditional stack for
 * #if/#ifdef nesting, splicing in #include files, and macro-expanding everything else — and prints the
 * result. Macro expansion follows the standard hide-set method so a macro can't expand itself forever.
 *
 * Supported: #include (<...> and "..."), #define (object- and function-like, incl. # stringize and ##
 * paste and __VA_ARGS__), #undef, #if/#ifdef/#ifndef/#elif/#else/#endif with defined() and integer
 * constant expressions, #error, #warning, #line (ignored), and #pragma: `once`; `GCC error`/`GCC warning`; the
 * advisory ones (diagnostic, system_header, optimize, STDC, …) are ignored, and the ones that would change the
 * program (pack, weak, visibility, push_macro, …) are rejected rather than dropped. An unknown directive in an
 * active region is an error, as in GCC. A few predefined macros.
 */
#define _POSIX_C_SOURCE 200809L
#include "strmap.h"
#define PATHMAX 4096   /* a file path (include search builds them: too long is an error, never a truncation) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

static void die(const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	fputs("cpp: ", stderr); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap); exit(1);
}
static char *xstrdup(const char *s) { char *p = malloc(strlen(s) + 1); strcpy(p, s); return p; }

/* ------------------------------------------------------------------ tokens + hide sets ------------ */
typedef enum { TIDENT, TNUM, TSTR, TCHAR, TPUNCT, TEOF, TLINE } Kind;   /* TLINE: a whole output line (a passed-through #pragma / #ident) */
typedef struct Hide { char *name; struct Hide *next; } Hide;
typedef struct Tok {
	Kind kind; char *text;
	int bol;              /* first token on its source line (=> a '#' here starts a directive) */
	int cbol;             /* ...but only because a comment spanning lines came before it: that does NOT end a directive
	                       * line (GCC: a #define whose body has a two-line comment in it is one definition) */
	int space;            /* had leading whitespace (for readable output)                      */
	int line;             /* its physical line in its file (0: not from a file)                */
	int col;              /* its 1-based byte column there (0: none) — output indentation, as GCC's  */
	Hide *hide;           /* macros this token must NOT be expanded by                          */
	struct Tok *next;
} Tok;

static int hide_has(Hide *h, const char *n) {
	for (; h; h = h->next) if (!strcmp(h->name, n)) return 1;
	return 0;
}
static Hide *hide_add(Hide *h, const char *n) {
	Hide *e = calloc(1, sizeof *e); e->name = xstrdup(n); e->next = h;
	return e;
}
static Hide *hide_union(Hide *a, Hide *b) {
	for (; b; b = b->next) if (!hide_has(a, b->name)) a = hide_add(a, b->name);
	return a;
}
static Hide *hide_inter(Hide *a, Hide *b) {
	Hide *r = NULL;
	for (; a; a = a->next) if (hide_has(b, a->name)) r = hide_add(r, a->name);
	return r;
}

static Tok *newtok(Kind k, const char *s, size_t n) {
	Tok *t = calloc(1, sizeof *t);
	t->kind = k; t->text = malloc(n + 1); memcpy(t->text, s, n); t->text[n] = 0;
	return t;
}
static int is_p(Tok *t, const char *s) { return t->kind == TPUNCT && !strcmp(t->text, s); }
static Tok *copytok(const Tok *o) {
	Tok *t = calloc(1, sizeof *t); *t = *o;
	t->text = xstrdup(o->text); t->next = NULL;
	return t;
}

/* ------------------------------------------------------------------ lexer ------------------------- */
/* What the language standard in force lets the lexer see (GCC's lang_defaults): `//` comments (not strict C90),
 * digraphs (not strict C90), trigraphs (the strict ISO modes, or -trigraphs), u/U/u8 literal prefixes (C11 and the
 * GNU modes from gnu99), raw strings R"d(...)d" (the GNU modes from gnu99), u8 character constants (C2x). */
static struct { int cxx_comments, digraphs, trigraphs, uliterals, rliterals, u8chars, extid; } lx = { 1, 1, 0, 1, 1, 0, 1 };
static void lang_for(const char *std_key) {   /* lx for a -std (predef_delta key; NULL = gnu17): GCC's lang_defaults */
	static const struct { const char *key; int c, d, t, u, r, u8, x; } tab[] = {
		{ "-std=gnu89",          1, 1, 0, 0, 0, 0, 0 }, { "-std=gnu99", 1, 1, 0, 1, 1, 0, 1 }, { "-std=gnu11", 1, 1, 0, 1, 1, 0, 1 },
		{ "-std=gnu2x",          1, 1, 0, 1, 1, 1, 1 }, { "-std=c89",   0, 0, 1, 0, 0, 0, 0 }, { "-std=iso9899:199409", 0, 1, 1, 0, 0, 0, 0 },
		{ "-std=c99",            1, 1, 1, 0, 0, 0, 1 }, { "-std=c11",   1, 1, 1, 1, 0, 0, 1 }, { "-std=c17", 1, 1, 1, 1, 0, 0, 1 },
		{ "-std=c2x",            1, 1, 1, 1, 0, 1, 1 } };
	if (!std_key) return;                                    /* gnu17: the initializer */
	for (unsigned i = 0; i < sizeof tab / sizeof *tab; i++) if (!strcmp(tab[i].key, std_key)) {
		lx.cxx_comments = tab[i].c; lx.digraphs = tab[i].d; lx.trigraphs = tab[i].t; lx.uliterals = tab[i].u;
		lx.rliterals = tab[i].r; lx.u8chars = tab[i].u8; lx.extid = tab[i].x; return;
	}
	die("internal: no lexer rules for %s", std_key);
}
/* Punctuators, longest first (digraphs keep their spelling: GCC prints `<:` as written). */
static const char *PUNCT[] = { "%:%:", "<<=", ">>=", "...", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=",
	"&&", "||", "##", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<:", ":>", "<%", "%>", "%:", NULL };
static int is_digraph(const char *s) { return !strcmp(s, "<:") || !strcmp(s, ":>") || !strcmp(s, "<%") || !strcmp(s, "%>") || !strcmp(s, "%:") || !strcmp(s, "%:%:"); }
static int ident_start(const char *p) { return isalpha((unsigned char)*p) || *p == '_' || *p == '$' || (lx.extid && ((unsigned char)*p >= 0x80
	|| (p[0] == '\\' && (p[1] == 'u' || p[1] == 'U')))); }
static size_t ident_char(const char *p) {   /* bytes of the identifier character at p (0: none) */
	if (isalnum((unsigned char)*p) || *p == '_' || *p == '$' || (lx.extid && (unsigned char)*p >= 0x80)) return 1;
	if (lx.extid && p[0] == '\\' && (p[1] == 'u' || p[1] == 'U')) {   /* a UCN: \uXXXX / \UXXXXXXXX */
		size_t n = p[1] == 'u' ? 4 : 8;
		for (size_t k = 0; k < n; k++) if (!isxdigit((unsigned char)p[2 + k])) return 0;
		return 2 + n;
	}
	return 0;
}
/* A literal's encoding prefix at p (L u U u8, + R for raw) that the language knows: its length, 0 if none. */
static size_t lit_prefix(const char *p, int *raw) {
	size_t n = 0; *raw = 0;
	if (p[0] == 'L') n = 1;
	else if (lx.uliterals && p[0] == 'u' && p[1] == '8') n = 2;
	else if (lx.uliterals && (p[0] == 'u' || p[0] == 'U')) n = 1;
	if (lx.rliterals && p[n] == 'R' && p[n + 1] == '"') { *raw = 1; return n + 1; }
	if (p[n] == '"') return n;
	if (p[n] == '\'' && !(n == 2 && !lx.u8chars) && !(n == 1 && p[0] != 'L' && !lx.uliterals)) return n;
	return 0;
}
/* The length of the preprocessing token at p (not whitespace or a comment) and its kind. */
static size_t lex_len(const char *p, Kind *kind) {
	const char *s = p; int raw; size_t pre = (*p == 'L' || *p == 'u' || *p == 'U' || *p == 'R') ? lit_prefix(p, &raw) : 0;
	if (pre && raw) {                                                       /* R"delim( ... )delim" */
		p += pre + 1; const char *d = p; while (*p && *p != '(' && p - d < 16) p++;
		size_t dl = p - d;
		for (p++; *p; p++) if (*p == ')' && !strncmp(p + 1, d, dl) && p[1 + dl] == '"') { p += dl + 2; break; }
		*kind = TSTR; return p - s;
	}
	if (pre || *p == '"' || *p == '\'') {                                   /* string / char literal (to the closing quote or the line's end) */
		p += pre; char q = *p++;
		while (*p && *p != q && *p != '\n') { if (*p == '\\' && p[1] && p[1] != '\n') p++; p++; }
		if (*p == q) p++;
		*kind = q == '"' ? TSTR : TCHAR;
		return p - s;
	}
	if (isdigit((unsigned char)*p) || (*p == '.' && isdigit((unsigned char)p[1]))) {   /* pp-number */
		p++;
		for (;;) {
			if ((*p == '+' || *p == '-') && (p[-1] == 'e' || p[-1] == 'E' || p[-1] == 'p' || p[-1] == 'P')) p++;
			else if (*p == '.') p++;
			else { size_t c = ident_char(p); if (!c) break; p += c; }
		}
		*kind = TNUM; return p - s;
	}
	if (ident_start(p)) {
		size_t c; while ((c = ident_char(p))) p += c;
		if (p == s) p++;                                                    /* a lone backslash */
		*kind = p - s == 1 && *s == '\\' ? TPUNCT : TIDENT; return p - s;
	}
	size_t l = 1;
	for (int i = 0; PUNCT[i]; i++) { size_t pl = strlen(PUNCT[i]); if (!strncmp(p, PUNCT[i], pl) && (lx.digraphs || !is_digraph(PUNCT[i]))) { l = pl; break; } }
	if ((unsigned char)*p >= 0x80) while ((((unsigned char)p[l]) & 0xc0) == 0x80) l++;   /* one UTF-8 character */
	*kind = TPUNCT; return l;
}
/* Translation phases 1-2 over a file's bytes: newlines normalized (CR LF, CR), a leading UTF-8 BOM dropped, trigraphs
 * replaced (when on), backslash-newline splices removed (also with trailing blanks, as GCC accepts). *lines gets each
 * output byte's physical line. */
static char *phase12(const char *src, size_t n, int **lines) {
	char *o = malloc(n + 2); int *ln = malloc((n + 2) * sizeof *ln); size_t k = 0; int line = 1;
	size_t i = (n >= 3 && !memcmp(src, "\xEF\xBB\xBF", 3)) ? 3 : 0;
	static const char tri_from[] = "=(/)'<!>-", tri_to[] = "#[\\]^{|}~";
	while (i < n) {
		char c = src[i]; size_t w = 1;
		if (c == '\r') { c = '\n'; if (i + 1 < n && src[i + 1] == '\n') w = 2; }
		else if (lx.trigraphs && c == '?' && i + 2 < n && src[i + 1] == '?' && strchr(tri_from, src[i + 2]) && src[i + 2]) { c = tri_to[strchr(tri_from, src[i + 2]) - tri_from]; w = 3; }
		if (c == '\\') {                                                    /* a splice: \ [blanks] newline */
			size_t j = i + w; while (j < n && (src[j] == ' ' || src[j] == '\t' || src[j] == '\f' || src[j] == '\v')) j++;
			if (j < n && (src[j] == '\n' || src[j] == '\r')) { i = j + (src[j] == '\r' && j + 1 < n && src[j + 1] == '\n' ? 2 : 1); line++; continue; }
		}
		o[k] = c; ln[k] = line; k++; i += w;
		if (c == '\n') line++;
	}
	o[k] = 0; ln[k] = line; *lines = ln;
	return o;
}
static Tok *tokenize_lines(const char *src, const int *lines);
static Tok *tokenize(const char *src) { return tokenize_lines(src, NULL); }
static Tok *tokenize_lines(const char *src, const int *lines) {
	Tok head = {0}, *cur = &head;
	int bol = 1, cbol = 0, space = 0; const char *ls = src;   /* ls: the start of the current line */
	for (const char *p = src; *p; ) {
		if (*p == '\n') { p++; ls = p; bol = 1; cbol = 0; space = 0; continue; }
		if (*p == ' ' || *p == '\t' || *p == '\f' || *p == '\v') { p++; space = 1; continue; }
		if (lx.cxx_comments && p[0] == '/' && p[1] == '/') { while (*p && *p != '\n') p++; space = 1; continue; }
		if (p[0] == '/' && p[1] == '*') {
			const char *e = strstr(p + 2, "*/"); if (!e) die("unterminated comment");
			for (const char *q = p; q < e; q++) if (*q == '\n') { ls = q + 1; if (!bol) bol = 1, cbol = 1; }   /* a comment spanning lines: a new
			                                                                              * line for text, not for a directive */
			p = e + 2; space = 1; continue;
		}
		Kind k; size_t l = lex_len(p, &k);
		Tok *t = newtok(k, p, l); t->line = lines ? lines[p - src] : 0; t->col = (int)(p - ls) + 1; p += l;
		t->bol = bol; t->cbol = cbol; t->space = space; bol = 0; cbol = 0; space = 0;
		cur = cur->next = t;
	}
	cur->next = newtok(TEOF, "", 0); cur->next->bol = 1;
	return head.next;
}
static Tok *slurp_tokens(const char *path) {
	FILE *f = fopen(path, "rb"); if (!f) return NULL;
	struct stat st; if (fstat(fileno(f), &st) || S_ISDIR(st.st_mode)) { fclose(f); die("%s is a directory", path); }
	size_t n = 0, cap = 1 << 16, r; char *b = malloc(cap);
	while ((r = fread(b + n, 1, cap - n, f)) > 0) { n += r; if (n == cap) b = realloc(b, cap *= 2); }
	if (ferror(f)) die("read failed: %s", path);
	fclose(f);
	int *lines; char *t = phase12(b, n, &lines); free(b);
	return tokenize_lines(t, lines);
}

/* ------------------------------------------------------------------ macro table ------------------- */
typedef struct Macro {
	char *name; int func; int variadic;
	char **params; int nparams;
	Tok *body;
	struct Macro *next;
} Macro;
/* The defined macros: a hash table by name (a kernel translation unit defines ~20K — every identifier the
 * preprocessor sees is a lookup), and a list in definition order (newest first) for -dM. */
static Macro *macros; static StrMap macro_map;
static Macro *macro_find(const char *n) { return strmap_get(&macro_map, n); }
static void macro_undef(const char *n) {
	Macro *m = strmap_get(&macro_map, n); if (!m) return;
	strmap_put(&macro_map, m->name, NULL);
	for (Macro **p = &macros; *p; p = &(*p)->next) if (*p == m) { *p = m->next; return; }
}
static Macro *macro_add(const char *n) {
	macro_undef(n);                                          /* a redefinition replaces the old one */
	Macro *m = calloc(1, sizeof *m); m->name = xstrdup(n); m->next = macros; macros = m;
	strmap_put(&macro_map, m->name, m);
	return m;
}

/* ------------------------------------------------------------------ include paths ----------------- */
static char *inc_dirs[64]; static int n_inc_dirs; static char inc_sys[64];   /* inc_sys: an -isystem/-idirafter dir */
/* Dependency output (-MD / -MMD, -MF file, -MT target, -MP): the main file and every header read, in first-read order
 * — for -MMD without the ones found in system directories — in make's syntax, as GCC writes it. */
static int dep_mode;                   /* 0 none, 1 -MD (all), 2 -MMD (user headers) */
static const char *dep_file, *dep_target; static int dep_phony;
static char **deps; static int ndeps, depcap;
static void add_dep(const char *path, int sys) {
	if (!dep_mode || (dep_mode == 2 && sys)) return;
	for (int i = 0; i < ndeps; i++) if (!strcmp(deps[i], path)) return;
	if (ndeps == depcap) { depcap = depcap ? 2 * depcap : 64; deps = realloc(deps, depcap * sizeof *deps); }
	deps[ndeps++] = strdup(path);
}
static void write_deps(const char *main_in, const char *outpath) {
	if (!dep_mode) return;
	char tgt[PATHMAX];
	if (dep_target) snprintf(tgt, sizeof tgt, "%s", dep_target);
	else {   /* GCC: the source's basename with .o */
		const char *b = strrchr(main_in, '/'); b = b ? b + 1 : main_in;
		snprintf(tgt, sizeof tgt, "%s", b); char *dot = strrchr(tgt, '.'); if (dot) strcpy(dot, ".o"); else strcat(tgt, ".o");
	}
	(void)outpath;
	FILE *f = fopen(dep_file, "w"); if (!f) die("cannot write dependency file %s", dep_file);
	int col = fprintf(f, "%s:", tgt);
	for (int i = 0; i < ndeps; i++) {
		int n = (int)strlen(deps[i]);
		if (i && col + 1 + n > 75) { fputs(" \\\n", f); col = 0; }
		col += fprintf(f, " %s", deps[i]);
	}
	fputc('\n', f);
	if (dep_phony) for (int i = 1; i < ndeps; i++) fprintf(f, "%s:\n", deps[i]);
	if (fclose(f)) die("cannot write dependency file %s", dep_file);
}

/* ------------------------------------------------------------------ output ------------------------ */
static FILE *out;
/* Would `a` written right before `b` read back as other tokens (`-` `-1` -> `--1`, `x` `y` -> `xy`, `/` `*`)? Tokens
 * from different places (a macro's result and its neighbour, a paste) meet with no space between; GCC's output
 * separates exactly these (cpp_avoid_paste). */
static int would_merge(const char *a, const char *b) {
	size_t la = strlen(a), lb = strlen(b);
	if (a[la - 1] == '/' && (b[0] == '/' || b[0] == '*')) return 1;       /* a comment */
	char sb[256], *buf = la + lb < sizeof sb ? sb : malloc(la + lb + 1);
	memcpy(buf, a, la); memcpy(buf + la, b, lb + 1);
	Kind k; int m = lex_len(buf, &k) != la;
	if (buf != sb) free(buf);
	return m;
}
static int dump_macros;
static void emit(Tok *tok) {
	if (dump_macros) return;                                 /* -dM: only the macros, at the end */
	int line_open = 0; const Tok *prev = NULL;
	for (; tok && tok->kind != TEOF; tok = tok->next) {
		if (tok->kind == TLINE) { if (line_open) fputc('\n', out); fprintf(out, "%s\n", tok->text); line_open = 0; prev = NULL; continue; }
		if (tok->bol) {                                /* a source line's first token: in its column (GCC cb_line_change) */
			if (line_open) fputc('\n', out);
			for (int c = 1; c < tok->col; c++) fputc(' ', out);
			if (!tok->col && line_open) fputc(' ', out);
		}
		else if (tok->space || (prev && would_merge(prev->text, tok->text))) fputc(' ', out);
		fputs(tok->text, out); line_open = 1; prev = tok;
	}
	if (line_open) fputc('\n', out);
}

/* ------------------------------------------------------------------ list helpers ------------------ */
static Tok *last(Tok *t) { while (t && t->next) t = t->next; return t; }
/* Copy a macro body, tagging every token with the given hide set (union with its own). */
static Tok *copy_body(Tok *body, Hide *hs) {
	Tok head = {0}, *cur = &head;
	for (Tok *t = body; t; t = t->next) {
		Tok *c = copytok(t);
		c->hide = hide_union(c->hide, hs);
		cur = cur->next = c;
	}
	return head.next;
}

/* ------------------------------------------------------------------ function-macro args ----------- */
/* Read one macro invocation's arguments starting just after the name. `tok` points at "("; on return it
 * points just past the matching ")", and *rparen at it. args[i] is a NUL(EOF)-terminated token list for the i-th
 * argument. */
static int read_args(Tok **tok, Tok **rparen, Tok **args, int max, int variadic, int nparams) {
	Tok *t = (*tok)->next;   /* skip "(" */
	int n = 0, depth = 0;
	Tok head = {0}, *cur = &head;
	for (;;) {
		if (t->kind == TEOF) die("unterminated macro argument list");
		if (depth == 0 && t->text[0] == ')' && !t->text[1]) break;      /* end of the whole arg list */
		/* a top-level comma ends an argument — unless we're inside the variadic tail (it keeps commas) */
		if (depth == 0 && t->text[0] == ',' && !t->text[1] && !(variadic && n == nparams - 1)) {
			cur->next = newtok(TEOF, "", 0);
			if (n < max) args[n] = head.next;
			n++; head.next = NULL; cur = &head; t = t->next;
			continue;
		}
		if (t->text[0] == '(' && !t->text[1]) depth++;                  /* track nested parens */
		if (t->text[0] == ')' && !t->text[1]) depth--;
		cur = cur->next = copytok(t); t = t->next;
	}
	cur->next = newtok(TEOF, "", 0);                                    /* the final argument */
	if (n < max) args[n] = head.next;
	n++;
	*rparen = t; *tok = t->next;   /* past ")" */
	return n;
}

static Tok *expand(Tok *tok);   /* fwd */

/* Turn a token list into a single string literal (the # operator). */
static Tok *stringize(Tok *arg) {   /* C11 6.10.3.2: spaces between tokens become one; a \ or " inside a string or
	                                    * character literal gets a \ (only there: a stray \ is spelled as is) */
	size_t cap = 64, k = 0; char *buf = malloc(cap);
	#define PUT(ch) do { if (k + 2 >= cap) { cap *= 2; buf = realloc(buf, cap); } buf[k++] = (ch); } while (0)
	PUT('"');
	for (Tok *t = arg; t && t->kind != TEOF; t = t->next) {
		if (t->space && k > 1) PUT(' ');
		int lit = t->kind == TSTR || t->kind == TCHAR;
		for (const char *c = t->text; *c; c++) { if (lit && (*c == '"' || *c == '\\')) PUT('\\'); PUT(*c); }
	}
	PUT('"'); buf[k] = 0;
	#undef PUT
	Tok *r = newtok(TSTR, buf, k); free(buf); return r;
}
/* Paste two tokens' spellings into one new token (the ## operator). Either side may be NULL (an empty
 * macro argument acts as a placemarker); if both are empty the result is nothing (NULL). The spelling must be
 * exactly one preprocessing token (GCC: "pasting X and Y does not give a valid preprocessing token"). */
static int asm_mode;   /* -x assembler-with-cpp (set from po before preprocessing) */
static Tok *paste(Tok *a, Tok *b) {
	if (!a || !b) return a ? a : b;
	size_t n = strlen(a->text) + strlen(b->text) + 1; char *buf = malloc(n); snprintf(buf, n, "%s%s", a->text, b->text);
	Tok *r = tokenize(buf);
	if (r->kind == TEOF || r->next->kind != TEOF) {
		/* GCC's assembler mode lets it be: the two stay separate tokens, written side by side (a linker script's
		 * `.initcall##level##.init` -> `.initcall0.init`) — a list of two, the caller emits the first */
		if (asm_mode) { free(buf); Tok *x = copytok(a), *y = copytok(b); x->next = y; y->next = NULL; y->space = 0; y->bol = 0; x->bol = 0; return x; }
		die("pasting \"%s\" and \"%s\" does not give a valid preprocessing token", a->text, b->text);
	}
	free(buf); r->next = NULL; r->bol = 0; r->space = a->space;   /* where the left operand was */
	return r;
}
/* Copy an argument's tokens WITHOUT its terminating TEOF (which must never leak into the output stream). */
static Tok *copy_noeof(Tok *arg) {
	Tok head = {0}, *cur = &head;
	for (Tok *t = arg; t && t->kind != TEOF; t = t->next) cur = cur->next = copytok(t);
	return head.next;
}

static int is_paste(const Tok *t) { return t->kind == TPUNCT && !strcmp(t->text, "##"); }
static int param_index(const Macro *m, const Tok *t) {
	if (t->kind == TIDENT) for (int i = 0; i < m->nparams; i++) if (!strcmp(t->text, m->params[i])) return i;
	return -1;
}
/* A ## operand's tokens (fresh, TEOF-free): a parameter's raw argument (NULL: absent or empty), else the token. */
static Tok *operand(const Macro *m, const Tok *t, Tok **args, int nargs) {
	int i = param_index(m, t);
	if (i < 0) return copytok(t);
	return i < nargs ? copy_noeof(args[i]) : NULL;
}
/* Substitute args into a function-macro body, handling # and ##; params expanded unless adjacent to #/##. */
static Tok *subst(Macro *m, Tok **args, int nargs, int line) {   /* line: the invocation's (its body's tokens take it) */
	Tok head = {0}, *cur = &head;
	Tok *pre[64] = {0}; char have[64] = {0};   /* each argument macro-expanded ONCE, when first needed (GCC): a parameter
	                                              * used twice must not expand its argument twice (__COUNTER__ would tick) */
	for (Tok *t = m->body; t; ) {
		int pi = -1;
		if (t->kind == TIDENT) for (int i = 0; i < m->nparams; i++) if (!strcmp(t->text, m->params[i])) pi = i;
		int va = (m->variadic && t->kind == TIDENT && !strcmp(t->text, "__VA_ARGS__"));

		if (m->func && t->text[0] == '#' && !t->text[1] && t->next) {   /* # param  -> stringize (function-like only) */
			int qi = -1; for (int i = 0; i < m->nparams; i++) if (!strcmp(t->next->text, m->params[i])) qi = i;
			Tok *a = (qi >= 0 && qi < nargs) ? args[qi] : NULL;
			cur = cur->next = stringize(a); cur->line = line; t = t->next->next; continue;
		}
		if (t->next && is_paste(t->next)) {   /* a ## chain: lhs ## rhs [## rhs ...] */
			/* Each operand is a parameter's RAW (unexpanded) tokens — an absent or empty argument is a placemarker —
			 * or the body token itself. The last token so far (acc) pastes onto the next operand's first; an
			 * operand's other tokens pass through. The group's first token keeps the body position's spacing. */
			int sp = t->space, first = 1;
			#define EMIT(x) do { Tok *e_ = (x); if (first) { e_->space = sp; first = 0; } if (!e_->line) e_->line = line; cur = cur->next = e_; } while (0)
			Tok *acc = NULL;
			for (Tok *x = operand(m, t, args, nargs); x; ) { Tok *nx = x->next; x->next = NULL; if (nx) EMIT(x); else acc = x; x = nx; }
			while (t->next && is_paste(t->next)) {
				Tok *rhs = t->next->next;
				if (!rhs) die("'##' cannot appear at either end of a macro expansion");
				Tok *r = operand(m, rhs, args, nargs);
				int vaop = m->variadic && param_index(m, rhs) == m->nparams - 1;
				if (vaop && acc && !strcmp(acc->text, ",")) {   /* GNU `, ## __VA_ARGS__`: an OMITTED variadic argument drops
				                                               * the comma (as does an empty one when it's the only parameter);
				                                               * else the comma stays and the arguments follow, unpasted */
					int omitted = nargs < m->nparams || (m->nparams == 1 && !r);
					if (!omitted) EMIT(acc);
					acc = NULL;
					for (Tok *x = r; x; ) { Tok *nx = x->next; x->next = NULL; if (nx) EMIT(x); else acc = x; x = nx; }
				} else {
					Tok *rest = r ? r->next : NULL; if (r) r->next = NULL;
					Tok *p = paste(acc, r);
					if (p && p->next) { Tok *second = p->next; p->next = NULL; EMIT(p); p = second; }   /* (assembler mode: not one token) */
					if (!rest) acc = p;
					else { if (p) EMIT(p); acc = NULL; for (Tok *x = rest; x; ) { Tok *nx = x->next; x->next = NULL; if (nx) EMIT(x); else acc = x; x = nx; } }
				}
				t = rhs;
			}
			if (acc) EMIT(acc);
			#undef EMIT
			t = t->next;
			continue;
		}
		if (pi >= 0 && pi < nargs) {                                 /* a parameter: splice its EXPANDED argument, */
			int firsttok = 1;                                       /* spaced where the parameter is in the body */
			if (pi >= 64) die("macro '%s': too many parameters", m->name);
			if (!have[pi]) { pre[pi] = expand(copy_body(args[pi], NULL)); have[pi] = 1; }
			for (Tok *a = pre[pi]; a && a->kind != TEOF; a = a->next) {
				cur = cur->next = copytok(a);
				if (firsttok) { cur->space = t->space; firsttok = 0; }
			}
			t = t->next; continue;
		}
		if (pi >= 0 || va) { t = t->next; continue; }               /* an omitted (variadic) argument: nothing */
		cur = cur->next = copytok(t); cur->line = line; t = t->next;   /* ordinary body token */
	}
	return head.next;
}

/* Core: expand all macros in a token list, returning a fresh list (used for text and #if lines). */
static Tok *builtin_tok(const Tok *t);
static Tok *do_pragma(Tok *line);
static Tok *destringize_pragma(const Tok *s);
static char **poisoned; static int npoisoned;             /* #pragma GCC poison */
static int is_poisoned(const char *n) { for (int i = 0; i < npoisoned; i++) if (!strcmp(poisoned[i], n)) return 1; return 0; }
/* __has_attribute / __has_c_attribute / __has_builtin (anywhere; the operand is macro-expanded, as GCC does) and
 * __has_include / __has_include_next (#if only): GCC's answers — the first three from has.h, measured from GCC by
 * gen-has.py (a name it doesn't hold is an error: regenerate, never a silent 0); the includes by searching. */
#include "has.h"
static int in_if;                                      /* evaluating a #if / #elif line */
static char *find_include_from(const char *name, int angle, int from);
static int cur_inc_idx = -1;                           /* the include dir the current file came from (-1: none) */
static int has_kind(const char *n) {
	return !strcmp(n, "__has_attribute") ? 1 : !strcmp(n, "__has_c_attribute") ? 2 : !strcmp(n, "__has_builtin") ? 3
	     : !strcmp(n, "__has_include") ? 4 : !strcmp(n, "__has_include_next") ? 5 : 0;
}
static long has_lookup(int kind, const char *raw) {
	char n[256]; size_t L = strlen(raw);
	if (kind != 3 && L > 4 && raw[0] == '_' && raw[1] == '_' && raw[L - 1] == '_' && raw[L - 2] == '_') snprintf(n, sizeof n, "%.*s", (int)(L - 4), raw + 2);
	else snprintf(n, sizeof n, "%s", raw);
	#define LOOK(tab) for (unsigned i = 0; i < sizeof tab / sizeof *tab; i++) if (!strcmp(tab[i].name, n)) return tab[i].val;
	if (kind == 1) { LOOK(has_attr_tab) } else if (kind == 2) { LOOK(has_c_attr_tab) } else { LOOK(has_builtin_tab) }
	#undef LOOK
	die("%s(%s): not in has.h — regenerate it with gen-has.py (add the name) so the answer is GCC's",
	    kind == 1 ? "__has_attribute" : kind == 2 ? "__has_c_attribute" : "__has_builtin", raw);
	return 0;
}
static Tok *expand(Tok *tok);
static long has_eval(int kind, const char *op, Tok *arg) {   /* arg: the tokens between the parentheses */
	if (kind >= 4) {
		if (!in_if) die("\"%s\" used outside of preprocessing directive", op);
		if (arg->kind != TSTR && !is_p(arg, "<")) arg = expand(arg);   /* a macro naming the header */
		char fn[PATHMAX] = ""; int angle = 0;
		if (arg->kind == TSTR) { size_t n = strlen(arg->text); if (n < 2 || n - 2 >= sizeof fn) die("%s: bad file name", op); memcpy(fn, arg->text + 1, n - 2); fn[n - 2] = 0; }
		else if (is_p(arg, "<")) { angle = 1; for (Tok *t = arg->next; t && !is_p(t, ">") && t->kind != TEOF; t = t->next) strncat(fn, t->text, sizeof fn - strlen(fn) - 1); }
		else die("operator \"%s\" requires a header-name", op);
		return find_include_from(fn, angle, kind == 5 ? cur_inc_idx + 1 : -1) != NULL;
	}
	Tok *x = expand(arg);
	if (x->kind != TIDENT) die("macro \"%s\" requires an identifier", op);
	if (x->next && x->next->kind != TEOF) {   /* a scoped name: gnu::X is X (GCC); other scopes it doesn't know */
		if (kind == 3 || !is_p(x->next, ":") || !x->next->next || !is_p(x->next->next, ":") || !x->next->next->next || x->next->next->next->kind != TIDENT)
			die("%s: unsupported operand", op);
		if (strcmp(x->text, "gnu") && strcmp(x->text, "__gnu__")) die("%s(%s::...): only the gnu:: scope is supported", op, x->text);
		return has_lookup(kind, x->next->next->next->text);
	}
	return has_lookup(kind, x->text);
}
static Tok *expand(Tok *tok) {
	Tok head = {0}, *cur = &head;
	while (tok && tok->kind != TEOF) {
		if (tok->kind == TIDENT && !hide_has(tok->hide, tok->text)) {
			if (npoisoned && is_poisoned(tok->text)) die("attempt to use poisoned \"%s\"", tok->text);
			if (!strcmp(tok->text, "_Pragma")) {   /* its operand is macro-expanded (GCC): `_Pragma(STR(GCC diagnostic s))` */
				Tok *o = tok->next;
				if (!o || !is_p(o, "(")) die("_Pragma takes a parenthesized string literal");
				Tok ah = {0}, *ac = &ah, *e = o->next; int depth = 0;
				while (e && e->kind != TEOF && !(depth == 0 && is_p(e, ")"))) { if (is_p(e, "(")) depth++; else if (is_p(e, ")")) depth--; ac = ac->next = copytok(e); e = e->next; }
				if (!e || e->kind == TEOF) die("_Pragma takes a parenthesized string literal");
				ac->next = newtok(TEOF, "", 0);
				Tok *st = expand(ah.next);
				if (st->kind != TSTR || (st->next && st->next->kind != TEOF)) die("_Pragma takes a parenthesized string literal");
				Tok *pl = do_pragma(destringize_pragma(st));
				tok = e->next;
				if (pl) cur = cur->next = pl;
				continue;
			}
			int hk = has_kind(tok->text);
			if (hk && !macro_find(tok->text)) {   /* __has_*(...) -> its value */
				Tok *o = tok->next;
				if (!o || !is_p(o, "(")) die("missing '(' after \"%s\"", tok->text);
				Tok ah = {0}, *ac = &ah, *e = o->next; int depth = 0;
				while (e && e->kind != TEOF && !(depth == 0 && is_p(e, ")"))) { if (is_p(e, "(")) depth++; else if (is_p(e, ")")) depth--; ac = ac->next = copytok(e); e = e->next; }
				if (!e || e->kind == TEOF) die("unterminated \"%s\"", tok->text);
				ac->next = newtok(TEOF, "", 0);
				char b[32]; snprintf(b, sizeof b, "%ld", has_eval(hk, tok->text, ah.next));
				Tok *r = newtok(TNUM, b, strlen(b)); r->space = tok->space; r->bol = tok->bol; r->col = tok->col; r->next = e->next;
				cur = cur->next = r; tok = e->next; continue;
			}
			Macro *m = macro_find(tok->text);
			Tok *b = m ? NULL : builtin_tok(tok);
			if (b) { b->next = tok->next; tok = b; cur = cur->next = tok; tok = tok->next; continue; }
			if (m && !m->func) {                                    /* object-like */
				Hide *hs = hide_add(tok->hide, m->name);
				Tok *rep = copy_body(subst(m, NULL, 0, tok->line), hs);   /* (its ## pastes) */
				if (rep) { rep->space = tok->space; rep->bol = tok->bol; rep->col = tok->col; last(rep)->next = tok->next; tok = rep; }
				else tok = tok->next;
				continue;
			}
			if (m && m->func && tok->next && tok->next->text[0] == '(' && !tok->next->text[1]) {   /* function-like */
				Tok *args[64], *paren = tok->next, *rparen;
				int nargs = read_args(&paren, &rparen, args, 64, m->variadic, m->nparams);
				if (nargs > 64) die("macro \"%s\": too many arguments (%d)", m->name, nargs);
				if (m->variadic ? nargs < m->nparams - 1 : nargs != m->nparams && !(m->nparams == 0 && nargs == 1 && args[0]->kind == TEOF)) {
					if (nargs > m->nparams && !m->variadic) die("macro \"%s\" passed %d arguments, but takes just %d", m->name, nargs, m->nparams);
					die("macro \"%s\" requires %d arguments, but only %d given", m->name, m->variadic ? m->nparams - 1 : m->nparams, nargs);
				}
				Tok *rep = subst(m, args, nargs, tok->line);
				/* Prosser's rule: the name's hide set meets the ')''s — a ')' from past the end of the expansion
				 * that produced the name (the source's) re-enables what it hid, as GCC: `__concat(F, 1) (x)` where
				 * F1's body is `__concat(G, x)` expands both; `f(2)(9)` with f(a) a*g, g(a) f(a) is 2*9*g. */
				Hide *hs = hide_add(hide_inter(tok->hide, rparen->hide), m->name);
				rep = copy_body(rep, hs);
				if (rep) { rep->space = tok->space; rep->bol = tok->bol; rep->col = tok->col; last(rep)->next = paren; tok = rep; } else tok = paren;
				continue;
			}
		}
		cur = cur->next = tok; tok = tok->next;
	}
	cur->next = tok;   /* the EOF */
	return head.next;
}

/* ------------------------------------------------------------------ dynamic builtins ------------- */
/* The macros whose value is the preprocessor's own state (GCC's builtins; not in -dM): where we are (__LINE__ in
 * the presumed numbering #line / line markers set, __FILE__, __FILE_NAME__, __BASE_FILE__, __INCLUDE_LEVEL__), a
 * counter, and when (__DATE__ / __TIME__ from SOURCE_DATE_EPOCH when set, as GCC; __TIMESTAMP__ = the file's mtime). */
static int line_delta;                 /* presumed line = physical + line_delta (this file; #line / markers move it) */
static const char *presumed = "";      /* presumed file name (__FILE__) */
/* -fmacro-prefix-map=OLD=NEW (or -ffile-prefix-map): __FILE__ / __BASE_FILE__ with a leading OLD spelled NEW; the last
 * matching option wins (GCC remap_filename). The kernel passes -fmacro-prefix-map=./= for reproducible paths. */
static struct PrefixMap { const char *old, *new_; struct PrefixMap *next; } *prefix_maps;
static const char *prefix_mapped(const char *f) {
	for (struct PrefixMap *m = prefix_maps; m; m = m->next) {
		size_t n = strlen(m->old);
		if (!strncmp(f, m->old, n)) { size_t k = strlen(m->new_) + strlen(f + n) + 1; char *r = malloc(k); snprintf(r, k, "%s%s", m->new_, f + n); return r; }
	}
	return f;
}
static const char *base_file = "";     /* the main file, as named on the command line */
static int include_level, counter;
static time_t file_mtime;              /* the current file's (for __TIMESTAMP__) */
static char *builtin_off[16]; static int nbuiltin_off;   /* #undef'd builtins (GCC allows it, with a warning) */
static const char *const builtins[] = { "__LINE__", "__FILE__", "__FILE_NAME__", "__BASE_FILE__", "__INCLUDE_LEVEL__",
	"__COUNTER__", "__DATE__", "__TIME__", "__TIMESTAMP__",
	"__has_attribute", "__has_c_attribute", "__has_builtin", "__has_include", "__has_include_next", NULL };   /* (defined: 1) */
static int is_builtin(const char *n) {
	for (int i = 0; i < nbuiltin_off; i++) if (!strcmp(builtin_off[i], n)) return 0;
	for (int i = 0; builtins[i]; i++) if (!strcmp(builtins[i], n)) return 1;
	return 0;
}
static Tok *str_tok(const char *s) {                    /* "s", escaped */
	size_t n = strlen(s); char *b = malloc(2 * n + 3), *o = b; *o++ = '"';
	for (; *s; s++) { if (*s == '"' || *s == '\\') *o++ = '\\'; *o++ = *s; }
	*o++ = '"'; *o = 0; Tok *t = newtok(TSTR, b, o - b); free(b); return t;
}
static Tok *builtin_tok(const Tok *t) {
	if (!is_builtin(t->text)) return NULL;
	const char *n = t->text; char b[64]; Tok *r;
	static const char *const mon[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	static const char *const day[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	if (!strcmp(n, "__LINE__")) { snprintf(b, sizeof b, "%d", t->line + line_delta); r = newtok(TNUM, b, strlen(b)); }
	else if (!strcmp(n, "__FILE__")) r = str_tok(prefix_mapped(presumed));
	else if (!strcmp(n, "__FILE_NAME__")) { const char *s = strrchr(presumed, '/'); r = str_tok(s ? s + 1 : presumed); }
	else if (!strcmp(n, "__BASE_FILE__")) r = str_tok(prefix_mapped(base_file));
	else if (!strcmp(n, "__INCLUDE_LEVEL__")) { snprintf(b, sizeof b, "%d", include_level); r = newtok(TNUM, b, strlen(b)); }
	else if (!strcmp(n, "__COUNTER__")) { snprintf(b, sizeof b, "%d", counter++); r = newtok(TNUM, b, strlen(b)); }
	else if (!strcmp(n, "__TIMESTAMP__")) {
		struct tm *tm = localtime(&file_mtime);
		if (tm) { snprintf(b, sizeof b, "\"%s %s %2d %02d:%02d:%02d %d\"", day[tm->tm_wday], mon[tm->tm_mon], tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_year + 1900); }
		else strcpy(b, "\"??? ??? ?? ??:??:?? ????\"");
		r = newtok(TSTR, b, strlen(b));
	} else {                                               /* __DATE__ / __TIME__ */
		static struct tm now; static int have;
		if (!have) {
			const char *e = getenv("SOURCE_DATE_EPOCH"); time_t tt;
			if (e && *e) { char *end; long long v = strtoll(e, &end, 10); if (*end || v < 0) die("environment variable SOURCE_DATE_EPOCH must expand to a non-negative integer"); tt = (time_t)v; now = *gmtime(&tt); }
			else { tt = time(NULL); now = *localtime(&tt); }
			have = 1;
		}
		if (n[2] == 'D') snprintf(b, sizeof b, "\"%s %2d %d\"", mon[now.tm_mon], now.tm_mday, now.tm_year + 1900);
		else snprintf(b, sizeof b, "\"%02d:%02d:%02d\"", now.tm_hour, now.tm_min, now.tm_sec);
		r = newtok(TSTR, b, strlen(b));
	}
	r->space = t->space; r->bol = t->bol; r->col = t->col; r->line = t->line;
	return r;
}

/* ------------------------------------------------------------------ #if expression evaluator ------ */
static long ev_ternary(Tok **t);
static long tok_num(Tok *t) {
	if (t->kind == TCHAR) { const char *p = t->text + 1; if (*p == '\\') { p++; switch (*p) { case 'n': return '\n'; case 't': return '\t'; case '0': return 0; case 'r': return '\r'; default: return *p; } } return (unsigned char)*p; }
	return strtol(t->text, NULL, 0);
}
static long ev_primary(Tok **t) {
	if (is_p(*t, "(")) { *t = (*t)->next; long v = ev_ternary(t); if (is_p(*t, ")")) *t = (*t)->next; return v; }
	if (is_p(*t, "!")) { *t = (*t)->next; return !ev_primary(t); }
	if (is_p(*t, "~")) { *t = (*t)->next; return ~ev_primary(t); }
	if (is_p(*t, "-")) { *t = (*t)->next; return -ev_primary(t); }
	if (is_p(*t, "+")) { *t = (*t)->next; return  ev_primary(t); }
	long v = ((*t)->kind == TNUM || (*t)->kind == TCHAR) ? tok_num(*t) : 0;   /* leftover idents = 0 */
	*t = (*t)->next; return v;
}
/* Precedence-climbing ladder over the token list: one function per level, low precedence outermost. */
static long ev_mul(Tok **t) {
	long v = ev_primary(t);
	for (;;) {
		if      (is_p(*t, "*")) { *t = (*t)->next; v *= ev_primary(t); }
		else if (is_p(*t, "/")) { *t = (*t)->next; long d = ev_primary(t); v = d ? v / d : 0; }
		else if (is_p(*t, "%")) { *t = (*t)->next; long d = ev_primary(t); v = d ? v % d : 0; }
		else return v;
	}
}
static long ev_add(Tok **t) {
	long v = ev_mul(t);
	for (;;) {
		if      (is_p(*t, "+")) { *t = (*t)->next; v += ev_mul(t); }
		else if (is_p(*t, "-")) { *t = (*t)->next; v -= ev_mul(t); }
		else return v;
	}
}
static long ev_shift(Tok **t) {
	long v = ev_add(t);
	for (;;) {
		if      (is_p(*t, "<<")) { *t = (*t)->next; v <<= ev_add(t); }
		else if (is_p(*t, ">>")) { *t = (*t)->next; v >>= ev_add(t); }
		else return v;
	}
}
static long ev_rel(Tok **t) {
	long v = ev_shift(t);
	for (;;) {
		if      (is_p(*t, "<"))  { *t = (*t)->next; v = v <  ev_shift(t); }
		else if (is_p(*t, ">"))  { *t = (*t)->next; v = v >  ev_shift(t); }
		else if (is_p(*t, "<=")) { *t = (*t)->next; v = v <= ev_shift(t); }
		else if (is_p(*t, ">=")) { *t = (*t)->next; v = v >= ev_shift(t); }
		else return v;
	}
}
static long ev_eq(Tok **t) {
	long v = ev_rel(t);
	for (;;) {
		if      (is_p(*t, "==")) { *t = (*t)->next; v = v == ev_rel(t); }
		else if (is_p(*t, "!=")) { *t = (*t)->next; v = v != ev_rel(t); }
		else return v;
	}
}
static long ev_band(Tok **t) { long v = ev_eq(t);   while (is_p(*t, "&"))  { *t = (*t)->next; v &= ev_eq(t); }   return v; }
static long ev_bxor(Tok **t) { long v = ev_band(t); while (is_p(*t, "^"))  { *t = (*t)->next; v ^= ev_band(t); } return v; }
static long ev_bor(Tok **t)  { long v = ev_bxor(t); while (is_p(*t, "|"))  { *t = (*t)->next; v |= ev_bxor(t); } return v; }
static long ev_land(Tok **t) { long v = ev_bor(t);  while (is_p(*t, "&&")) { *t = (*t)->next; long r = ev_bor(t);  v = v && r; } return v; }
static long ev_lor(Tok **t)  { long v = ev_land(t); while (is_p(*t, "||")) { *t = (*t)->next; long r = ev_land(t); v = v || r; } return v; }
static long ev_ternary(Tok **t) {
	long c = ev_lor(t);
	if (!is_p(*t, "?")) return c;
	*t = (*t)->next;
	long a = ev_ternary(t);
	if (is_p(*t, ":")) *t = (*t)->next;
	long b = ev_ternary(t);
	return c ? a : b;
}

/* Evaluate a #if / #elif line: resolve defined(), macro-expand, then evaluate the constant expression. */
static long eval_if(Tok *line) {
	Tok head = {0}, *cur = &head;                          /* pass 1: defined X / defined(X) -> 1/0 */
	for (Tok *t = line; t && t->kind != TEOF; ) {
		if (t->kind == TIDENT && !strcmp(t->text, "defined")) {
			Tok *n = t->next; const char *nm = NULL;
			if (n && is_p(n, "(")) { nm = n->next ? n->next->text : ""; t = n->next && n->next->next ? n->next->next->next : NULL; }
			else { nm = n ? n->text : ""; t = n ? n->next : NULL; }
			cur = cur->next = newtok(TNUM, macro_find(nm) || is_builtin(nm) ? "1" : "0", 1); continue;
		}
		cur = cur->next = copytok(t); t = t->next;
	}
	cur->next = newtok(TEOF, "", 0);
	in_if = 1; Tok *ex = expand(head.next); in_if = 0;     /* pass 2: expand macros (and the __has_* operators) */
	Tok *c = ex; long v = ev_ternary(&c); return v;        /* leftover idents evaluate to 0 in ev_primary */
}

/* ------------------------------------------------------------------ include search ---------------- */
static char cur_dir[PATHMAX];
/* #pragma once: the files that asked, by identity (device + inode: `a.h` and `../inc/a.h` are one file). */
static struct stat cur_file;                           /* the file being driven */
static struct stat *once; static int nonce, oncecap;
static int is_once(const struct stat *st) {
	for (int i = 0; i < nonce; i++) if (once[i].st_dev == st->st_dev && once[i].st_ino == st->st_ino) return 1;
	return 0;
}
static void mark_once(void) {
	if (is_once(&cur_file)) return;
	if (nonce == oncecap) { oncecap = oncecap ? 2 * oncecap : 32; once = realloc(once, oncecap * sizeof *once); if (!once) die("out of memory"); }
	once[nonce++] = cur_file;
}
/* A header: "" first beside the current file, then the include dirs in order. from >= 0 (#include_next): only the
 * dirs after the one the current file was found in. found_idx: where it was found (-1: beside the file). */
static int found_idx;
static char *find_include_from(const char *name, int angle, int from) {
	static char path[PATHMAX]; FILE *f;
	#define MKPATH(...) do { if (snprintf(path, sizeof path, __VA_ARGS__) >= (int)sizeof path) die("include path too long: %s", name); } while (0)
	if (name[0] == '/') { found_idx = -1; MKPATH("%s", name); if ((f = fopen(path, "rb"))) { fclose(f); return path; } return NULL; }
	if (!angle && from < 0) { found_idx = -1; MKPATH("%s%s%s", cur_dir, cur_dir[0] ? "/" : "", name); if ((f = fopen(path, "rb"))) { fclose(f); return path; } }
	for (int i = from < 0 ? 0 : from; i < n_inc_dirs; i++) { MKPATH("%s/%s", inc_dirs[i], name); if ((f = fopen(path, "rb"))) { fclose(f); found_idx = i; return path; } }
	#undef MKPATH
	return NULL;
}
static char *find_include(const char *name, int angle) { return find_include_from(name, angle, -1); }

/* ------------------------------------------------------------------ conditional stack ------------- */
static struct { int active, taken, parent; } cond[256]; static int ncond;
static int active_now(void) { return ncond == 0 ? 1 : cond[ncond - 1].active; }
static void push_cond(int parent, int cond_true) {   /* open a new #if frame */
	if (ncond >= 256) die("too many nested #if (>256)");
	cond[ncond].parent = parent;
	cond[ncond].active = cond_true;
	cond[ncond].taken  = cond_true;
	ncond++;
}

/* ------------------------------------------------------------------ the driver -------------------- */
static void drive(Tok *tok, const char *dir);

/* #pragma: `once`, and GCC's error/warning. Pragmas that only advise the compiler are ignored; those that would
 * change the program's meaning (struct layout, symbol binding, macro state) are rejected, never silently dropped. */
/* A directive line for the compiler, as spelled: `#pragma X...` / `#ident "s"` (GCC's -E passes them on). */
static Tok *line_tok(const char *head, Tok *line) {
	size_t n = strlen(head) + 2; for (Tok *t = line; t && t->kind != TEOF; t = t->next) n += strlen(t->text) + 1;
	char *b = malloc(n), *o = b; o += sprintf(o, "%s", head);
	for (Tok *t = line; t && t->kind != TEOF; t = t->next) { if (t == line || t->space) *o++ = ' '; o += sprintf(o, "%s", t->text); }
	if (!line || line->kind == TEOF) *o++ = ' ';            /* `#pragma` alone: GCC prints "#pragma " */
	*o = 0; Tok *r = newtok(TLINE, b, o - b); free(b); return r;
}
/* #pragma / _Pragma: the ones the PREPROCESSOR acts on are done here and consumed — once, push_macro/pop_macro, GCC
 * poison / system_header / dependency / warning / error; every other one goes to the compiler as a `#pragma` line
 * (GCC's -E does the same: cc decides — it implements pack, weak, visibility, ... and ignores the advisory ones).
 * Returns the line to emit, NULL when consumed. */
typedef struct Saved { char *name; Macro *m; struct Saved *next; } Saved;
static Saved *saved_macros;                               /* push_macro stack */
static const char *pragma_macro_name(Tok *t, const char *what) {   /* ("NAME") */
	if (!t || !is_p(t, "(") || !t->next || t->next->kind != TSTR || !t->next->next || !is_p(t->next->next, ")"))
		die("#pragma %s: expected (\"name\")", what);
	const char *q = t->next->text; size_t l = strlen(q);
	if (q[0] != '"') die("#pragma %s: invalid string %s", what, q);
	char *n = malloc(l); memcpy(n, q + 1, l - 2); n[l - 2] = 0; return n;
}
static Tok *do_pragma(Tok *line) {
	const char *p0 = line && line->kind != TEOF ? line->text : "", *p1 = line && line->kind != TEOF && line->next ? line->next->text : "";
	if (!strcmp(p0, "once")) { if (include_level) mark_once(); return NULL; }
	if (!strcmp(p0, "push_macro")) {
		const char *n = pragma_macro_name(line->next, p0); Saved *s = calloc(1, sizeof *s); s->name = (char *)n;
		Macro *m = macro_find(n); if (m) { s->m = malloc(sizeof *m); *s->m = *m; s->m->next = NULL; }
		s->next = saved_macros; saved_macros = s; return NULL;
	}
	if (!strcmp(p0, "pop_macro")) {
		const char *n = pragma_macro_name(line->next, p0);
		for (Saved **p = &saved_macros; *p; p = &(*p)->next) if (!strcmp((*p)->name, n)) {
			Saved *s = *p; *p = s->next; macro_undef(n);
			if (s->m) { Macro *m = macro_add(n); Macro *nx = m->next; *m = *s->m; m->next = nx; }
			return NULL;
		}
		return NULL;                                         /* nothing pushed: GCC does nothing */
	}
	if (!strcmp(p0, "GCC")) {
		Tok *a = line->next ? line->next->next : NULL;
		if (!strcmp(p1, "poison")) {
			for (; a && a->kind != TEOF; a = a->next) {
				if (a->kind != TIDENT) die("#pragma GCC poison: invalid identifier '%s'", a->text);
				if (macro_find(a->text)) fprintf(stderr, "cpp: warning: poisoning existing macro \"%s\"\n", a->text);
				poisoned = realloc(poisoned, (npoisoned + 1) * sizeof *poisoned); poisoned[npoisoned++] = strdup(a->text);
			}
			return NULL;
		}
		if (!strcmp(p1, "system_header")) return NULL;       /* (its effect: warnings off in the rest of the file) */
		if (!strcmp(p1, "dependency")) {
			if (!a || a->kind != TSTR) die("#pragma GCC dependency: expected a file name");
			char fn[PATHMAX]; size_t l = strlen(a->text); if (l < 2 || l - 2 >= sizeof fn) die("#pragma GCC dependency: bad file name");
			memcpy(fn, a->text + 1, l - 2); fn[l - 2] = 0;
			char *path = find_include(fn, 0); struct stat st;
			if (!path || stat(path, &st)) die("#pragma GCC dependency: cannot find %s", fn);
			if (st.st_mtime > file_mtime) fprintf(stderr, "cpp: warning: current file is older than %s\n", fn);
			return NULL;
		}
		if (!strcmp(p1, "error") || !strcmp(p1, "warning")) {
			char msg[512] = "";
			for (Tok *t = a; t && t->kind != TEOF; t = t->next) strncat(msg, t->text, sizeof msg - strlen(msg) - 1);
			if (!strcmp(p1, "error")) die("#pragma GCC error %s", msg);
			fprintf(stderr, "cpp: warning: %s\n", msg);
			return NULL;
		}
	}
	return line_tok("#pragma", line);
}
/* _Pragma("...") (C99 6.10.9): destringize — drop the L / u8 / u / U prefix and the quotes, \" -> ", \\ -> \ — and
 * run it as a #pragma. */
static Tok *destringize_pragma(const Tok *s) {
	const char *q = s->text; while (*q && *q != '"') q++;
	size_t l = strlen(q); char *b = malloc(l + 1), *o = b;
	for (const char *c = q + 1; c < q + l - 1; c++) { if (*c == '\\' && (c[1] == '"' || c[1] == '\\')) c++; *o++ = *c; }
	*o = 0; Tok *t = tokenize(b); return t;
}

/* `#line N ["file"]` (its operands macro-expanded) and GNU line markers `# N "file" [flags]`: the NEXT line is line N
 * (of "file"). A marker's flag 1 enters an include level, 2 returns from one (GCC's numbering, for __INCLUDE_LEVEL__). */
static void line_control(Tok *t, int marker, Tok *hash, Tok *next_line) {
	const char *what = marker ? "line marker" : "#line";
	if (!t || t->kind != TNUM) die("%s: \"%s\" after #line is not a positive integer", what, t && t->kind != TEOF ? t->text : "");
	for (const char *c = t->text; *c; c++) if (!isdigit((unsigned char)*c)) die("%s: \"%s\" after #line is not a positive integer", what, t->text);
	errno = 0; long long n = strtoll(t->text, NULL, 10);
	if (errno || n > 2147483647LL) die("%s: line number out of range", what);
	Tok *f = t->next;
	if (f && f->kind == TSTR) {
		if (f->text[0] != '"') die("%s: invalid filename %s", what, f->text);
		size_t l = strlen(f->text); char *nm = malloc(l), *o = nm;
		for (const char *c = f->text + 1; c < f->text + l - 1; c++) { if (*c == '\\' && c + 1 < f->text + l - 1) c++; *o++ = *c; }
		*o = 0; presumed = nm; f = f->next;
		if (marker) for (; f && f->kind == TNUM; f = f->next) { if (!strcmp(f->text, "1")) include_level++; else if (!strcmp(f->text, "2") && include_level) include_level--; }
	}
	if (f && f->kind != TEOF && !(marker && f->kind == TNUM)) die("%s: extra tokens at end of directive ('%s')", what, f->text);
	Tok *lastt = hash; for (Tok *q = hash; q && q != next_line && q->kind != TEOF; q = q->next) lastt = q;
	line_delta = (int)n - (lastt->line + 1);                 /* the physical line after the directive is line n */
}
/* Parse a #define directive whose tokens (after "define") start at `t`. */
static void do_define(Tok *t) {
	if (!t || t->kind != TIDENT) die("#define: expected a macro name");
	Macro *m = macro_add(t->text);
	if (t->next && is_p(t->next, "(") && !t->next->space) {   /* function-like: name immediately followed by '(' */
		m->func = 1; Tok *p = t->next->next;
		char *params[64]; int np = 0;
		while (p && !is_p(p, ")")) {
			if (np >= 64) die("too many macro parameters (>64)");
			if (is_p(p, "...")) { m->variadic = 1; params[np++] = xstrdup("__VA_ARGS__"); p = p->next; break; }
			if (p->kind == TIDENT) params[np++] = xstrdup(p->text);
			p = p->next;
			if (p && is_p(p, "...")) { m->variadic = 1; p = p->next; break; }   /* GNU named variadic `args...` */
			if (p && is_p(p, ",")) p = p->next;
		}
		m->nparams = np; m->params = malloc(np * sizeof(char *)); for (int i = 0; i < np; i++) m->params[i] = params[i];
		t = p ? p->next : NULL;   /* body starts after ')' */
	} else t = t->next;          /* object-like: body starts after the name */
	Tok bh = {0}, *bc = &bh;
	for (; t && t->kind != TEOF; t = t->next) bc = bc->next = copytok(t);
	if (bh.next) bh.next->space = 0;
	m->body = bh.next;
}

/* Handle the directive whose '#' is `hash`; return the first token of the NEXT line. */
static Tok *directive(Tok *hash) {
	Tok *name = hash->next;
	/* the directive's argument line = tokens after the name, up to the next bol/EOF */
	Tok *linestart = name ? name->next : NULL;
	Tok *end = linestart; while (end && end->kind != TEOF && (!end->bol || end->cbol)) end = end->next;   /* one past the line */
	/* build a NUL(EOF)-terminated copy of the line for handlers that consume it */
	Tok lh = {0}, *lc = &lh; for (Tok *t = linestart; t && t != end && t->kind != TEOF; t = t->next) lc = lc->next = copytok(t); lc->next = newtok(TEOF, "", 0);
	Tok *line = lh.next;
	const char *d = (name && name->kind == TIDENT) ? name->text : (name ? name->text : "");

	if (!strcmp(d, "if")) {
		int p = active_now();
		push_cond(p, p && eval_if(line) != 0);               /* eval only when the parent is active */
	} else if (!strcmp(d, "ifdef")) {
		int p = active_now();
		push_cond(p, p && (macro_find(line->text) || is_builtin(line->text)));
	} else if (!strcmp(d, "ifndef")) {
		int p = active_now();
		push_cond(p, p && !(macro_find(line->text) || is_builtin(line->text)));
	} else if (!strcmp(d, "elif")) {
		if (ncond) {
			if (!cond[ncond-1].parent || cond[ncond-1].taken) cond[ncond-1].active = 0;   /* no branch left */
			else { int c = eval_if(line) != 0; cond[ncond-1].active = c; cond[ncond-1].taken |= c; }
		}
	} else if (!strcmp(d, "else")) {
		if (ncond) {
			cond[ncond-1].active = cond[ncond-1].parent && !cond[ncond-1].taken;
			cond[ncond-1].taken = 1;
		}
	} else if (!strcmp(d, "endif")) {
		if (ncond) ncond--;
	}
	else if (active_now()) {
		if (!strcmp(d, "define")) {
			do_define(line);
		} else if (!strcmp(d, "undef")) {
			if (is_builtin(line->text) && !macro_find(line->text) && nbuiltin_off < 16) builtin_off[nbuiltin_off++] = strdup(line->text);
			macro_undef(line->text);
		} else if (!strcmp(d, "include") || !strcmp(d, "include_next")) {
			/* filename: a "..." string token, or the tokens between < and > concatenated — or macros that expand to one
			 * (a computed include: the kernel's `#include TRACE_INCLUDE(TRACE_INCLUDE_FILE)`) */
			char fn[PATHMAX]; int angle = 0, next = d[7] == '_';
			if (line->kind != TSTR && !is_p(line, "<")) line = expand(line);
			if (line->kind == TSTR) {
				size_t n = strlen(line->text); memcpy(fn, line->text + 1, n - 2); fn[n - 2] = 0;
			} else if (is_p(line, "<")) {
				angle = 1; fn[0] = 0;
				for (Tok *t = line->next; t && !is_p(t, ">") && t->kind != TEOF; t = t->next)
					strncat(fn, t->text, sizeof fn - strlen(fn) - 1);
			}
			else die("#%s expects \"FILENAME\" or <FILENAME>", d);
			char *path = find_include_from(fn, angle, next && cur_inc_idx >= 0 ? cur_inc_idx + 1 : -1);   /* (#include_next in the main
			                                                                                                 * file: a plain #include, as GCC) */
			if (!path) die("#%s: cannot find '%s'", d, fn);
			int idx = found_idx;
			add_dep(path, idx >= 0 && inc_sys[idx]);
			struct stat st;
			if (stat(path, &st)) die("#include: cannot stat '%s'", path);
			if (is_once(&st)) return end;                  /* #pragma once: already included */
			/* the included file's directory becomes cur_dir for its own "" includes; restore after */
			char full[PATHMAX]; snprintf(full, sizeof full, "%s", path);
			char saved[PATHMAX]; snprintf(saved, sizeof saved, "%s", cur_dir);
			char ndir[PATHMAX]; char *slash = strrchr(full, '/');
			if (slash) { size_t k = slash - full; memcpy(ndir, full, k); ndir[k] = 0; } else ndir[0] = 0;   /* "": the working directory */
			Tok *sub = slurp_tokens(full);
			if (!sub) die("#include: cannot open '%s'", full);
			struct stat outer = cur_file; cur_file = st;
			const char *opres = presumed; int odelta = line_delta; time_t omt = file_mtime;
			presumed = strdup(full); line_delta = 0; file_mtime = st.st_mtime; include_level++;
			int oidx = cur_inc_idx; cur_inc_idx = idx;
			drive(sub, ndir);
			cur_inc_idx = oidx;
			include_level--; presumed = opres; line_delta = odelta; file_mtime = omt;
			cur_file = outer;
			strcpy(cur_dir, saved);
		} else if (!strcmp(d, "error")) {
			char msg[512] = "";
			for (Tok *t = line; t && t->kind != TEOF; t = t->next) {
				strncat(msg, " ", sizeof msg - strlen(msg) - 1);
				strncat(msg, t->text, sizeof msg - strlen(msg) - 1);
			}
			die("#error:%s", msg);
		} else if (!strcmp(d, "warning")) {
			fputs("cpp: warning:", stderr);
			for (Tok *t = line; t && t->kind != TEOF; t = t->next) fprintf(stderr, " %s", t->text);
			fputc('\n', stderr);
		} else if (!strcmp(d, "pragma")) {
			Tok *pl = do_pragma(line); if (pl) emit(pl);
		} else if (!strcmp(d, "ident") || !strcmp(d, "sccs")) {   /* `#ident "s"` for the compiler (it emits .ident) */
			Tok *x = expand(line);
			if (x->kind != TSTR || x->text[0] != '"' || (x->next && x->next->kind != TEOF)) die("invalid #%s directive", d);
			x->next = NULL; emit(line_tok("#ident", x));
		} else if (!strcmp(d, "line") || (name && name->kind == TNUM)) {
			Tok *mk = NULL; if (name->kind == TNUM) { mk = copytok(name); mk->next = line; }   /* `# N "file" flags` */
			line_control(mk ? mk : expand(line), mk != NULL, hash, end);
		} else if (d[0]) {   /* `#` alone is the null directive */
			if (!asm_mode) die("invalid preprocessing directive #%s", d);
			/* GCC's assembler mode: not a directive but a line of text (the assembler takes a leading # as a comment),
			 * macro-expanded like any other */
			Tok th = {0}, *tc = &th; tc = tc->next = copytok(hash);
			for (Tok *t = name; t && t != end && t->kind != TEOF; t = t->next) tc = tc->next = copytok(t);
			tc->next = newtok(TEOF, "", 0);
			emit(expand(th.next));
		}
	}
	return end;
}

/* Process a token list: run directives, skip inactive regions, macro-expand + emit active text runs. */
/* A function-like macro call whose argument list is still open at the end of a run of text (its arguments go on
 * past a directive: `F(a,\n#ifdef X\n b\n#endif\n)` — GCC collects them across it, running the directive). Returns
 * the link that points at the macro's name, or NULL. The outermost open call wins. */
static Tok **open_call(Tok **head) {
	Tok **st[256]; int sp = 0; Tok **prevlink = NULL;
	for (Tok **l = head; *l && (*l)->kind != TEOF; l = &(*l)->next) {
		Tok *t = *l;
		if (is_p(t, "(")) { if (sp < 256) st[sp] = prevlink; sp++; }
		else if (is_p(t, ")") && sp) sp--;
		prevlink = l;
	}
	for (int i = 0; i < sp && i < 256; i++) {
		Tok **nl = st[i]; if (!nl) continue;
		Macro *m = (*nl)->kind == TIDENT ? macro_find((*nl)->text) : NULL;
		if (m && m->func) return nl;
	}
	return NULL;
}
static void drive(Tok *tok, const char *dir) {
	char saved[PATHMAX]; snprintf(saved, sizeof saved, "%s", cur_dir); strncpy(cur_dir, dir, sizeof cur_dir - 1);
	Tok *carry = NULL, **carry_end = &carry;   /* an open macro call, continued in the next active run */
	while (tok && tok->kind != TEOF) {
		if (tok->bol && is_p(tok, "#")) { tok = directive(tok); continue; }
		/* a run of ordinary tokens up to the next directive line / EOF */
		Tok rh = {0}, *rc = &rh;
		while (tok && tok->kind != TEOF && !(tok->bol && is_p(tok, "#"))) { rc = rc->next = copytok(tok); tok = tok->next; }
		rc->next = NULL;
		if (!active_now()) continue;
		*carry_end = rh.next; Tok *run = carry; carry = NULL; carry_end = &carry;
		Tok **oc = (tok && tok->kind != TEOF) ? open_call(&run) : NULL;
		if (oc) { carry = *oc; *oc = NULL; Tok **e = &carry; while (*e) e = &(*e)->next; carry_end = e; }
		Tok **te = &run; while (*te) te = &(*te)->next; *te = newtok(TEOF, "", 0);
		emit(expand(run));
	}
	if (carry) { *carry_end = newtok(TEOF, "", 0); emit(expand(carry)); }   /* (still open at the end: expand reports it) */
	strcpy(cur_dir, saved);
}

/* ------------------------------------------------------------------ predefined macros ------------- */
/* GCC's: the reference compiler's own set for this target (predef.h, generated by gen-predef.py from `gcc -dM`) plus
 * the delta each option group makes. A header that branches on one (musl's setjmp saves d8-d15 under __VFP_FP__) sees
 * exactly what it sees under GCC. */
#include "predef.h"
static void define_line(const char *name, const char *val) {   /* `#define name val`; name may carry "(params)" */
	size_t n = strlen(name) + strlen(val) + 2; char *s = malloc(n); snprintf(s, n, "%s %s", name, val);
	do_define(tokenize(s));
}
static void undef_name(const char *name) {                   /* the name of `F(c)` is F */
	char b[128]; snprintf(b, sizeof b, "%s", name); char *p = strchr(b, '('); if (p) *p = 0;
	macro_undef(b);
}
static void apply_delta(const char *key) {
	for (unsigned i = 0; i < sizeof predef_delta / sizeof *predef_delta; i++)
		if (!strcmp(predef_delta[i].opt, key)) {
			if (predef_delta[i].val) define_line(predef_delta[i].name, predef_delta[i].val); else undef_name(predef_delta[i].name);
		}
}
/* The option state that decides the predefines (GCC's groups; within one, the last option wins). */
static struct {
	char olevel;           /* 0 1 2 3 s z g f(ast) */
	int fast;              /* -ffast-math / -fno-fast-math: 1 / 0; -1: -Ofast decides */
	int pic, pie;          /* -fpic/-fPIC 1/2, -fpie/-fPIE 1/2 (0: -fno-...) */
	const char *std;       /* its predef.h key; NULL: gnu17, the default */
	int freestanding, pthread, undef, assembler, signed_char, trigraphs;
	const char *float_abi; /* NULL (hard) | "soft" | "softfp" */
	int short_wchar, thumb, no_unaligned;
	const char *march, *mfpu;   /* NULL: the target's own (cortex-a7, neon-vfpv4); else a predef.h key */
	const char *ssp;            /* NULL | "-fstack-protector[-strong|-all|-explicit]" */
} po = { '0', -1, 0, 0, NULL, 0, 0, 0, 0, 0, 0, NULL, 0, 0, 0, NULL, NULL, NULL };
static const char *std_key(const char *v) {                 /* -std=VALUE -> its key in predef_delta */
	static const struct { const char *v, *key; } m[] = {
		{"c89","-std=c89"},{"c90","-std=c89"},{"iso9899:1990","-std=c89"},{"gnu89","-std=gnu89"},{"gnu90","-std=gnu89"},
		{"iso9899:199409","-std=iso9899:199409"},{"c99","-std=c99"},{"c9x","-std=c99"},{"iso9899:1999","-std=c99"},
		{"iso9899:199x","-std=c99"},{"gnu99","-std=gnu99"},{"gnu9x","-std=gnu99"},{"c11","-std=c11"},{"c1x","-std=c11"},
		{"iso9899:2011","-std=c11"},{"gnu11","-std=gnu11"},{"gnu1x","-std=gnu11"},{"c17","-std=c17"},{"c18","-std=c17"},
		{"iso9899:2017","-std=c17"},{"iso9899:2018","-std=c17"},{"gnu17",NULL},{"gnu18",NULL},{"c2x","-std=c2x"},
		{"gnu2x","-std=gnu2x"} };
	for (unsigned i = 0; i < sizeof m / sizeof *m; i++) if (!strcmp(v, m[i].v)) return m[i].key;
	die("unsupported option '-std=%s'", v); return NULL;
}
static void predefines(void) {
	for (unsigned i = 0; i < sizeof predef_base / sizeof *predef_base; i++) define_line(predef_base[i][0], predef_base[i][1]);
	static const char *const okey[] = { ['1'] = "-O1", ['2'] = "-O2", ['3'] = "-O3", ['s'] = "-Os", ['z'] = "-Oz", ['g'] = "-Og", ['f'] = "-O3" };
	if (po.olevel != '0') apply_delta(okey[(unsigned char)po.olevel]);
	if (po.fast == 1 || (po.fast < 0 && po.olevel == 'f')) apply_delta("-ffast-math");
	int pic = po.pie ? po.pie : po.pic;                      /* GCC: a PIE is PIC at its level */
	if (po.pie) apply_delta(po.pie == 2 ? "-fPIE" : "-fpie"); else if (pic) apply_delta(pic == 2 ? "-fPIC" : "-fpic");
	if (po.std) apply_delta(po.std);
	if (po.freestanding) apply_delta("-ffreestanding");
	if (po.signed_char) apply_delta("-fsigned-char");
	if (po.pthread) apply_delta("-pthread");
	if (po.short_wchar) apply_delta("-fshort-wchar");
	if (po.march) apply_delta(po.march);
	if (po.mfpu) apply_delta(po.mfpu);
	if (po.float_abi) apply_delta(!strcmp(po.float_abi, "soft") ? "-mfloat-abi=soft" : "-mfloat-abi=softfp");   /* after the FPU's:
	                                                           * soft float takes back the __ARM_FP it defined */
	if (po.no_unaligned) apply_delta("-mno-unaligned-access");
	if (po.thumb) apply_delta("-mthumb");
	if (po.ssp) apply_delta(po.ssp);
	if (po.assembler) apply_delta("-x assembler-with-cpp");
	if (po.undef) {                                          /* -undef: only the standard's own (and -D's, after) */
		static const char *const keep[] = { "__STDC__", "__STDC_VERSION__", "__STDC_HOSTED__", "__STDC_UTF_16__", "__STDC_UTF_32__", "_REENTRANT", "__ASSEMBLER__" };
		for (Macro **p = &macros; *p; ) {
			int k = 0; for (unsigned i = 0; i < sizeof keep / sizeof *keep; i++) k |= !strcmp((*p)->name, keep[i]);
			if (k) p = &(*p)->next; else { strmap_put(&macro_map, (*p)->name, NULL); *p = (*p)->next; }
		}
	}
}
/* -dM: every macro defined at the end, as `#define NAME[(params)] body` (GCC's format). */
static void dump_all(void) {
	for (Macro *m = macros; m; m = m->next) {
		fprintf(out, "#define %s", m->name);
		if (m->func) {
			fputc('(', out);
			for (int i = 0; i < m->nparams; i++) {
				int va = m->variadic && i == m->nparams - 1;
				if (va && !strcmp(m->params[i], "__VA_ARGS__")) fputs("...", out);
				else fprintf(out, "%s%s", m->params[i], va ? "..." : "");
				if (i + 1 < m->nparams) fputc(',', out);
			}
			fputc(')', out);
		}
		fputc(' ', out);
		for (Tok *t = m->body; t; t = t->next) { if (t != m->body && t->space) fputc(' ', out); fputs(t->text, out); }
		fputc('\n', out);
	}
}

int main(int argc, char **argv) {
	const char *in = NULL, *outpath = NULL;
	const char *forced[16]; int nforced = 0;               /* -include FILE: as if #include "FILE" opened the main file */
	char *quote_dirs[64], *sys_dirs[64]; int nquote = 0, nsys = 0; char sys_after[64];   /* -I, then -isystem, then -idirafter (GCC) */
	struct { char kind; const char *arg; } *cmd = malloc(argc * sizeof *cmd); int ncmd = 0;   /* -D / -U, in order */
	#define ARG() (argv[i][2] ? argv[i] + 2 : (i + 1 < argc ? argv[++i] : (die("%s needs an argument", argv[i]), "")))
	#define IS(o) (!strcmp(a, o))
	#define PRE(o) (!strncmp(a, o, strlen(o)))
	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		if (a[0] != '-' || !a[1]) { if (in) die("more than one input file ('%s', '%s')", in, a); in = a; continue; }
		if (PRE("-I")) { if (nquote >= 64) die("too many include directories (>64)"); quote_dirs[nquote++] = (char *)ARG(); }
		else if (IS("-include")) { if (i + 1 >= argc) die("-include needs a file"); if (nforced >= 16) die("too many -include files"); forced[nforced++] = argv[++i]; }
		else if (IS("-imacros")) die("unsupported option '-imacros'");
		else if (IS("-isystem") || IS("-idirafter")) { if (i + 1 >= argc) die("%s needs a directory", a); if (nsys >= 64) die("too many include directories (>64)");
			sys_dirs[nsys] = argv[++i]; sys_after[nsys++] = a[2] == 'd'; }
		else if (IS("-MD")) dep_mode = 1; else if (IS("-MMD")) dep_mode = 2; else if (IS("-MP")) dep_phony = 1;
		else if (IS("-MF")) { if (i + 1 >= argc) die("-MF needs a file"); dep_file = argv[++i]; }
		else if (IS("-MT") || IS("-MQ")) { if (i + 1 >= argc) die("%s needs a target", a); dep_target = argv[++i]; }
		else if (IS("-M") || IS("-MM") || IS("-MG")) die("unsupported option '%s' (use -MD/-MMD with -MF)", a);
		else if (PRE("-D") || PRE("-U")) { char k = a[1]; cmd[ncmd].kind = k; cmd[ncmd++].arg = ARG(); }
		else if (IS("-o")) { if (i + 1 >= argc) die("-o needs a file"); outpath = argv[++i]; }
		else if (IS("-E") || IS("-P") || IS("-nostdinc")) ;
		else if (IS("-C")) ;   /* keep comments: ours are always dropped (the tokens — all a compiler or ld sees — are the same) */   /* preprocess only; we emit no line markers; no default dirs */
		else if (IS("-undef")) po.undef = 1;
		else if (IS("-trigraphs")) po.trigraphs = 1;
		else if (IS("-dM")) dump_macros = 1;
		else if (IS("-x")) { if (i + 1 >= argc) die("-x needs a language"); const char *l = argv[++i];
			if (!strcmp(l, "c")) po.assembler = 0; else if (!strcmp(l, "assembler-with-cpp")) po.assembler = 1; else die("unsupported option '-x %s'", l); }
		else if (PRE("-std=")) po.std = std_key(a + 5);
		else if (IS("-ansi")) po.std = "-std=c89";
		else if (PRE("-O")) {   /* -O = -O1; -O4+ = -O3 (GCC) */
			char l = a[2];
			if (!l) l = '1';
			else if (!strcmp(a + 2, "fast")) l = 'f';
			else if (isdigit((unsigned char)l)) { for (const char *q = a + 2; *q; q++) if (!isdigit((unsigned char)*q)) die("unsupported option '%s'", a); l = atoi(a + 2) > 3 ? '3' : l; }
			else if ((l != 's' && l != 'z' && l != 'g') || a[3]) die("unsupported option '%s'", a);
			po.olevel = l;
		}
		else if (IS("-fpic")) po.pic = 1; else if (IS("-fPIC")) po.pic = 2; else if (IS("-fno-pic") || IS("-fno-PIC")) po.pic = 0;
		else if (IS("-fpie")) po.pie = 1; else if (IS("-fPIE")) po.pie = 2; else if (IS("-fno-pie") || IS("-fno-PIE")) po.pie = 0;
		else if (IS("-ffreestanding") || IS("-fno-hosted")) po.freestanding = 1; else if (IS("-fhosted")) po.freestanding = 0;
		else if (IS("-ffast-math")) po.fast = 1; else if (IS("-fno-fast-math")) po.fast = 0;
		else if (IS("-fsigned-char") || IS("-fno-unsigned-char")) po.signed_char = 1;
		else if (IS("-funsigned-char") || IS("-fno-signed-char")) po.signed_char = 0;
		else if (IS("-pthread")) po.pthread = 1;
		else if (PRE("-mfloat-abi=")) { const char *v = a + 12;
			if (!strcmp(v, "hard")) po.float_abi = NULL; else if (!strcmp(v, "soft") || !strcmp(v, "softfp")) po.float_abi = v; else die("unsupported option '%s'", a); }
		else if (IS("-marm")) po.thumb = 0; else if (IS("-mthumb")) po.thumb = 1;
		else if (IS("-mcpu=cortex-a7")) po.march = NULL;          /* the target's own */
		else if (IS("-march=armv7-a")) po.march = "-march=armv7-a";
		else if (PRE("-march=") || PRE("-mcpu=")) die("unsupported option '%s' (its predefines aren't in predef.h: add it to gen-predef.py)", a);
		else if (IS("-mfpu=neon-vfpv4")) po.mfpu = NULL; else if (IS("-mfpu=vfp")) po.mfpu = "-mfpu=vfp";
		else if (PRE("-mfpu=")) die("unsupported option '%s' (its predefines aren't in predef.h: add it to gen-predef.py)", a);
		else if (IS("-msoft-float")) po.float_abi = "soft";      /* = -mfloat-abi=soft (the same predefines) */
		else if (IS("-mno-unaligned-access")) po.no_unaligned = 1; else if (IS("-munaligned-access")) po.no_unaligned = 0;
		else if (IS("-fshort-wchar")) po.short_wchar = 1; else if (IS("-fno-short-wchar")) po.short_wchar = 0;
		else if (IS("-fstack-protector") || IS("-fstack-protector-strong") || IS("-fstack-protector-all") || IS("-fstack-protector-explicit")) po.ssp = a;
		else if (IS("-fno-stack-protector")) po.ssp = NULL;
		else if (PRE("-fmacro-prefix-map=") || PRE("-ffile-prefix-map=")) {
			const char *v = strchr(a, '=') + 1, *eq = strchr(v, '=');
			if (!eq) die("%s: expected OLD=NEW", a);
			struct PrefixMap *m = malloc(sizeof *m); m->old = strndup(v, (size_t)(eq - v)); m->new_ = eq + 1; m->next = prefix_maps; prefix_maps = m;
		}
		else if (IS("-w") || IS("-pedantic") || IS("-Wpedantic") || (PRE("-W") && !PRE("-Werror") && !PRE("-Wp,")) || PRE("-g")
		         || PRE("-fdiagnostics-") || IS("-fno-show-column") || PRE("-fmessage-length=") || PRE("-ftrack-macro-expansion")
		         || IS("-fdollars-in-identifiers")) ;   /* diagnostics / debug info: no effect on the output */
		else die("unsupported option '%s'", a);
	}
	#undef ARG
	#undef IS
	#undef PRE
	for (int k = 0; k < nquote; k++) { inc_dirs[n_inc_dirs] = quote_dirs[k]; inc_sys[n_inc_dirs++] = 0; }
	for (int pass = 0; pass < 2; pass++) for (int k = 0; k < nsys; k++) if (sys_after[k] == pass) { if (n_inc_dirs >= 64) die("too many include directories (>64)"); inc_dirs[n_inc_dirs] = sys_dirs[k]; inc_sys[n_inc_dirs++] = 1; }
	if (dep_mode && !dep_file) die("-MD/-MMD needs -MF <file>");
	if (!in) die("usage: cpp [-Idir] [-isystem dir] [-Dmacro[=val]] [-Umacro] [-std=...] [-O...] [-f[no-]pic...] [-undef] [-dM] [-o out] in.c");
	if (!po.assembler) { size_t l = strlen(in); if (l > 2 && (!strcmp(in + l - 2, ".S") || (l > 3 && !strcmp(in + l - 3, ".sx")))) po.assembler = 1; }
	lang_for(po.std); if (po.trigraphs) lx.trigraphs = 1;
	asm_mode = po.assembler;
	predefines();
	for (int k = 0; k < ncmd; k++) {                         /* -D NAME[(params)][=val] (val: 1 when absent), -U NAME */
		if (cmd[k].kind == 'U') { macro_undef(cmd[k].arg); continue; }
		char *s = strdup(cmd[k].arg), *eq = strchr(s, '=');
		if (eq) { *eq = 0; define_line(s, eq + 1); } else define_line(s, "1");
	}
	out = outpath ? fopen(outpath, "w") : stdout; if (!out) die("cannot open %s", outpath);
	char dir[PATHMAX]; snprintf(dir, sizeof dir, "%s", in); char *slash = strrchr(dir, '/'); if (slash) *slash = 0; else dir[0] = 0;
	Tok *toks = slurp_tokens(in); if (!toks) die("cannot open %s", in);
	if (stat(in, &cur_file)) die("cannot stat %s", in);
	add_dep(in, 0);
	for (int k = 0; k < nforced; k++) {                   /* GCC: looked up in the working directory first, then the "" chain */
		cur_dir[0] = 0;
		char *p = find_include(forced[k], 0); if (!p) die("-include: cannot find '%s'", forced[k]);
		char full[PATHMAX]; snprintf(full, sizeof full, "%s", p);
		struct stat st; if (stat(full, &st)) die("-include: cannot stat '%s'", full);
		add_dep(full, found_idx >= 0 && inc_sys[found_idx]);
		Tok *sub = slurp_tokens(full); if (!sub) die("-include: cannot open '%s'", full);
		char ndir[PATHMAX]; snprintf(ndir, sizeof ndir, "%s", full); char *sl = strrchr(ndir, '/'); if (sl) *sl = 0; else ndir[0] = 0;
		struct stat outer = cur_file; cur_file = st; presumed = strdup(full); line_delta = 0; file_mtime = st.st_mtime; cur_inc_idx = found_idx;
		drive(sub, ndir);
		cur_file = outer; cur_inc_idx = -1;
	}
	if (stat(in, &cur_file)) die("cannot stat %s", in);
	presumed = base_file = in; file_mtime = cur_file.st_mtime;
	drive(toks, dir);
	if (dump_macros) dump_all();
	write_deps(in, outpath);
	return 0;
}
