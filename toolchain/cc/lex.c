/*
 * lex.c — the lexer: turn C source text into a linked list of tokens. Skips whitespace and both comment
 * styles, recognizes integer literals (decimal / 0x hex / 0 octal), identifiers, the M1 keywords, and the
 * C punctuators (multi-character ones matched before their single-character prefixes so "==" beats "=").
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "cc.h"

/* One UTF-8 sequence at p -> *cp; returns its length (an invalid lead byte is taken as one raw byte). */
static int utf8_decode(const char *p, unsigned *cp) {
	const unsigned char *u = (const unsigned char *)p;
	int n = u[0] >= 0xf0 ? 4 : u[0] >= 0xe0 ? 3 : u[0] >= 0xc0 ? 2 : 1;
	unsigned v = n == 1 ? u[0] : u[0] & (0x7f >> n);
	for (int i = 1; i < n; i++) { if ((u[i] & 0xc0) != 0x80) { *cp = u[0]; return 1; } v = (v << 6) | (u[i] & 0x3f); }
	*cp = v; return n;
}
/* A wide literal's raw text -> code points: C escapes (\ooo up to 3 digits, \x all hex digits, \n ...) and
 * UTF-8 multibyte characters. Returns the count (excluding the terminator), dying past cap. */
int wstr_decode(const char *s, unsigned *out, int cap) {
	int n = 0;
	while (*s) {
		unsigned v;
		if (*s == '\\') {
			s++;
			switch (*s) {
			case 'n': v = '\n'; s++; break; case 't': v = '\t'; s++; break; case 'r': v = '\r'; s++; break;
			case 'a': v = '\a'; s++; break; case 'b': v = '\b'; s++; break; case 'f': v = '\f'; s++; break; case 'v': v = '\v'; s++; break;
			case 'e': case 'E': v = 27; s++; break;   /* (GNU: ESC) */
			case 'x': s++; v = 0; while (isxdigit((unsigned char)*s)) { v = v * 16 + (isdigit((unsigned char)*s) ? *s - '0' : (*s | 32) - 'a' + 10); s++; } break;
			default:
				if (*s >= '0' && *s <= '7') { v = 0; for (int k = 0; k < 3 && *s >= '0' && *s <= '7'; k++, s++) v = v * 8 + (*s - '0'); }
				else { v = (unsigned char)*s; s++; }
			}
		} else if ((unsigned char)*s >= 0x80) s += utf8_decode(s, &v);
		else v = (unsigned char)*s++;
		if (n >= cap) die("lex: wide literal too long");
		out[n++] = v;
	}
	return n;
}
/* GNU alternate keyword spellings, canonicalized here so the parser only ever sees one form. */
static const char *const GNU_SPELLING[][2] = {
	{ "__attribute", "__attribute__" }, { "__signed", "signed" }, { "__signed__", "signed" },
	{ "__const", "const" }, { "__const__", "const" }, { "__volatile", "volatile" }, { "__volatile__", "volatile" },
	{ "__restrict", "restrict" }, { "__restrict__", "restrict" }, { "__inline", "inline" }, { "__inline__", "inline" },
	{ "__asm", "asm" }, { "__asm__", "asm" }, { "__typeof", "typeof" }, { "__typeof__", "typeof" },
	{ "__alignof", "_Alignof" }, { "__alignof__", "_Alignof" }, { "alignof", "_Alignof" },
	{ "__thread", "_Thread_local" }, { "thread_local", "_Thread_local" }, { "alignas", "_Alignas" },
	{ "__complex__", "_Complex" }, { "__complex", "_Complex" }, { "__real", "__real__" }, { "__imag", "__imag__" }, { "", "" } };
static const char *KEYWORDS[] = {
	"int", "char", "void", "short", "long", "signed", "unsigned", "float", "double",   /* base arithmetic types */
	"struct", "union", "enum", "typedef",                                   /* aggregate + alias       */
	"const", "volatile", "restrict", "static", "extern", "register", "inline", "sizeof", "__attribute__",  /* qualifiers/storage/op */
	"_Thread_local", "_Noreturn", "_Alignas",                               /* C11: thread storage duration (GNU __thread), noreturn, alignment */
	"__extension__", "_Bool", "_Generic",                                   /* GNU no-op prefix; C99 bool; C11 _Generic */
	"_Complex", "__real__", "__imag__",                                     /* C99 complex; GNU part operators */
	"auto",                                                                 /* storage class */
	"_Alignof", "typeof", "asm",                                            /* alignof operator; GNU typeof; inline assembly */
	"__label__",                                                            /* GNU local-label declaration */
	"__auto_type",                                                          /* GNU type inference (kernel min/max) */
	"return", "if", "else", "while", "do", "for", "break", "continue", "switch", "case", "default", "goto",  /* control flow */
	NULL };
/* Longest punctuators first so a prefix (e.g. "<") never shadows a longer match (e.g. "<<" / "<="). */
static const char *PUNCT[] = { "<<=", ">>=", "...",                                 /* 3-char first    */
                               "<<", ">>", "==", "!=", "<=", ">=", "&&", "||", "->", "++", "--",
                               "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=",       /* compound assign */
                               "+","-","*","/","%","(",")","{","}","[","]",";",",",".","=","<",">","&","|","^","~","!","?",":", NULL };

/* ---- directives in preprocessed input: line markers (skipped), #ident, #pragma ------------------------------------
 * A pragma the compiler acts on is done here, in source order: `pack` and `GCC visibility push/pop` are state stamped on
 * every later token (the parser reads it where it applies: a struct's closing brace, a declaration's name), `weak` and
 * `redefine_extname` name symbols (the parser applies them). GCC's advisory ones (diagnostic, optimize, STDC ...) and
 * pragmas GCC doesn't know are ignored (C11 6.10.6); the ones that change code we don't implement die. */
static int pack_cur, pack_stack[64], npack;
static int vis_stack[64], nvis;
PragmaName *pragma_weaks, *pragma_redefines, *idents;
static void pname_add(PragmaName **l, const char *n, size_t nl, const char *t, size_t tl) {
	PragmaName *e = calloc(1, sizeof *e); e->name = strndup(n, nl); e->target = t ? strndup(t, tl) : NULL;
	PragmaName **p = l; while (*p) p = &(*p)->next; *p = e;   /* in source order */
}
static const char *skip_blank(const char *p) { while (*p == ' ' || *p == '\t') p++; return p; }
static const char *word(const char *p, size_t *n) {   /* an identifier at p: its length in *n (0: none) */
	const char *s = p; while (isalnum((unsigned char)*p) || *p == '_' || *p == '$') p++; *n = p - s; return s;
}
static void pragma(const char *p, const char *eol, int line) {
	char buf[512]; size_t len = eol - p < (long)sizeof buf - 1 ? (size_t)(eol - p) : sizeof buf - 1; memcpy(buf, p, len); buf[len] = 0;
	size_t n; const char *w = word(skip_blank(buf), &n), *q = skip_blank(w + n);
	#define IS(s) (n == strlen(s) && !strncmp(w, s, n))
	if (IS("pack")) {   /* (n) () (push[, n]) (pop): the maximum alignment of the members of later structs */
		if (*q != '(') die("lex: #pragma pack: expected '(' on line %d", line);
		q = skip_blank(q + 1);
		size_t k; const char *a = word(q, &k); int push = 0, pop = 0;
		if (k == 4 && !strncmp(a, "push", 4)) push = 1; else if (k == 3 && !strncmp(a, "pop", 3)) pop = 1;
		if (push || pop) { q = skip_blank(a + k); if (*q == ',') q = skip_blank(q + 1); }
		int val = -1;
		if (isdigit((unsigned char)*q)) { val = (int)strtol(q, (char **)&q, 0); q = skip_blank(q); }
		else if (isalpha((unsigned char)*q) || *q == '_') die("lex: #pragma pack with an identifier is not supported (line %d)", line);
		if (*q != ')') die("lex: #pragma pack: malformed (line %d)", line);
		if (val >= 0 && val != 0 && val != 1 && val != 2 && val != 4 && val != 8 && val != 16)
			die("lex: #pragma pack: alignment must be a small power of two, not %d (line %d)", val, line);
		if (push) { if (npack == 64) die("lex: #pragma pack: push too deep"); pack_stack[npack++] = pack_cur; if (val >= 0) pack_cur = val; }
		else if (pop) { if (val >= 0) die("lex: #pragma pack(pop, n) is not supported (line %d)", line); if (npack) pack_cur = pack_stack[--npack]; }
		else pack_cur = val < 0 ? 0 : val;                  /* pack() : the default */
	} else if (IS("weak")) {   /* weak NAME [= TARGET] */
		size_t k; const char *a = word(q, &k); if (!k) die("lex: #pragma weak: expected a name (line %d)", line);
		q = skip_blank(a + k);
		if (*q == '=') { size_t tk2; const char *t = word(skip_blank(q + 1), &tk2); if (!tk2) die("lex: #pragma weak: expected a target (line %d)", line); pname_add(&pragma_weaks, a, k, t, tk2); }
		else pname_add(&pragma_weaks, a, k, NULL, 0);
	} else if (IS("redefine_extname")) {
		size_t k1, k2; const char *a = word(q, &k1), *b = word(skip_blank(a + k1), &k2);
		if (!k1 || !k2) die("lex: #pragma redefine_extname: expected two names (line %d)", line);
		pname_add(&pragma_redefines, a, k1, b, k2);
	} else if (IS("GCC")) {
		size_t k; const char *a = word(q, &k); q = skip_blank(a + k);
		#define ISA(s) (k == strlen(s) && !strncmp(a, s, k))
		if (ISA("visibility")) {
			size_t k2; const char *op = word(q, &k2); q = skip_blank(op + k2);
			if (k2 == 3 && !strncmp(op, "pop", 3)) { if (!nvis) die("lex: #pragma GCC visibility pop with no push (line %d)", line); nvis--; }
			else if (k2 == 4 && !strncmp(op, "push", 4)) {
				if (*q != '(') die("lex: #pragma GCC visibility push: expected '(' (line %d)", line);
				size_t k3; const char *v = word(skip_blank(q + 1), &k3);
				int vis = k3 == 7 && !strncmp(v, "default", 7) ? VIS_DEFAULT : k3 == 6 && !strncmp(v, "hidden", 6) ? VIS_HIDDEN
				        : k3 == 8 && !strncmp(v, "internal", 8) ? VIS_INTERNAL : k3 == 9 && !strncmp(v, "protected", 9) ? VIS_PROTECTED : 0;
				if (!vis) die("lex: #pragma GCC visibility push: unknown visibility (line %d)", line);
				if (nvis == 64) die("lex: #pragma GCC visibility: push too deep");
				vis_stack[nvis++] = vis;
			} else die("lex: #pragma GCC visibility: expected push or pop (line %d)", line);
		} else if (ISA("target") || ISA("pch_preprocess")) die("lex: #pragma GCC %.*s is not supported (line %d)", (int)k, a, line);
		/* diagnostic optimize push_options pop_options reset_options unroll ivdep novector system_header poison
		 * dependency warning error, and GCC pragmas we don't know: nothing to do in the code */
		#undef ISA
	} else if (IS("scalar_storage_order")) die("lex: #pragma scalar_storage_order is not supported (line %d)", line);
	else if (IS("message")) fprintf(stderr, "cc: note: #pragma message: %s\n", q);
	/* STDC ..., omp / acc (no -fopenmp), and unknown pragmas: ignored, as GCC does */
	#undef IS
}
static const char *directive(const char *p, int line) {   /* p just past the '#'; returns the end of its line */
	const char *eol = p; while (*eol && *eol != '\n') eol++;
	p = skip_blank(p);
	size_t n; const char *w = word(p, &n);
	if (isdigit((unsigned char)*p) || (n == 4 && !strncmp(w, "line", 4)) || p == eol) return eol;   /* a line marker; `#` alone */
	if (n == 6 && !strncmp(w, "pragma", 6)) { pragma(w + 6, eol, line); return eol; }
	if (n == 5 && !strncmp(w, "ident", 5)) {                 /* #ident "string" -> .ident */
		const char *s = skip_blank(w + 5), *e = eol; while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r')) e--;
		if (*s != '"' || e - s < 2 || e[-1] != '"') die("lex: #ident: expected a string on line %d", line);
		pname_add(&idents, s, e - s, NULL, 0); return eol;
	}
	die("lex: unexpected #%.*s directive in preprocessed input (line %d)", (int)(n ? n : 1), n ? w : p, line);
	return eol;
}
/* Token lexemes: copied whole into an append-only arena (tokens live as long as the compile) */
static char *tx_arena; static size_t tx_left;
static const char *tok_text(const char *s, size_t n) {
	if (n + 1 > tx_left) { tx_left = n + 1 > (1u << 20) ? n + 1 : (1u << 20); tx_arena = malloc(tx_left); if (!tx_arena) die("lex: out of memory"); }
	char *d = tx_arena; memcpy(d, s, n); d[n] = 0; tx_arena += n + 1; tx_left -= n + 1; return d;
}
static Token *new_tok(TokKind kind, int line) {
	Token *t = calloc(1, sizeof *t); t->kind = kind; t->line = line; t->text = "";
	t->pack = (unsigned char)pack_cur; t->vis = nvis ? (unsigned char)vis_stack[nvis - 1] : 0;
	return t;
}

Token *lex(const char *src) {
	Token head = {0}, *cur = &head;
	int line = 1, bol = 1;
	for (const char *p = src; *p; ) {
		if (*p == '\n') { line++; p++; bol = 1; continue; }
		if (isspace((unsigned char)*p)) { p++; continue; }
		if (*p == '#' && bol) { p = directive(p + 1, line); continue; }                   /* a directive line cpp passed on */
		bol = 0;
		if (p[0] == '/' && p[1] == '/') { while (*p && *p != '\n') p++; continue; }      /* line comment  */
		if (p[0] == '/' && p[1] == '*') { p += 2; while (*p && !(p[0]=='*'&&p[1]=='/')) { if(*p=='\n')line++; p++; } if(*p) p+=2; continue; }

		int wide = 0;                                                                    /* literal prefix: L/U (4-byte), u (2), u8 (plain) */
		if ((p[0] == 'L' || p[0] == 'U') && (p[1] == '\'' || p[1] == '"')) { wide = p[0] == 'L' && short_wchar ? 2 : 4; p++; }   /* L: wchar_t */
		else if (p[0] == 'u' && p[1] == '8' && p[2] == '"') p += 2;
		else if (p[0] == 'u' && (p[1] == '\'' || p[1] == '"')) { wide = 2; p++; }
		if (*p == '\'') {                                                                /* char literal 'x' / '\n' / '\001' / '\xff' */
			long v; p++;
			if (*p == '\\') { p++;
				switch (*p) {
				case 'n': v='\n'; p++; break; case 't': v='\t'; p++; break; case 'r': v='\r'; p++; break;
				case 'a': v='\a'; p++; break; case 'b': v='\b'; p++; break; case 'f': v='\f'; p++; break;
				case 'v': v='\v'; p++; break; case '\\': v='\\'; p++; break; case '\'': v='\''; p++; break;
				case 'e': case 'E': v = 27; p++; break;   /* (GNU: ESC) */
				case '"': v='"'; p++; break; case '?': v='?'; p++; break;
				case 'x': { p++; v = 0;                                          /* \xHH… hex escape */
					for (int d; (d = (*p>='0'&&*p<='9') ? *p-'0' : (*p|32)>='a'&&(*p|32)<='f' ? (*p|32)-'a'+10 : -1) >= 0; p++) v = v*16 + d;
					break; }
				default:
					if (*p >= '0' && *p <= '7') { v = 0; for (int k = 0; k < 3 && *p>='0' && *p<='7'; k++, p++) v = v*8 + (*p-'0'); }   /* \NNN octal */
					else { v = (unsigned char)*p; p++; }
				}
			} else if (wide && (unsigned char)*p >= 0x80) { unsigned cp; p += utf8_decode(p, &cp); v = cp; }   /* L'Ä': the code point */
			else { v = (unsigned char)*p; p++; }
			if (*p != '\'') die("lex: unterminated char literal on line %d", line);
			p++;
			Token *t = new_tok(TK_NUM, line); t->val = v; t->wide = wide; t->text = "'"; cur = cur->next = t; continue;   /* (text: a char constant) */
		}
		if (*p == '"') {                                                                 /* string literal */
			const char *s = ++p;                                                         /* skip opening quote */
			while (*p && *p != '"') { if (*p == '\\' && p[1]) p += 2; else p++; }         /* keep escapes intact */
			Token *t = new_tok(TK_STR, line); t->wide = wide;
			size_t full = p - s;
			t->sval = malloc(full + 1); memcpy(t->sval, s, full); t->sval[full] = 0;     /* full raw text (as spelled) — unbounded */
			t->text = t->sval;
			if (*p == '"') p++;                                                          /* skip closing quote */
			cur = cur->next = t; continue;
		}
		if (isdigit((unsigned char)*p) || (*p == '.' && isdigit((unsigned char)p[1]))) {   /* number */
			Token *t = new_tok(TK_NUM, line);
			char *end, *fend; t->val = (long)strtoull(p, &end, 0);                     /* 0x.. / 0.. / dec; strtoull: literals > LONG_MAX (e.g. 64-bit hash primes) must wrap, not saturate */
			double fv = strtod(p, &fend);
			if (p[0] == '0' && (p[1] == 'b' || p[1] == 'B') && (p[2] == '0' || p[2] == '1')) {   /* 0b101: GNU (and C23) binary */
				t->val = (long)strtoull(p + 2, &end, 2); fend = end; }
			if (fend > end || *p == '.') {   /* floating: a '.', an exponent (1e5), or a hex float (0x1p3) goes past the integer */
				t->fval = fv; t->fp = 2; end = fend;
				for (;; end++) {   /* f / l (long double == double), and GNU i / j (imaginary), in either order */
					if (*end == 'f' || *end == 'F') t->fp = 1;
					else if (*end == 'i' || *end == 'I' || *end == 'j' || *end == 'J') t->imag = 1;
					else if (*end != 'l' && *end != 'L') break;
				}
				t->text = tok_text(p, (size_t)(end - p)); p = end;
				cur = cur->next = t; continue;
			}
			for (; *end == 'u' || *end == 'U' || *end == 'l' || *end == 'L' || *end == 'i' || *end == 'I' || *end == 'j' || *end == 'J'; end++)   /* UL, LL, …; GNU i / j */
				if (*end == 'i' || *end == 'I' || *end == 'j' || *end == 'J') t->imag = 1;
			t->text = tok_text(p, (size_t)(end - p)); p = end;
			cur = cur->next = t; continue;
		}
		if (isalpha((unsigned char)*p) || *p == '_' || *p == '$') {                      /* ident / keyword ($: GCC allows it) */
			const char *s = p; while (isalnum((unsigned char)*p) || *p == '_' || *p == '$') p++;
			Token *t = new_tok(TK_IDENT, line);
			t->text = tok_text(s, (size_t)(p - s));
			for (int i = 0; GNU_SPELLING[i][0][0]; i++) if (!strcmp(t->text, GNU_SPELLING[i][0])) { t->text = GNU_SPELLING[i][1]; break; }   /* one canonical keyword */
			for (int i = 0; KEYWORDS[i]; i++) if (!strcmp(t->text, KEYWORDS[i])) { t->kind = TK_KW; break; }
			cur = cur->next = t; continue;
		}
		{   /* digraphs (cpp keeps their spelling): <: :> <% %> are [ ] { } */
			static const char dg[][2][3] = { {"<:", "["}, {":>", "]"}, {"<%", "{"}, {"%>", "}"} };
			int d = -1; for (int i = 0; i < 4; i++) if (p[0] == dg[i][0][0] && p[1] == dg[i][0][1]) d = i;
			if (d >= 0) { Token *t = new_tok(TK_PUNCT, line); t->text = dg[d][1]; cur = cur->next = t; p += 2; continue; }
		}
		int matched = 0;                                                                 /* punctuator */
		for (int i = 0; PUNCT[i]; i++) { size_t l = strlen(PUNCT[i]);
			if (!strncmp(p, PUNCT[i], l)) { Token *t = new_tok(TK_PUNCT, line); t->text = PUNCT[i]; cur = cur->next = t; p += l; matched = 1; break; } }
		if (!matched) die("lex: unexpected character '%c' on line %d", *p, line);
	}
	cur->next = new_tok(TK_EOF, line);
	return head.next;
}
