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
#include <string.h>
#define IDMAX 256   /* an identifier's longest spelling + 1 in the name buffers (the kernel's reach ~120) */

/* ---- tokens (lex.c) ------------------------------------------------------------------------------ */
typedef enum { TK_NUM, TK_IDENT, TK_STR, TK_PUNCT, TK_KW, TK_EOF } TokKind;
typedef struct Token {
	TokKind kind;
	struct Token *next;
	long val;              /* TK_NUM: the integer value                         */
	double fval; int fp;   /* TK_NUM floating literal: its value; fp = 1 float (f suffix), 2 double (incl. l = long double) */
	const char *text;      /* the raw lexeme (ident/keyword name, punctuator, number, string), whole — never truncated */
	char *sval;            /* TK_STR: the full (untruncated) raw string contents — asm templates/format strings exceed text[64] */
	int line;              /* source line, for diagnostics                       */
	int wide;              /* TK_STR / char TK_NUM: element width of an L"" / U"" (4) or u"" (2) literal; 0 = plain */
	int imag;              /* TK_NUM: an imaginary constant (GNU `2.0i`, `3if`, `1i`) */
	unsigned char pack;    /* #pragma pack in force here: the maximum member alignment (0: none) */
	unsigned char vis;     /* #pragma GCC visibility push in force here: a VIS_* (0: none) */
} Token;

Token *lex(const char *src);                 /* tokenize the whole source into a linked list */
/* #pragma weak / redefine_extname and #ident lines the lexer met (applied by the parser / emitted by gen). */
typedef struct PragmaName { char *name, *target; struct PragmaName *next; } PragmaName;
extern PragmaName *pragma_weaks, *pragma_redefines, *idents;   /* weak: target NULL or `= target`; ident: name = its string */
int   wstr_decode(const char *raw, unsigned *out, int cap);   /* wide literal text -> code points (escapes + UTF-8) */

/* ---- types (type.c) ------------------------------------------------------------------------------ */
typedef enum { TY_INT, TY_CHAR, TY_SHORT, TY_LLONG, TY_FLOAT, TY_DOUBLE, TY_PTR, TY_ARRAY, TY_STRUCT, TY_VECTOR, TY_COMPLEX } TypeKind;   /* arithmetic kinds precede TY_PTR */
/* A struct member. Ordinary members use `offset` (bytes). A BITFIELD (`is_bitfield`) instead occupies
 * `bit_width` bits at `bit_offset` bits into the storage unit that starts at byte `offset`. `bf_dense`: it was
 * laid out BIT BY BIT (packed / #pragma pack), so that declared unit may reach past the end of the object and
 * need not be aligned — codegen must then touch only the bytes the field itself occupies. */
typedef struct Member { char name[IDMAX]; struct Type *type; int offset; int is_bitfield; int bit_offset; int bit_width; int bf_dense; int is_anon; int align; int packed; int ealign; int promoted; int voff, vdepth; struct Member *next; } Member;   /* align: aligned(N) on the member; packed: packed on the member (alignment 1); ealign: the alignment the layout gave it; promoted: a lookup alias of an anonymous member's member (no storage of its own); voff: after a variable-length member, the frame slot holding its offset (0: `offset` is it), in function vdepth's frame */
/* size drives load/store WIDTH (1/2/4/8 -> b/h/word/pair); is_unsigned drives sign-extension + narrowing.
 * align, when >0, is a forced byte alignment (from __attribute__((packed))=1 / ((aligned(N)))=N on a struct). */
/* fn_ret: non-NULL only for a type made by a FUNCTION typedef (`typedef int fn_t(args);`) — naming a declaration with
 * it declares a function returning fn_ret (not a variable); as a parameter it adjusts to a function pointer. */
/* is_bool: _Bool (1 byte, unsigned) — every conversion INTO it yields 0/1 (x != 0), not a truncation. */
/* vsize_off: a VARIABLY-modified array (VLA): the frame slot holding its byte size, computed where its declarator
 * was (size is then 0 and unused); 0 = a normal fixed-size type. vsize_depth: the function (nesting depth) whose
 * frame holds that slot. */
/* A function type (fn_ret set) also carries its prototype: params[nparams], variadic (1 = `...`, 2 = unknown). */
/* TY_VECTOR (GCC vector_size): `len` lanes of the integer/floating element `base`, `size` bytes; held and copied by
 * value like a struct (codegen: a vector-valued expression yields its address). TY_COMPLEX (_Complex, and GCC's
 * integer complex types): the real then the imaginary part, both of type `base`, held the same way. */
typedef struct Type { TypeKind kind; struct Type *base; int size; int len; Member *members; int is_unsigned; int align; struct Type *fn_ret; int is_bool; int vsize_off, vsize_depth;
                      struct Type **params; int nparams; int variadic;
                      int quals;      /* 1 = const, 2 = volatile (only __builtin_types_compatible_p looks) */
                      int tag;        /* a distinct type of an otherwise-equal representation: each enum, long double (-1) */
                      int prec;       /* a wide bit-field's type (GCC: `unsigned long long b:40` is a 40-bit type): its
                                       * value bits (0 = all); arithmetic on it wraps at that width */
                      int sso;        /* a struct with scalar_storage_order("big-endian"): its scalars are stored
                                       * byte-reversed (bit-fields in big-endian bit order) */
                      int opaque;     /* a vector comparison's result: converts implicitly to any vector of its size */
                      int transparent; } Type;   /* a transparent_union: a parameter of it takes any member's type, passed as
                                                  * its first member */
extern Type *ty_int, *ty_char;               /* signed int (4) + plain char (1, unsigned on ARM) */
extern Type *ty_uint, *ty_schar, *ty_short, *ty_ushort;   /* the remaining 32/16/8-bit scalar singletons */
extern Type *ty_llong, *ty_ullong;           /* long long / unsigned long long (8 bytes, register pair) */
extern Type *ty_bool;                        /* _Bool */
extern Type *ty_float, *ty_double, *ty_ldouble;   /* IEEE single / double; long double has double's representation (ARM EABI) */
int   is_fp(Type *t);
Type *usual_arith(Type *a, Type *b);         /* usual-arithmetic-conversion result type (drives op width+sign) */
Type *func_ret_type(const char *name);       /* a called function's declared return type (NULL if unknown) */
int   func_declared(const char *name);        /* 1 if `name` has a recorded function signature (=> direct `bl`, not indirect) */
Type *func_param_type(const char *name, int i);   /* a callee's declared param i type (NULL if unknown/vararg) */
int   func_base_pcs(const char *name);      /* calls use the base PCS: declared with `...`, or pcs("aapcs") */
int   func_returns_twice(const char *name); /* setjmp/vfork-like: the call can return again later (a longjmp, the vfork parent) */
/* AAPCS argument placement, shared by caller + callee. Arguments occupy consecutive "argument words": words
 * 0..3 are r0..r3, word 4+ is outgoing stack word (w-4). pos[i] = arg i's first word; returns the total. */
/* vfp = the AAPCS-VFP variant (hard float, non-variadic callee): float/double/HFA args go to s0-s15 instead,
 * vreg[i] = their first s-register (-1 = core/stack; a VFP arg that overflowed to the stack has a pos). */
int  aapcs_layout(Type **ty, int n, int first, int vfp, int *pos, int *vreg);
typedef struct { int ncrn, nsaa, vclosed, vfp; unsigned sused; } Aapcs;   /* the placement so far (NCRN, NSAA, the used s-regs) */
static inline Aapcs aapcs_start(int first, int vfp) { return (Aapcs){ .ncrn = first, .vfp = vfp }; }
void aapcs_place(Aapcs *a, Type *t, int *pos, int *vreg);
int  aapcs_words(const Aapcs *a);
int  vfp_class(Type *t, int *nel);           /* 0; 1 = float elements; 2 = double; 3/4 = 64/128-bit vectors (a scalar/vector/HFA/HVA) */
extern int exec_stack;                       /* the code needs an executable stack (a nested function's trampoline) */
extern int gnu89_inline;                     /* -fgnu89-inline: gnu89 `inline` semantics (see cc.c) */
extern int soft_float;                       /* -mfloat-abi=soft: no floating code, base PCS attributes */
extern int general_regs_only;                /* -mgeneral-regs-only: no FP registers, but the hard-float ABI stays */
extern int short_wchar, no_unaligned, unsafe_math, finite_math, softfp_abi, unwind_tables, function_sections, data_sections;   /* cc.c: target options (build attributes) */
extern unsigned fixed_regs;                  /* cc.c: -ffixed-REG (r4-r10) */
int fixed_reg_number(const char *s);         /* gen.c: a register name -> its number, else -1 */
extern const char *opt_march, *opt_mfpu; extern char opt_olevel;
int  arg_words(Type *t);                     /* words an argument of type t occupies (struct = ceil(size/4)) */
int  is_sret(Type *t, int vfp);              /* returned via a caller-supplied buffer (struct > 4 bytes, not an HFA under VFP; vector > 16) */
Type *pointer_to(Type *base);                /* a fresh `base *` type */
Type *array_of(Type *base, int len);         /* a fresh `base [len]` type (size = len*base->size) */
int   is_ptr(Type *t);
int   is_ptr_like(Type *t);                  /* pointer OR array (both index/decay the same way) */
int   align_of(Type *t);                     /* byte alignment (int/ptr=4, char=1, array=elem, struct=max) */
Type *vector_of(Type *elem, long size);      /* GCC vector_size(size) of elem (an invalid one is an error) */
int   is_vec(Type *t);
int   is_aggr(Type *t);                      /* held by value in memory, its value its address: a struct/union or a vector */
int   vec_convertible(Type *to, Type *from); /* an implicit vector conversion (assignment, argument, return) is valid */
Type *complex_of(Type *elem);                /* _Complex elem */
int   is_cplx(Type *t);
int   same_cplx(Type *a, Type *b);           /* two complex types of the same element type */
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
	ND_COND, ND_CAST, ND_COMMA, ND_STMTEXPR, ND_REGVAR,          /* c?a:b ; (type)expr ; (a,b) ; ({...}) ; global reg var */
	ND_VA_START, ND_VA_ARG,                                      /* __builtin_va_start / __builtin_va_arg */
	ND_RMW, ND_CUR,                                              /* lhs op= rhs / ++ / -- (lvalue evaluated ONCE) ; its old value */
	ND_VLAMARK,                                                  /* a VLA declaration's stack mark (re-execution frees its last instance) */
	ND_CPAIR,                                                    /* a complex value from its parts: lhs real, rhs imaginary */
	ND_RETURN, ND_IF, ND_WHILE, ND_DOWHILE, ND_FOR, ND_BREAK, ND_CONTINUE,  /* statements             */
	ND_SWITCH, ND_CASE, ND_GOTO, ND_LABEL, ND_LABELADDR, ND_ASM, ND_BLOCK, ND_EXPRSTMT /* +goto/label, &&label, inline asm, block */
} NodeKind;

typedef struct Node {
	NodeKind kind;
	Type *type;                  /* result type (filled by add_type); drives ptr scaling + load width */
	struct Node *lhs, *rhs;      /* binary/unary operands                                        */
	long val;                    /* ND_NUM ; ND_CASE low value (an FP ND_NUM: its IEEE bit pattern) */
	double fval;                 /* FP ND_NUM: the value (constant folding)                      */
	long val2; int is_range;     /* ND_CASE `low ... high` range (GCC case ranges)               */
	int is_prop;                 /* ND_NUM: a PROPAGATED constant (a read-only local, an inlined call's value): folds like
	                              * one, but isn't an integer constant expression (`char a[z]` stays a VLA)          */
	int decl_init;               /* ND_ASSIGN: a declaration's initializer (of a local declared right there) */
	int rng_ok; long long rlo, rhi; unsigned long long rzeros;   /* ND_VAR: the range its value is known to lie in there
	                              * [rlo, rhi], and bits known to be 0 (the parser's value ranges)                   */
	char name[IDMAX];            /* ND_VAR / ND_CALL name                                        */
	char reg[8];                 /* ND_VAR pinned to a hard register (`register x __asm__("r7")`)*/
	char cons[32];               /* ND_ASM operand: its constraint ("r"/"=r"/"+r"/"i"/…)         */
	char *asm_tmpl;              /* ND_ASM: the (possibly multi-line, %N-bearing) template string */
	int asm_basic;               /* ND_ASM: basic asm (no `:` sections) — `%` is literal, not an operand ref */
	int asm_clobber;             /* ND_ASM: register mask from the clobber list (operands avoid them; r4-r10 saved) */
	int offset;                  /* ND_VAR: byte offset from fp (negative = local slot); struct ND_CALL: its result temp */
	int bit_width, bit_offset;   /* ND_MEMBER on a bitfield: field width + bit offset in its unit (width 0 = not a bitfield) */
	int bf_dense;                /* ND_MEMBER on a bitfield: packed, laid out bit by bit (see Member.bf_dense) */
	int mem_align;               /* ND_MEMBER: the alignment the LAYOUT gave it, when that is narrower than the
	                              * type's own (a packed member) — 0 means natural. -mno-unaligned-access splits
	                              * an access this narrow into bytes; by default, as gcc, we access it wide. */
	struct Type *bf_type;        /* ND_MEMBER on a bitfield: its DECLARED type (unit width + extraction sign); n->type is
	                              * the promoted type (C11 6.3.1.1p2: a <= int-sized field whose values fit in int is int) */
	struct Node *cond, *then, *els;  /* ND_IF / ND_WHILE / ND_FOR (cond + then/body, els for if)  */
	struct Node *init, *inc;         /* ND_FOR: initializer statement + per-iteration step expr    */
	struct Node *case_list, *case_next;  /* ND_SWITCH: its cases; ND_CASE: link in that list        */
	int is_default;                  /* ND_CASE: this is `default:`                                 */
	struct Node *body;           /* ND_BLOCK: statement list (chained via ->next); val = LABELED: a label and its statement */
	struct Node *args;           /* ND_CALL: argument list (chained via ->next)                  */
	struct Node *next;           /* next statement / next argument in a list                     */
	int is_post;                 /* ND_RMW: x++ / x-- — the result is the OLD value               */
	int vla_obj;                 /* ND_VAR: a VLA object — its slot holds the (alloca'd) address, not the array */
	struct Node *target;         /* ND_CUR: the ND_RMW whose old value it reads (a back-link, not a child) */
	int tls;                     /* ND_GVAR: a thread-local object (its address comes from the thread pointer) */
	int sso;                     /* ND_MEMBER / ND_DEREF: a scalar (or array element) of a scalar_storage_order("big-endian")
	                              * struct — byte-reversed in memory */
	int tls_local;               /* ...defined in this translation unit (a non-PIC access can use local-exec) */
	int chain;                   /* nested functions: ND_VAR of an enclosing function's local — the static-chain hops to
	                              * its frame; ND_CALL of a nested function — 1 + the hops to the frame it expects as its
	                              * static chain (0: none); ND_GOTO / ND_LABELADDR to an enclosing function's label — hops */
	char *owner;                 /* ND_GOTO / ND_LABELADDR: the enclosing function the label is in (NULL: this one) */
	int unproto;                 /* ND_CALL: no prototype in scope at the call (the arguments go by their own promoted types,
	                              * whatever a later definition says — as GCC does) */
	char *imm_sym;               /* ND_ASM immediate operand ("i"/"s") that is an address: its `sym+off` (else the value is val) */
	int twice;                   /* gen: the subtree makes a returns-twice call (setjmp, vfork) */
	int imm_bad;                 /* ND_ASM immediate operand that isn't a constant: an error if the asm is emitted (a
	                              * dead `if (__builtin_constant_p(x)) asm(... "i"(x))` branch is dropped first) */
} Node;

/* Declaration attributes we honor (GCC __attribute__((...))): weak -> .weak binding; used -> never dropped by
 * DCE; align -> aligned(N) (0 = natural); section -> placed in that section; alias -> `.set name, alias`. */
typedef struct Attr { int weak, used, align, pcs, vis, packed, always_inline, gnu_inline, common, naked, returns_twice;
	int isr;          /* interrupt("IRQ"/"FIQ"/"ABORT"/"SWI"/"UNDEF"): an exception handler. 1 = return via
	                   * `lr - 4` (IRQ/FIQ/ABORT, where lr points one instruction too far), 2 = via lr itself
	                   * (SWI/UNDEF). Either way the handler must save every register it touches and return
	                   * with `ldmfd sp!, {..., pc}^`, which restores CPSR from SPSR. */
	int ctor, dtor;   /* constructor/destructor: 0 none, -1 no priority, else the priority (its .init_array.NNNNN) */
	char section[IDMAX], alias[IDMAX], cleanup[IDMAX]; } Attr;   /* pcs: 1 = pcs("aapcs") (base PCS even under hard float), 2 = pcs("aapcs-vfp");
                                                                                         * packed: on a member (its alignment 1) or an enum (its smallest type);
                                                                                         * cleanup: a local's scope-exit function (cleanup(fn));
                                                                                         * returns_twice: setjmp-like (see func_returns_twice);
                                                                                         * vis: 0 unset, else VIS_* */
enum { VIS_DEFAULT = 1, VIS_HIDDEN, VIS_INTERNAL, VIS_PROTECTED };
extern int default_vis;                      /* -fvisibility=: a definition's visibility when it names none (0 = default) */
const char *vis_directive(int vis);          /* ".hidden" / ".internal" / ".protected", or NULL for default/unset */
int vis_local(int vis);                      /* 1 if the visibility hides the symbol from other modules (hidden/internal) */
typedef struct ProtoVis { char name[IDMAX]; int vis; struct ProtoVis *next; } ProtoVis;
extern ProtoVis *proto_vis;                   /* a prototype's visibility: emitted by gen() only if the symbol is used */

/* A compiled function: name, its parameter count, the total stack frame it needs, and its body list. */
typedef struct Func {
	char name[IDMAX];
	Type *ret_type;              /* declared return type (so `return e` widens to 64-bit when needed) */
	int nparams;                 /* number of NAMED params (excludes the variadic `...`)         */
	/* Incoming argument words (aapcs_layout; includes the hidden sret pointer). A function with any homes
	 * r0..r3 above its frame record, so every param sits at [r11, #8 + 4*word] and va_start = 8 + 4*nfixed_words. */
	int nfixed_words;
	int variadic;                /* 1 if declared with `...` (needs the register-save prologue)  */
	int is_static;               /* 1 if `static` — file-local symbol, emit no .global            */
	int reachable;               /* DCE: 0 = unreachable (drop), 1 = reachable/queued, 2 = walked  */
	int frame;                   /* bytes of stack for locals+params (8-aligned)                 */
	int vfp;                     /* AAPCS-VFP (hard float, not variadic): FP params/results in VFP registers */
	int vfp_save;                /* a param arrives in s0-s15: d0-d7 are saved above the homed r0-r3 */
	int gnu89_inline;            /* gnu89 `inline` semantics apply (-fgnu89-inline or __attribute__((gnu_inline))) */
	int nested;                  /* a GNU nested function: its static chain (the enclosing frame) arrives in ip and is kept
	                              * in its first local slot, [r11, #-4] */
	int final_frame, alloca_slot;   /* set by codegen (-1 before): the frame size and alloca-floor slot (a non-local goto into it) */
	int no_emit;                 /* an inline-only definition (GNU `extern inline` + gnu_inline): calls go to the external one */
	Attr attr;
	Node *body;                  /* statement list                                               */
	struct Func *next;
} Func;

void  add_type(Node *n);         /* recursively annotate a subtree with result types (type.c) */
void  type_node(Node *n);        /* ...just n, from its (already typed) operands */

/* A global's initializer, as a flat list of emitted items (constant word/byte, a symbol's address, or
 * a run of zero bytes for padding / uninitialized tail). NULL init => the whole object goes in .bss. */
enum { INIT_CONST, INIT_SYM, INIT_ZERO };
#define SYMEXPR_MAX (2 * IDMAX + 16)   /* INIT_SYM: a symbol, or a difference `a - b` (label/anchor offsets in static data) */
typedef struct Init { int kind; long val; char sym[SYMEXPR_MAX]; int size; struct Init *next; } Init;

/* File-scope objects: global variables and string literals, emitted by gen() as .data/.bss/.rodata. */
/* storage-class bits declspec() reports for file-scope objects (they set symbol binding). */
#define SC_EXTERN 1   /* `extern` — a reference; emit no definition */
#define SC_STATIC 2   /* `static` — file-local symbol (no .global)  */
#define SC_REGISTER 4 /* `register` — for a file-scope `register T x asm("rN")` global register variable */
#define SC_TLS 8      /* `_Thread_local` / `__thread` — one object per thread (.tdata/.tbss) */
#define SC_INLINE 16  /* `inline` (with extern + gnu_inline: an inline-only definition, never emitted) */
#define SC_AUTO 32    /* `auto`: a block-scope object's default; on a block-scope function, a nested one declared ahead */
typedef struct Gvar {
	char name[IDMAX];               /* symbol (a var name, or a .LSTR label for a string)           */
	Type *type;
	int is_str;                  /* 1 = string literal -> .rodata .asciz; 0 = variable            */
	int is_extern;               /* 1 = `extern` decl -> reference only, emit no storage           */
	int is_static;               /* 1 = `static` -> file-local symbol, emit no .global             */
	int is_topasm;               /* 1 = file-scope `asm("...")` -> emit `str` verbatim (e.g. .weak/.set aliases) */
	int is_tls;                  /* 1 = thread-local: .tdata/.tbss, addressed through the thread pointer */
	int const_obj;               /* declared const (a struct type keeps no qualified copy: said here) */
	int wide;                    /* is_str: element width of a wide literal (4 = L/U, 2 = u); 0 = char */
	Init *init;                  /* initializer item list -> .data; NULL -> .bss                 */
	Attr attr;
	char *str;                   /* is_str: the literal's bytes (slen of them; adjacent ones concatenated, each decoded on
	                              * its own); is_topasm: the text */
	int slen;
	unsigned *wstr;              /* a wide literal (wide = 2/4): its slen code units */
	int reachable;               /* dce(): emitted (a static object or literal only when something emitted refers to it) */
	struct Gvar *next;
} Gvar;
extern Gvar *globals;            /* built by parse(), consumed by gen() */
extern int warn_implicit_fn;     /* an undeclared call: 0 silent, 1 warn (default, as gcc 13), 2 error (-Werror=) */
void warn(const char *fmt, ...);
extern int pic;                  /* -fPIC: global/string access goes through the GOT (position-independent) */
extern int word_relocs;          /* -mword-relocations: addresses as literal words (U-Boot relocates only those) */

Func *parse(Token *tok);         /* tokens -> a list of functions (+ fills `globals`) */

/* ---- codegen (gen.c) ----------------------------------------------------------------------------- */
#define CLABEL_FMT ".Ll_%s.%s"   /* asm symbol of C label (function, label): goto, &&label code and static data share it
                                  * ('.' can't be in either name: `f_a`+`b` and `f`+`a_b` were one symbol) */
void gen(Func *prog, const char *out);   /* emit ARM assembly text for the whole program */

/* ---- shared (cc.c) ------------------------------------------------------------------------------- */
__attribute__((noreturn, format(printf, 1, 2))) void die(const char *fmt, ...);          /* "cc: ..." + exit(1) */
/* An identifier into a name buffer (char[IDMAX]): whole, or a loud error — a truncated name would silently alias
 * another (two long kernel __UNIQUE_ID names did). */
static inline void idcpy(char *dst, const char *src) {
	size_t n = strlen(src); if (n >= IDMAX) die("identifier too long (%zu characters, limit %d): %.60s...", n, IDMAX - 1, src);
	memcpy(dst, src, n + 1);
}
extern const char *typing_func;          /* type.c: the function add_type is typing, for its errors */
int parse_cur_line(void);
int null_ptr_const(Node *e);             /* parse.c: an integer constant 0, or one cast to void * */                /* parse.c: the line the parser is at (0 after parsing) */
int  name_is_weak(const char *name);     /* declared weak on a prototype (parse.c) */
const char *asm_name(const char *name);  /* a C name's assembler symbol: its asm label (`T x __asm__("sym")`), else itself */
int  is_alias_target(const char *name);  /* named by an alias("name") attribute (so kept, even static and otherwise unused) */

#endif
