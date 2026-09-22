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

static const char *KEYWORDS[] = {
	"int", "char", "void", "short", "long", "signed", "unsigned",          /* base integer types      */
	"struct", "union", "enum", "typedef",                                   /* aggregate + alias       */
	"const", "volatile", "restrict", "static", "extern", "register", "inline", "sizeof", "__attribute__",  /* qualifiers/storage/op */
	"typeof", "__typeof__",                                                  /* GNU typeof(expr|type)   */
	"__asm__", "__volatile__", "asm",                                       /* inline assembly         */
	"return", "if", "else", "while", "do", "for", "break", "continue", "switch", "case", "default", "goto",  /* control flow */
	NULL };
/* Longest punctuators first so a prefix (e.g. "<") never shadows a longer match (e.g. "<<" / "<="). */
static const char *PUNCT[] = { "<<=", ">>=",                                        /* 3-char first    */
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

		if (*p == '\'') {                                                                /* char literal 'x' / '\n' */
			long v; p++;
			if (*p == '\\') { p++;
				switch (*p) { case 'n': v='\n'; break; case 't': v='\t'; break; case 'r': v='\r'; break;
				              case '0': v=0; break; case '\\': v='\\'; break; case '\'': v='\''; break;
				              case '"': v='"'; break; default: v=(unsigned char)*p; } p++;
			} else { v = (unsigned char)*p; p++; }
			if (*p != '\'') die("lex: unterminated char literal on line %d", line);
			p++;
			Token *t = new_tok(TK_NUM, line); t->val = v; cur = cur->next = t; continue;
		}
		if (*p == '"') {                                                                 /* string literal */
			const char *s = ++p;                                                         /* skip opening quote */
			while (*p && *p != '"') { if (*p == '\\' && p[1]) p += 2; else p++; }         /* keep escapes intact */
			Token *t = new_tok(TK_STR, line);
			size_t n = p - s; if (n >= sizeof t->text) n = sizeof t->text - 1;
			memcpy(t->text, s, n); t->text[n] = 0;                                       /* raw inner text (as spelled) */
			if (*p == '"') p++;                                                          /* skip closing quote */
			cur = cur->next = t; continue;
		}
		if (isdigit((unsigned char)*p)) {                                                /* integer literal */
			Token *t = new_tok(TK_NUM, line);
			char *end; t->val = strtol(p, &end, 0);                                      /* 0x.. / 0.. / dec */
			while (*end == 'u' || *end == 'U' || *end == 'l' || *end == 'L') end++;      /* skip int suffixes (UL, LL, …) */
			size_t n = end - p; if (n >= sizeof t->text) n = sizeof t->text - 1;
			memcpy(t->text, p, n); t->text[n] = 0; p = end;
			cur = cur->next = t; continue;
		}
		if (isalpha((unsigned char)*p) || *p == '_') {                                   /* ident / keyword */
			const char *s = p; while (isalnum((unsigned char)*p) || *p == '_') p++;
			Token *t = new_tok(TK_IDENT, line);
			size_t n = p - s; if (n >= sizeof t->text) n = sizeof t->text - 1;
			memcpy(t->text, s, n); t->text[n] = 0;
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
