/*
 * type.c — the (minimal) type system: char/short/int (signed + unsigned), pointer-to, array, struct. Two
 * jobs: provide the type singletons + constructors, and walk a parsed subtree bottom-up assigning each node
 * a result Type (add_type). Codegen needs those types for: the LOAD/STORE WIDTH of an lvalue (char=1,
 * short=2, int/pointer=4 -> ldrb/ldrh/ldr), the SIGN of a narrow load (is_unsigned -> ldr{b,h} vs
 * ldrs{b,h}), and — via the parser's new_add/new_sub — POINTER ARITHMETIC scaling. Plain `char` is unsigned
 * (ARM's default); scalars align to their own width. `long` is 32-bit (ILP32); `long long`/64-bit is TODO.
 */
#include <stdlib.h>
#include "cc.h"

static Type int_ty    = { TY_INT,   NULL, 4, 0, NULL, 0, 0 };
static Type uint_ty   = { TY_INT,   NULL, 4, 0, NULL, 1, 0 };
static Type char_ty   = { TY_CHAR,  NULL, 1, 0, NULL, 1, 0 };   /* plain char = unsigned (ARM default) */
static Type schar_ty  = { TY_CHAR,  NULL, 1, 0, NULL, 0, 0 };   /* signed char                        */
static Type short_ty  = { TY_SHORT, NULL, 2, 0, NULL, 0, 0 };
static Type ushort_ty = { TY_SHORT, NULL, 2, 0, NULL, 1, 0 };
static Type llong_ty  = { TY_LLONG, NULL, 8, 0, NULL, 0, 0 };   /* long long          (r0:r1 pair)    */
static Type ullong_ty = { TY_LLONG, NULL, 8, 0, NULL, 1, 0 };   /* unsigned long long                 */
Type *ty_int  = &int_ty;   Type *ty_uint   = &uint_ty;
Type *ty_char = &char_ty;  Type *ty_schar  = &schar_ty;
Type *ty_short = &short_ty; Type *ty_ushort = &ushort_ty;
Type *ty_llong = &llong_ty; Type *ty_ullong = &ullong_ty;

Type *pointer_to(Type *base) { Type *t = calloc(1, sizeof *t); t->kind = TY_PTR; t->base = base; t->size = 4; return t; }
Type *array_of(Type *base, int len) { Type *t = calloc(1, sizeof *t); t->kind = TY_ARRAY; t->base = base; t->len = len; t->size = base->size * len; return t; }
int   is_ptr(Type *t) { return t && t->kind == TY_PTR; }
int   is_ptr_like(Type *t) { return t && (t->kind == TY_PTR || t->kind == TY_ARRAY); }
int   align_of(Type *t) {
	if (t->kind == TY_ARRAY) return align_of(t->base);
	if (t->kind == TY_STRUCT) {
		if (t->align) return t->align;   /* forced by __attribute__((packed))=1 / ((aligned(N))) */
		int a = 1; for (Member *m = t->members; m; m = m->next) { int ma = align_of(m->type); if (ma > a) a = ma; } return a;
	}
	return t->size;   /* scalars align to their own width: char=1, short=2, int/pointer=4 */
}

/* Integer promotion: anything narrower than int (char/short, signed or not) becomes signed int — its
 * value always fits; int/long stays 32-bit, long long stays 64-bit; sign is preserved for the 32/64 types.
 * Pointers/arrays/structs pass through unchanged. */
static Type *promote(Type *t) {
	if (t->kind == TY_PTR || t->kind == TY_ARRAY || t->kind == TY_STRUCT) return t;
	if (t->size == 8) return t->is_unsigned ? ty_ullong : ty_llong;
	return (t->is_unsigned && t->size >= 4) ? ty_uint : ty_int;
}
/* Usual arithmetic conversions (the integer part): the higher-rank (wider) type wins; at equal rank an
 * unsigned operand makes the result unsigned. So `ll + uint` -> ll (64 bits hold every uint), `ull + ll`
 * -> ull, `uint + int` -> uint. */
Type *usual_arith(Type *a, Type *b) {
	Type *pa = promote(a), *pb = promote(b);
	int size = pa->size > pb->size ? pa->size : pb->size;
	if (size == 8) {
		int u = (pa->size == 8 && pa->is_unsigned) || (pb->size == 8 && pb->is_unsigned);
		return u ? ty_ullong : ty_llong;
	}
	return (pa == ty_uint || pb == ty_uint) ? ty_uint : ty_int;
}

/* Recursively annotate a subtree with result types (children first, then the node itself). Idempotent
 * enough to run over a whole function body; leaves set by the parser (ND_VAR/ND_NUM) are respected. */
void add_type(Node *n) {
	if (!n) return;
	add_type(n->lhs); add_type(n->rhs);
	add_type(n->cond); add_type(n->then); add_type(n->els);
	add_type(n->init); add_type(n->inc);
	for (Node *c = n->body; c; c = c->next) add_type(c);
	for (Node *a = n->args; a; a = a->next) add_type(a);
	if (n->type) return;                 /* leaf/cast/member set by the parser — keep it (children now typed) */

	switch (n->kind) {
	case ND_NUM:   /* type by magnitude (suffixes were stripped by the lexer): int -> uint -> long long */
		if (n->val < -2147483648LL || n->val > 4294967295LL) n->type = ty_llong;
		else if (n->val > 2147483647LL) n->type = ty_uint;
		else n->type = ty_int;
		return;
	case ND_ADD: case ND_SUB:
		if (n->lhs->type->kind == TY_ARRAY) n->type = pointer_to(n->lhs->type->base);   /* array decays to pointer */
		else if (is_ptr(n->lhs->type)) n->type = n->lhs->type;                          /* pointer stays pointer */
		else n->type = usual_arith(n->lhs->type, n->rhs->type);                         /* int: signedness folds */
		return;
	case ND_MUL: case ND_DIV: case ND_MOD:
	case ND_BITAND: case ND_BITOR: case ND_BITXOR:
		n->type = usual_arith(n->lhs->type, n->rhs->type); return;   /* unsigned if either operand is */
	case ND_SHL: case ND_SHR:
	case ND_NEG: case ND_BITNOT:
		n->type = promote(n->lhs->type); return;      /* shift/negate/complement keep the (promoted) operand type */
	case ND_CALL: {                                   /* result = the callee's declared return type */
		Type *rt = n->lhs ? NULL : func_ret_type(n->name);   /* indirect (fn-ptr) call: unknown -> int */
		n->type = rt ? rt : ty_int; return;
	}
	case ND_EQ: case ND_NE: case ND_LT: case ND_LE: case ND_GT: case ND_GE:
	case ND_AND: case ND_OR: case ND_NOT:
		n->type = ty_int; return;                     /* comparisons/logical yield a plain int */
	case ND_ASSIGN: n->type = n->lhs->type; return;
	case ND_COND:   /* ?: over two arithmetic arms takes the usual-arithmetic-conversion type (so a mixed
	                 * 32/64 ?: is 64-bit and each arm gets widened in codegen); else the `then` arm's type. */
		n->type = (n->then->type && n->els->type && n->then->type->kind < TY_PTR && n->els->type->kind < TY_PTR)
		          ? usual_arith(n->then->type, n->els->type) : n->then->type;
		return;
	case ND_COMMA:  n->type = n->rhs->type; return;    /* value of the right operand */
	case ND_ADDR:   n->type = pointer_to(n->lhs->type); return;
	case ND_DEREF:
		if (!is_ptr_like(n->lhs->type)) die("cc: cannot dereference a non-pointer");
		n->type = n->lhs->type->base; return;
	default: n->type = ty_int; return;                /* statements: type unused */
	}
}
