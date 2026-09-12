/*
 * cc.h — interface for our from-scratch C compiler, the last core tool of the toolchain (after as + ld).
 * Like a real cc it emits TEXT ASSEMBLY (C -> .s), so it rides on our own as + ld rather than writing ELF
 * itself. Classic three-stage split, one file per stage (mirrors gas/binutils layering in this project):
 *
 *   lex.c    lexer:  source text -> a token stream                                    (front end)
 *   parse.c  parser: tokens -> an AST (recursive descent, standard C precedence)      (front end)
 *   gen.c    codegen: AST -> ARMv7-A/A32 assembly text, AAPCS calling convention      (BACKEND = arch)
 *   cc.c     driver: read file, run the stages, write the .s
 *
 * M1 subset (grown test-driven): `int`/`void`; functions with up to 4 params + calls; local `int` vars;
 * integer arithmetic/bitwise/shift/comparison/logical operators; if/else, while, return, blocks.
 * Direct AST->ARM (a stack machine: every expression leaves its result in r0) — simple, unoptimized.
 */
#ifndef CC_H
#define CC_H

/* ---- tokens (lex.c) ------------------------------------------------------------------------------ */
typedef enum { TK_NUM, TK_IDENT, TK_PUNCT, TK_KW, TK_EOF } TokKind;
typedef struct Token {
	TokKind kind;
	struct Token *next;
	long val;              /* TK_NUM: the integer value                         */
	char text[64];         /* the raw lexeme (ident/keyword name, or punctuator) */
	int line;              /* source line, for diagnostics                       */
} Token;

Token *lex(const char *src);                 /* tokenize the whole source into a linked list */

/* ---- AST (parse.c) ------------------------------------------------------------------------------- */
typedef enum {
	ND_NUM, ND_VAR, ND_ASSIGN, ND_CALL,                          /* leaves + assignment + call        */
	ND_ADD, ND_SUB, ND_MUL, ND_DIV, ND_MOD,                      /* arithmetic                        */
	ND_EQ, ND_NE, ND_LT, ND_LE, ND_GT, ND_GE,                    /* comparisons (result 0/1)          */
	ND_AND, ND_OR,                                               /* && || (short-circuit)             */
	ND_BITAND, ND_BITOR, ND_BITXOR, ND_SHL, ND_SHR,              /* bitwise + shifts                  */
	ND_NEG, ND_NOT, ND_BITNOT,                                   /* unary - ! ~                       */
	ND_RETURN, ND_IF, ND_WHILE, ND_BLOCK, ND_EXPRSTMT            /* statements                        */
} NodeKind;

typedef struct Node {
	NodeKind kind;
	struct Node *lhs, *rhs;      /* binary/unary operands                                        */
	long val;                    /* ND_NUM                                                       */
	char name[64];               /* ND_VAR / ND_CALL name                                        */
	int offset;                  /* ND_VAR: byte offset from fp (negative = local slot)          */
	struct Node *cond, *then, *els;  /* ND_IF / ND_WHILE (cond+then, els for if)                 */
	struct Node *body;           /* ND_BLOCK: statement list (chained via ->next)                */
	struct Node *args;           /* ND_CALL: argument list (chained via ->next)                  */
	struct Node *next;           /* next statement / next argument in a list                     */
} Node;

/* A compiled function: name, its parameter count, the total stack frame it needs, and its body list. */
typedef struct Func {
	char name[64];
	int nparams;                 /* params occupy the first slots (spilled from r0..r3)          */
	int frame;                   /* bytes of stack for locals+params (8-aligned)                 */
	Node *body;                  /* statement list                                               */
	struct Func *next;
} Func;

Func *parse(Token *tok);         /* tokens -> a list of functions */

/* ---- codegen (gen.c) ----------------------------------------------------------------------------- */
void gen(Func *prog, const char *out);   /* emit ARM assembly text for the whole program */

/* ---- shared (cc.c) ------------------------------------------------------------------------------- */
void die(const char *fmt, ...);          /* "cc: ..." + exit(1) */

#endif
