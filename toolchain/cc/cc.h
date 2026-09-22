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
typedef enum { TK_NUM, TK_IDENT, TK_STR, TK_PUNCT, TK_KW, TK_EOF } TokKind;
typedef struct Token {
	TokKind kind;
	struct Token *next;
	long val;              /* TK_NUM: the integer value                         */
	char text[64];         /* the raw lexeme (ident/keyword name, or punctuator) */
	int line;              /* source line, for diagnostics                       */
} Token;

Token *lex(const char *src);                 /* tokenize the whole source into a linked list */

/* ---- types (type.c) ------------------------------------------------------------------------------ */
typedef enum { TY_INT, TY_CHAR, TY_SHORT, TY_LLONG, TY_PTR, TY_ARRAY, TY_STRUCT } TypeKind;
/* A struct member. Ordinary members use `offset` (bytes). A BITFIELD (`is_bitfield`) instead occupies
 * `bit_width` bits at `bit_offset` bits into the storage unit that starts at byte `offset`. */
typedef struct Member { char name[64]; struct Type *type; int offset; int is_bitfield; int bit_offset; int bit_width; int is_anon; struct Member *next; } Member;
/* size drives load/store WIDTH (1/2/4/8 -> b/h/word/pair); is_unsigned drives sign-extension + narrowing.
 * align, when >0, is a forced byte alignment (from __attribute__((packed))=1 / ((aligned(N)))=N on a struct). */
typedef struct Type { TypeKind kind; struct Type *base; int size; int len; Member *members; int is_unsigned; int align; } Type;
extern Type *ty_int, *ty_char;               /* signed int (4) + plain char (1, unsigned on ARM) */
extern Type *ty_uint, *ty_schar, *ty_short, *ty_ushort;   /* the remaining 32/16/8-bit scalar singletons */
extern Type *ty_llong, *ty_ullong;           /* long long / unsigned long long (8 bytes, register pair) */
Type *usual_arith(Type *a, Type *b);         /* usual-arithmetic-conversion result type (drives op width+sign) */
Type *func_ret_type(const char *name);       /* a called function's declared return type (NULL if unknown) */
Type *func_param_type(const char *name, int i);   /* a callee's declared param i type (NULL if unknown/vararg) */
int  aapcs_layout(const int *is64, int n, int *onstk, int *word);   /* AAPCS arg placement (caller + callee agree) */
Type *pointer_to(Type *base);                /* a fresh `base *` type */
Type *array_of(Type *base, int len);         /* a fresh `base [len]` type (size = len*base->size) */
int   is_ptr(Type *t);
int   is_ptr_like(Type *t);                  /* pointer OR array (both index/decay the same way) */
int   align_of(Type *t);                     /* byte alignment (int/ptr=4, char=1, array=elem, struct=max) */
/* add_type is declared after the Node typedef below */

/* ---- AST (parse.c) ------------------------------------------------------------------------------- */
typedef enum {
	ND_NUM, ND_VAR, ND_GVAR, ND_ASSIGN, ND_CALL,                 /* leaves (local/global) + assign + call */
	ND_ADDR, ND_DEREF, ND_MEMBER,                                /* &, *, and struct member access (. / ->) */
	ND_ADD, ND_SUB, ND_MUL, ND_DIV, ND_MOD,                      /* arithmetic                        */
	ND_EQ, ND_NE, ND_LT, ND_LE, ND_GT, ND_GE,                    /* comparisons (result 0/1)          */
	ND_AND, ND_OR,                                               /* && || (short-circuit)             */
	ND_BITAND, ND_BITOR, ND_BITXOR, ND_SHL, ND_SHR,              /* bitwise + shifts                  */
	ND_NEG, ND_NOT, ND_BITNOT,                                   /* unary - ! ~                       */
	ND_COND, ND_CAST, ND_COMMA, ND_STMTEXPR,                     /* c?a:b ; (type)expr ; (a,b) ; ({...}) */
	ND_VA_START, ND_VA_ARG,                                      /* __builtin_va_start / __builtin_va_arg */
	ND_RETURN, ND_IF, ND_WHILE, ND_DOWHILE, ND_FOR, ND_BREAK, ND_CONTINUE,  /* statements             */
	ND_SWITCH, ND_CASE, ND_GOTO, ND_LABEL, ND_ASM, ND_BLOCK, ND_EXPRSTMT /* +goto/label, inline asm, block */
} NodeKind;

typedef struct Node {
	NodeKind kind;
	Type *type;                  /* result type (filled by add_type); drives ptr scaling + load width */
	struct Node *lhs, *rhs;      /* binary/unary operands                                        */
	long val;                    /* ND_NUM ; ND_CASE low value                                   */
	long val2; int is_range;     /* ND_CASE `low ... high` range (GCC case ranges)               */
	char name[64];               /* ND_VAR / ND_CALL name                                        */
	char reg[8];                 /* ND_VAR pinned to a hard register (`register x __asm__("r7")`)*/
	char cons[8];                /* ND_ASM operand: its constraint ("r"/"=r"/"+r"/"i"/…)         */
	char *asm_tmpl;              /* ND_ASM: the (possibly multi-line, %N-bearing) template string */
	int offset;                  /* ND_VAR: byte offset from fp (negative = local slot)          */
	int bit_width, bit_offset;   /* ND_MEMBER on a bitfield: field width + bit offset in its unit (width 0 = not a bitfield) */
	struct Node *cond, *then, *els;  /* ND_IF / ND_WHILE / ND_FOR (cond + then/body, els for if)  */
	struct Node *init, *inc;         /* ND_FOR: initializer statement + per-iteration step expr    */
	struct Node *case_list, *case_next;  /* ND_SWITCH: its cases; ND_CASE: link in that list        */
	int is_default;                  /* ND_CASE: this is `default:`                                 */
	struct Node *body;           /* ND_BLOCK: statement list (chained via ->next)                */
	struct Node *args;           /* ND_CALL: argument list (chained via ->next)                  */
	struct Node *next;           /* next statement / next argument in a list                     */
} Node;

/* A compiled function: name, its parameter count, the total stack frame it needs, and its body list. */
typedef struct Func {
	char name[64];
	Type *ret_type;              /* declared return type (so `return e` widens to 64-bit when needed) */
	int nparams;                 /* number of NAMED params (excludes the variadic `...`)         */
	/* 64-bit args occupy TWO consecutive argument WORDS; the first 4 words arrive in r0..r3, the rest on
	 * the stack. arg_regs = how many of r0..r3 hold incoming words; arg_off[w] = the frame byte offset the
	 * prologue spills register-word w to (0 = skip); nfixed_words = word count of the fixed params (va_start). */
	int nfixed_words;
	int arg_regs;
	int arg_off[4];
	int variadic;                /* 1 if declared with `...` (needs the register-save prologue)  */
	int is_static;               /* 1 if `static` — file-local symbol, emit no .global            */
	int frame;                   /* bytes of stack for locals+params (8-aligned)                 */
	Node *body;                  /* statement list                                               */
	struct Func *next;
} Func;

void  add_type(Node *n);         /* recursively annotate a subtree with result types (type.c) */

/* A global's initializer, as a flat list of emitted items (constant word/byte, a symbol's address, or
 * a run of zero bytes for padding / uninitialized tail). NULL init => the whole object goes in .bss. */
enum { INIT_CONST, INIT_SYM, INIT_ZERO };
typedef struct Init { int kind; long val; char sym[64]; int size; struct Init *next; } Init;

/* File-scope objects: global variables and string literals, emitted by gen() as .data/.bss/.rodata. */
/* storage-class bits declspec() reports for file-scope objects (they set symbol binding). */
#define SC_EXTERN 1   /* `extern` — a reference; emit no definition */
#define SC_STATIC 2   /* `static` — file-local symbol (no .global)  */
typedef struct Gvar {
	char name[64];               /* symbol (a var name, or a .LSTR label for a string)           */
	Type *type;
	int is_str;                  /* 1 = string literal -> .rodata .asciz; 0 = variable            */
	int is_extern;               /* 1 = `extern` decl -> reference only, emit no storage           */
	int is_static;               /* 1 = `static` -> file-local symbol, emit no .global             */
	Init *init;                  /* initializer item list -> .data; NULL -> .bss                 */
	char str[64];                /* is_str: the raw string bytes (escapes as spelled)             */
	struct Gvar *next;
} Gvar;
extern Gvar *globals;            /* built by parse(), consumed by gen() */
extern int pic;                  /* -fPIC: global/string access goes through the GOT (position-independent) */

Func *parse(Token *tok);         /* tokens -> a list of functions (+ fills `globals`) */

/* ---- codegen (gen.c) ----------------------------------------------------------------------------- */
void gen(Func *prog, const char *out);   /* emit ARM assembly text for the whole program */

/* ---- shared (cc.c) ------------------------------------------------------------------------------- */
void die(const char *fmt, ...);          /* "cc: ..." + exit(1) */

#endif
