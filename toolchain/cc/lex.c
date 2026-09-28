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
	{ "__thread", "_Thread_local" }, { "thread_local", "_Thread_local" },
	{ "__complex__", "_Complex" }, { "__complex", "_Complex" }, { "__real", "__real__" }, { "__imag", "__imag__" }, { "", "" } };
static const char *KEYWORDS[] = {
	"int", "char", "void", "short", "long", "signed", "unsigned", "float", "double",   /* base arithmetic types */
	"struct", "union", "enum", "typedef",                                   /* aggregate + alias       */
	"const", "volatile", "restrict", "static", "extern", "register", "inline", "sizeof", "__attribute__",  /* qualifiers/storage/op */
	"_Thread_local",                                                        /* C11 thread storage duration (GNU __thread) */
	"__extension__", "_Bool", "_Generic",                                   /* GNU no-op prefix; C99 bool; C11 _Generic */
	"_Complex", "__real__", "__imag__",                                     /* C99 complex; GNU part operators */
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

static Token *new_tok(TokKind kind, int line) {
	Token *t = calloc(1, sizeof *t); t->kind = kind; t->line = line; return t;
}

Token *lex(const char *src) {
	Token head = {0}, *cur = &head;
	int line = 1;
	for (const char *p = src; *p; ) {
		if (*p == '\n') { line++; p++; continue; }
		if (isspace((unsigned char)*p)) { p++; continue; }
		if (*p == '#') { while (*p && *p != '\n') p++; continue; }                        /* cpp line-marker/directive (we consume gcc -E output) */
		if (p[0] == '/' && p[1] == '/') { while (*p && *p != '\n') p++; continue; }      /* line comment  */
		if (p[0] == '/' && p[1] == '*') { p += 2; while (*p && !(p[0]=='*'&&p[1]=='/')) { if(*p=='\n')line++; p++; } if(*p) p+=2; continue; }

		int wide = 0;                                                                    /* literal prefix: L/U (4-byte), u (2), u8 (plain) */
		if ((p[0] == 'L' || p[0] == 'U') && (p[1] == '\'' || p[1] == '"')) { wide = 4; p++; }
		else if (p[0] == 'u' && p[1] == '8' && p[2] == '"') p += 2;
		else if (p[0] == 'u' && (p[1] == '\'' || p[1] == '"')) { wide = 2; p++; }
		if (*p == '\'') {                                                                /* char literal 'x' / '\n' / '\001' / '\xff' */
			long v; p++;
			if (*p == '\\') { p++;
				switch (*p) {
				case 'n': v='\n'; p++; break; case 't': v='\t'; p++; break; case 'r': v='\r'; p++; break;
				case 'a': v='\a'; p++; break; case 'b': v='\b'; p++; break; case 'f': v='\f'; p++; break;
				case 'v': v='\v'; p++; break; case '\\': v='\\'; p++; break; case '\'': v='\''; p++; break;
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
			Token *t = new_tok(TK_NUM, line); t->val = v; cur = cur->next = t; continue;
		}
		if (*p == '"') {                                                                 /* string literal */
			const char *s = ++p;                                                         /* skip opening quote */
			while (*p && *p != '"') { if (*p == '\\' && p[1]) p += 2; else p++; }         /* keep escapes intact */
			Token *t = new_tok(TK_STR, line); t->wide = wide;
			size_t full = p - s;
			t->sval = malloc(full + 1); memcpy(t->sval, s, full); t->sval[full] = 0;     /* full raw text (as spelled) — unbounded */
			size_t n = full; if (n >= sizeof t->text) n = sizeof t->text - 1;
			memcpy(t->text, s, n); t->text[n] = 0;                                       /* truncated preview */
			if (*p == '"') p++;                                                          /* skip closing quote */
			cur = cur->next = t; continue;
		}
		if (isdigit((unsigned char)*p) || (*p == '.' && isdigit((unsigned char)p[1]))) {   /* number */
			Token *t = new_tok(TK_NUM, line);
			char *end, *fend; t->val = (long)strtoull(p, &end, 0);                     /* 0x.. / 0.. / dec; strtoull: literals > LONG_MAX (e.g. 64-bit hash primes) must wrap, not saturate */
			double fv = strtod(p, &fend);
			if (fend > end || *p == '.') {   /* floating: a '.', an exponent (1e5), or a hex float (0x1p3) goes past the integer */
				t->fval = fv; t->fp = 2; end = fend;
				for (;; end++) {   /* f / l (long double == double), and GNU i / j (imaginary), in either order */
					if (*end == 'f' || *end == 'F') t->fp = 1;
					else if (*end == 'i' || *end == 'I' || *end == 'j' || *end == 'J') t->imag = 1;
					else if (*end != 'l' && *end != 'L') break;
				}
				size_t n = end - p; if (n >= sizeof t->text) n = sizeof t->text - 1;
				memcpy(t->text, p, n); t->text[n] = 0; p = end;
				cur = cur->next = t; continue;
			}
			for (; *end == 'u' || *end == 'U' || *end == 'l' || *end == 'L' || *end == 'i' || *end == 'I' || *end == 'j' || *end == 'J'; end++)   /* UL, LL, …; GNU i / j */
				if (*end == 'i' || *end == 'I' || *end == 'j' || *end == 'J') t->imag = 1;
			size_t n = end - p; if (n >= sizeof t->text) n = sizeof t->text - 1;
			memcpy(t->text, p, n); t->text[n] = 0; p = end;
			cur = cur->next = t; continue;
		}
		if (isalpha((unsigned char)*p) || *p == '_') {                                   /* ident / keyword */
			const char *s = p; while (isalnum((unsigned char)*p) || *p == '_') p++;
			Token *t = new_tok(TK_IDENT, line);
			size_t n = p - s; if (n >= sizeof t->text) n = sizeof t->text - 1;
			memcpy(t->text, s, n); t->text[n] = 0;
			for (int i = 0; GNU_SPELLING[i][0][0]; i++) if (!strcmp(t->text, GNU_SPELLING[i][0])) { strcpy(t->text, GNU_SPELLING[i][1]); break; }   /* one canonical keyword */
			for (int i = 0; KEYWORDS[i]; i++) if (!strcmp(t->text, KEYWORDS[i])) { t->kind = TK_KW; break; }
			cur = cur->next = t; continue;
		}
		int matched = 0;                                                                 /* punctuator */
		for (int i = 0; PUNCT[i]; i++) { size_t l = strlen(PUNCT[i]);
			if (!strncmp(p, PUNCT[i], l)) { Token *t = new_tok(TK_PUNCT, line); memcpy(t->text, PUNCT[i], l); t->text[l]=0; cur = cur->next = t; p += l; matched = 1; break; } }
		if (!matched) die("lex: unexpected character '%c' on line %d", *p, line);
	}
	cur->next = new_tok(TK_EOF, line);
	return head.next;
}
