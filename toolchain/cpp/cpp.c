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
 * constant expressions, #error, #pragma once (others ignored), #line (ignored). A few predefined macros.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

static void die(const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	fputs("cpp: ", stderr); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap); exit(1);
}
static char *xstrdup(const char *s) { char *p = malloc(strlen(s) + 1); strcpy(p, s); return p; }

/* ------------------------------------------------------------------ tokens + hide sets ------------ */
typedef enum { TIDENT, TNUM, TSTR, TCHAR, TPUNCT, TEOF } Kind;
typedef struct Hide { char *name; struct Hide *next; } Hide;
typedef struct Tok {
	Kind kind; char *text;
	int bol;              /* first token on its source line (=> a '#' here starts a directive) */
	int space;            /* had leading whitespace (for readable output)                      */
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

static Tok *newtok(Kind k, const char *s, size_t n) {
	Tok *t = calloc(1, sizeof *t);
	t->kind = k; t->text = malloc(n + 1); memcpy(t->text, s, n); t->text[n] = 0;
	return t;
}
static Tok *copytok(const Tok *o) {
	Tok *t = calloc(1, sizeof *t); *t = *o;
	t->text = xstrdup(o->text); t->next = NULL;
	return t;
}

/* ------------------------------------------------------------------ tokenizer --------------------- */
/* Multi-char punctuators we must keep whole (## for paste; the rest so #if expressions tokenize right). */
static const char *PUNCT[] = { "<<=", ">>=", "...", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=",
                               "&&", "||", "##", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", NULL };

static Tok *tokenize(const char *src) {
	Tok head = {0}, *cur = &head;
	int bol = 1, space = 0;
	for (const char *p = src; *p; ) {
		if (*p == '\n') { p++; bol = 1; space = 0; continue; }
		if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\f' || *p == '\v') { p++; space = 1; continue; }
		if (p[0] == '\\' && p[1] == '\n') { p += 2; continue; }                 /* line continuation */
		if (p[0] == '/' && p[1] == '/') { while (*p && *p != '\n') p++; continue; }
		if (p[0] == '/' && p[1] == '*') { p += 2; while (*p && !(p[0]=='*'&&p[1]=='/')) p++; if (*p) p += 2; space = 1; continue; }

		const char *s = p; Tok *t;
		if (*p == '"' || *p == '\'') {                                          /* string / char literal */
			char q = *p++; while (*p && *p != q) { if (*p == '\\' && p[1]) p++; p++; } if (*p) p++;
			t = newtok(q == '"' ? TSTR : TCHAR, s, p - s);
		} else if (isdigit((unsigned char)*p) || (*p == '.' && isdigit((unsigned char)p[1]))) {
			p++; while (isalnum((unsigned char)*p) || *p == '.' || ((*p=='+'||*p=='-') && (p[-1]=='e'||p[-1]=='E'))) p++;   /* pp-number */
			t = newtok(TNUM, s, p - s);
		} else if (isalpha((unsigned char)*p) || *p == '_') {
			while (isalnum((unsigned char)*p) || *p == '_') p++;
			t = newtok(TIDENT, s, p - s);
		} else {
			size_t l = 1;
			for (int i = 0; PUNCT[i]; i++) { size_t pl = strlen(PUNCT[i]); if (!strncmp(p, PUNCT[i], pl)) { l = pl; break; } }
			t = newtok(TPUNCT, s, l); p += l;
		}
		t->bol = bol; t->space = space; bol = 0; space = 0;
		cur = cur->next = t;
	}
	cur->next = newtok(TEOF, "", 0); cur->next->bol = 1;
	return head.next;
}
static Tok *slurp_tokens(const char *path) {
	FILE *f = fopen(path, "rb"); if (!f) return NULL;
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
	char *b = malloc(n + 1); if (fread(b, 1, n, f) != (size_t)n) die("read failed: %s", path); b[n] = 0; fclose(f);
	return tokenize(b);
}

/* ------------------------------------------------------------------ macro table ------------------- */
typedef struct Macro {
	char *name; int func; int variadic;
	char **params; int nparams;
	Tok *body;
	struct Macro *next;
} Macro;
static Macro *macros;
static Macro *macro_find(const char *n) {
	for (Macro *m = macros; m; m = m->next) if (!strcmp(m->name, n)) return m;
	return NULL;
}
static void macro_undef(const char *n) {
	for (Macro **p = &macros; *p; p = &(*p)->next)
		if (!strcmp((*p)->name, n)) { *p = (*p)->next; return; }
}
static Macro *macro_add(const char *n) {
	macro_undef(n);                                          /* a redefinition replaces the old one */
	Macro *m = calloc(1, sizeof *m); m->name = xstrdup(n); m->next = macros; macros = m;
	return m;
}

/* ------------------------------------------------------------------ include paths ----------------- */
static char *inc_dirs[64]; static int n_inc_dirs;

/* ------------------------------------------------------------------ output ------------------------ */
static FILE *out;
static void emit(Tok *tok) {
	int line_open = 0;
	for (; tok && tok->kind != TEOF; tok = tok->next) {
		if (tok->bol && line_open) fputc('\n', out);
		else if (tok->space || (tok->bol && !line_open)) fputc(' ', out);
		fputs(tok->text, out); line_open = 1;
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
 * points just past the matching ")". args[i] is a NUL(EOF)-terminated token list for the i-th argument. */
static int read_args(Tok **tok, Tok **args, int max, int variadic, int nparams) {
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
	*tok = t->next;   /* past ")" */
	return n;
}

static Tok *expand(Tok *tok);   /* fwd */

/* Turn a token list into a single string literal (the # operator). */
static Tok *stringize(Tok *arg) {
	char buf[4096]; size_t k = 0; buf[k++] = '"';
	for (Tok *t = arg; t && t->kind != TEOF; t = t->next) {
		if (t->space && k > 1) buf[k++] = ' ';
		for (char *c = t->text; *c && k < sizeof buf - 2; c++) { if (*c == '"' || *c == '\\') buf[k++] = '\\'; buf[k++] = *c; }
	}
	buf[k++] = '"'; buf[k] = 0;
	return newtok(TSTR, buf, k);
}
/* Paste two tokens' spellings into one new token (the ## operator). */
static Tok *paste(Tok *a, Tok *b) {
	char buf[512]; snprintf(buf, sizeof buf, "%s%s", a->text, b ? b->text : "");
	Kind k = (isalpha((unsigned char)buf[0]) || buf[0] == '_') ? TIDENT : isdigit((unsigned char)buf[0]) ? TNUM : TPUNCT;
	return newtok(k, buf, strlen(buf));
}

/* Substitute args into a function-macro body, handling # and ##; params expanded unless adjacent to #/##. */
static Tok *subst(Macro *m, Tok **args, int nargs) {
	Tok head = {0}, *cur = &head;
	for (Tok *t = m->body; t; ) {
		int pi = -1;
		if (t->kind == TIDENT) for (int i = 0; i < m->nparams; i++) if (!strcmp(t->text, m->params[i])) pi = i;
		int va = (m->variadic && t->kind == TIDENT && !strcmp(t->text, "__VA_ARGS__"));

		if (t->text[0] == '#' && !t->text[1] && t->next) {          /* # param  -> stringize */
			int qi = -1; for (int i = 0; i < m->nparams; i++) if (!strcmp(t->next->text, m->params[i])) qi = i;
			Tok *a = (qi >= 0 && qi < nargs) ? args[qi] : NULL;
			cur = cur->next = stringize(a); t = t->next->next; continue;
		}
		if (t->next && t->next->text[0] == '#' && t->next->text[1] == '#') {   /* lhs ## rhs -> paste */
			Tok *lhs = (pi >= 0 && pi < nargs) ? args[pi] : NULL;
			/* place all-but-last of lhs, then paste last-of-lhs with first-of-rhs below */
			Tok *lcopy = lhs ? copy_body(lhs, NULL) : NULL;
			Tok *ltail = last(lcopy);
			while (lcopy && lcopy != ltail) { cur = cur->next = lcopy; lcopy = lcopy->next; }
			Tok *rhs = t->next->next;
			int ri = -1; if (rhs && rhs->kind == TIDENT) for (int i = 0; i < m->nparams; i++) if (!strcmp(rhs->text, m->params[i])) ri = i;
			Tok *rfirst; Tok *rrest;
			if (ri >= 0 && ri < nargs) { Tok *rc = copy_body(args[ri], NULL); rfirst = rc; rrest = rc ? rc->next : NULL; }
			else { rfirst = rhs ? copytok(rhs) : NULL; rrest = NULL; }
			Tok *lastl = (pi >= 0) ? ltail : copytok(t);
			cur = cur->next = paste(lastl ? lastl : t, rfirst);
			for (Tok *x = rrest; x; x = x->next) cur = cur->next = copytok(x);
			t = t->next->next->next;   /* skip lhs? no: skip '##' and rhs token */
			continue;
		}
		if (pi >= 0 && pi < nargs) {                                 /* a parameter: splice its EXPANDED argument */
			for (Tok *a = expand(copy_body(args[pi], NULL)); a && a->kind != TEOF; a = a->next)
				cur = cur->next = copytok(a);
			t = t->next; continue;
		}
		if (va) {                                                   /* __VA_ARGS__: the trailing variadic argument */
			int i = m->nparams - 1;
			if (i < nargs) for (Tok *a = args[i]; a && a->kind != TEOF; a = a->next) cur = cur->next = copytok(a);
			t = t->next; continue;
		}
		cur = cur->next = copytok(t); t = t->next;                  /* ordinary body token */
	}
	return head.next;
}

/* Core: expand all macros in a token list, returning a fresh list (used for text and #if lines). */
static Tok *expand(Tok *tok) {
	Tok head = {0}, *cur = &head;
	while (tok && tok->kind != TEOF) {
		if (tok->kind == TIDENT && !hide_has(tok->hide, tok->text)) {
			Macro *m = macro_find(tok->text);
			if (m && !m->func) {                                    /* object-like */
				Hide *hs = hide_add(tok->hide, m->name);
				Tok *rep = copy_body(m->body, hs);
				if (rep) { rep->space = tok->space; rep->bol = tok->bol; last(rep)->next = tok->next; tok = rep; }
				else tok = tok->next;
				continue;
			}
			if (m && m->func && tok->next && tok->next->text[0] == '(' && !tok->next->text[1]) {   /* function-like */
				Tok *args[64]; Tok *open = tok->next; (void)open;
				Tok *after = tok; Tok *paren = tok->next;
				int nargs = read_args(&paren, args, 64, m->variadic, m->nparams);
				(void)nargs; (void)after;
				Tok *rep = subst(m, args, nargs);
				Hide *hs = hide_add(tok->hide, m->name);
				rep = copy_body(rep, hs);
				if (rep) { rep->space = tok->space; rep->bol = tok->bol; last(rep)->next = paren; tok = rep; } else tok = paren;
				continue;
			}
		}
		cur = cur->next = tok; tok = tok->next;
	}
	cur->next = tok;   /* the EOF */
	return head.next;
}

/* ------------------------------------------------------------------ #if expression evaluator ------ */
static long ev_ternary(Tok **t);
static long tok_num(Tok *t) {
	if (t->kind == TCHAR) { const char *p = t->text + 1; if (*p == '\\') { p++; switch (*p) { case 'n': return '\n'; case 't': return '\t'; case '0': return 0; case 'r': return '\r'; default: return *p; } } return (unsigned char)*p; }
	return strtol(t->text, NULL, 0);
}
static int is_p(Tok *t, const char *s) { return t->kind == TPUNCT && !strcmp(t->text, s); }
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
			cur = cur->next = newtok(TNUM, macro_find(nm) ? "1" : "0", 1); continue;
		}
		cur = cur->next = copytok(t); t = t->next;
	}
	cur->next = newtok(TEOF, "", 0);
	Tok *ex = expand(head.next);                           /* pass 2: expand macros */
	Tok *c = ex; long v = ev_ternary(&c); return v;        /* leftover idents evaluate to 0 in ev_primary */
}

/* ------------------------------------------------------------------ include search ---------------- */
static char cur_dir[512];
static char *find_include(const char *name, int angle) {
	static char path[1024]; FILE *f;
	if (!angle && cur_dir[0]) { snprintf(path, sizeof path, "%s/%s", cur_dir, name); if ((f = fopen(path, "rb"))) { fclose(f); return path; } }
	for (int i = 0; i < n_inc_dirs; i++) { snprintf(path, sizeof path, "%s/%s", inc_dirs[i], name); if ((f = fopen(path, "rb"))) { fclose(f); return path; } }
	return NULL;
}

/* ------------------------------------------------------------------ conditional stack ------------- */
static struct { int active, taken, parent; } cond[256]; static int ncond;
static int active_now(void) { return ncond == 0 ? 1 : cond[ncond - 1].active; }
static void push_cond(int parent, int cond_true) {   /* open a new #if frame */
	cond[ncond].parent = parent;
	cond[ncond].active = cond_true;
	cond[ncond].taken  = cond_true;
	ncond++;
}

/* ------------------------------------------------------------------ the driver -------------------- */
static void drive(Tok *tok, const char *dir);

/* Parse a #define directive whose tokens (after "define") start at `t`. */
static void do_define(Tok *t) {
	if (!t || t->kind != TIDENT) die("#define: expected a macro name");
	Macro *m = macro_add(t->text);
	if (t->next && is_p(t->next, "(") && !t->next->space) {   /* function-like: name immediately followed by '(' */
		m->func = 1; Tok *p = t->next->next;
		char *params[64]; int np = 0;
		while (p && !is_p(p, ")")) {
			if (is_p(p, "...")) { m->variadic = 1; params[np++] = xstrdup("__VA_ARGS__"); p = p->next; break; }
			if (p->kind == TIDENT) params[np++] = xstrdup(p->text);
			p = p->next; if (p && is_p(p, ",")) p = p->next;
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
	Tok *end = linestart; while (end && end->kind != TEOF && !end->bol) end = end->next;   /* one past the line */
	/* build a NUL(EOF)-terminated copy of the line for handlers that consume it */
	Tok lh = {0}, *lc = &lh; for (Tok *t = linestart; t && t != end && t->kind != TEOF; t = t->next) lc = lc->next = copytok(t); lc->next = newtok(TEOF, "", 0);
	Tok *line = lh.next;
	const char *d = (name && name->kind == TIDENT) ? name->text : (name ? name->text : "");

	if (!strcmp(d, "if")) {
		int p = active_now();
		push_cond(p, p && eval_if(line) != 0);               /* eval only when the parent is active */
	} else if (!strcmp(d, "ifdef")) {
		int p = active_now();
		push_cond(p, p && macro_find(line->text) != NULL);
	} else if (!strcmp(d, "ifndef")) {
		int p = active_now();
		push_cond(p, p && macro_find(line->text) == NULL);
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
			macro_undef(line->text);
		} else if (!strcmp(d, "include")) {
			/* filename: a "..." string token, or the tokens between < and > concatenated */
			char fn[512]; int angle = 0;
			if (line->kind == TSTR) {
				size_t n = strlen(line->text); memcpy(fn, line->text + 1, n - 2); fn[n - 2] = 0;
			} else if (is_p(line, "<")) {
				angle = 1; fn[0] = 0;
				for (Tok *t = line->next; t && !is_p(t, ">") && t->kind != TEOF; t = t->next)
					strncat(fn, t->text, sizeof fn - strlen(fn) - 1);
			}
			char *path = find_include(fn, angle);
			if (!path) die("#include: cannot find '%s'", fn);
			/* the included file's directory becomes cur_dir for its own "" includes; restore after */
			char full[512]; strcpy(full, path);
			char saved[512]; strcpy(saved, cur_dir);
			char ndir[512]; char *slash = strrchr(full, '/');
			if (slash) { size_t k = slash - full; memcpy(ndir, full, k); ndir[k] = 0; } else strcpy(ndir, ".");
			Tok *sub = slurp_tokens(full);
			if (!sub) die("#include: cannot open '%s'", full);
			drive(sub, ndir);
			strcpy(cur_dir, saved);
		} else if (!strcmp(d, "error")) {
			char msg[512] = "";
			for (Tok *t = line; t && t->kind != TEOF; t = t->next) {
				strncat(msg, " ", sizeof msg - strlen(msg) - 1);
				strncat(msg, t->text, sizeof msg - strlen(msg) - 1);
			}
			die("#error:%s", msg);
		}
		/* #pragma, #line, #warning, #ident, unknown: ignored (our headers guard with #ifndef, not #pragma once) */
	}
	return end;
}

/* Process a token list: run directives, skip inactive regions, macro-expand + emit active text runs. */
static void drive(Tok *tok, const char *dir) {
	char saved[512]; strcpy(saved, cur_dir); strncpy(cur_dir, dir, sizeof cur_dir - 1);
	while (tok && tok->kind != TEOF) {
		if (tok->bol && is_p(tok, "#")) { tok = directive(tok); continue; }
		/* a run of ordinary tokens up to the next directive line / EOF */
		Tok rh = {0}, *rc = &rh;
		while (tok && tok->kind != TEOF && !(tok->bol && is_p(tok, "#"))) { rc = rc->next = copytok(tok); tok = tok->next; }
		rc->next = newtok(TEOF, "", 0);
		if (active_now()) emit(expand(rh.next));
	}
	strcpy(cur_dir, saved);
}

/* ------------------------------------------------------------------ predefined macros ------------- */
static void predef(const char *name, const char *val) {
	Macro *m = macro_add(name);
	m->body = (val && *val) ? tokenize(val) : NULL;
	if (!m->body) return;
	Tok *e = m->body;                                        /* drop the tokenizer's trailing TEOF */
	while (e->next && e->next->kind != TEOF) e = e->next;
	e->next = NULL;
	m->body->space = 0; m->body->bol = 0;
}

int main(int argc, char **argv) {
	const char *in = NULL, *outpath = NULL;
	predef("__STDC__", "1"); predef("__STDC_HOSTED__", "0"); predef("__arm__", "1"); predef("__ARM_EABI__", "1");
	/* Compiler-predefined type macros (ARM32 EABI), as gcc supplies for <stdint.h> etc. Our cc collapses */
	predef("__INT8_TYPE__", "signed char");   predef("__UINT8_TYPE__", "unsigned char");    /* all integer  */
	predef("__INT16_TYPE__", "short");         predef("__UINT16_TYPE__", "unsigned short");  /* types to a   */
	predef("__INT32_TYPE__", "int");           predef("__UINT32_TYPE__", "unsigned int");    /* 4-byte int,  */
	predef("__INT64_TYPE__", "long long");     predef("__UINT64_TYPE__", "unsigned long long"); /* so exact  */
	predef("__INTMAX_TYPE__", "long long");    predef("__UINTMAX_TYPE__", "unsigned long long"); /* names    */
	predef("__INTPTR_TYPE__", "int");          predef("__UINTPTR_TYPE__", "unsigned int");   /* only matter  */
	predef("__SIZE_TYPE__", "unsigned int");   predef("__PTRDIFF_TYPE__", "int");            /* for parsing. */
	predef("__WCHAR_TYPE__", "unsigned int");  predef("__CHAR_BIT__", "8");
	predef("__SIZEOF_INT__", "4"); predef("__SIZEOF_LONG__", "4"); predef("__SIZEOF_POINTER__", "4");
	predef("__SCHAR_MAX__", "127"); predef("__SHRT_MAX__", "32767"); predef("__INT_MAX__", "2147483647");
	predef("__LONG_MAX__", "2147483647L"); predef("__LONG_LONG_MAX__", "9223372036854775807LL");
	predef("__INTMAX_MAX__", "9223372036854775807LL"); predef("__WCHAR_MAX__", "2147483647"); predef("__SIZE_MAX__", "4294967295U");
	for (int i = 1; i < argc; i++) {
		if (!strncmp(argv[i], "-I", 2)) inc_dirs[n_inc_dirs++] = argv[i][2] ? argv[i] + 2 : argv[++i];
		else if (!strcmp(argv[i], "-isystem")) inc_dirs[n_inc_dirs++] = argv[++i];
		else if (!strncmp(argv[i], "-D", 2)) {
			char *s = argv[i][2] ? argv[i] + 2 : argv[++i];      /* -DNAME or -DNAME=val */
			char *eq = strchr(s, '=');
			if (eq) { *eq = 0; predef(s, eq + 1); } else predef(s, "1");
		}
		else if (!strcmp(argv[i], "-o")) outpath = argv[++i];
		else if (argv[i][0] != '-') in = argv[i];
	}
	if (!in) die("usage: cpp [-Idir] [-Dmacro[=val]] [-o out] in.c");
	out = outpath ? fopen(outpath, "w") : stdout; if (!out) die("cannot open %s", outpath);
	char dir[512]; strcpy(dir, in); char *slash = strrchr(dir, '/'); if (slash) *slash = 0; else strcpy(dir, ".");
	Tok *toks = slurp_tokens(in); if (!toks) die("cannot open %s", in);
	drive(toks, dir);
	return 0;
}
