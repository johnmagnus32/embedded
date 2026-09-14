/*
 * type.c — the (minimal) type system: int, char, and pointer-to. Two jobs: provide the type singletons +
 * constructors, and walk a parsed subtree bottom-up assigning each node a result Type (add_type). Codegen
 * needs those types for two decisions: the LOAD/STORE WIDTH of an lvalue (char = 1 byte, int/pointer = 4)
 * and — via the parser's new_add/new_sub — POINTER ARITHMETIC scaling. Everything is 4-byte-aligned; char
 * is treated as unsigned (ARM's default), so byte access zero-extends.
 */
#include <stdlib.h>
#include "cc.h"

static Type int_ty  = { TY_INT,  NULL, 4, 0 };
static Type char_ty = { TY_CHAR, NULL, 1, 0 };
Type *ty_int  = &int_ty;
Type *ty_char = &char_ty;

Type *pointer_to(Type *base) { Type *t = calloc(1, sizeof *t); t->kind = TY_PTR; t->base = base; t->size = 4; return t; }
Type *array_of(Type *base, int len) { Type *t = calloc(1, sizeof *t); t->kind = TY_ARRAY; t->base = base; t->len = len; t->size = base->size * len; return t; }
int   is_ptr(Type *t) { return t && t->kind == TY_PTR; }
int   is_ptr_like(Type *t) { return t && (t->kind == TY_PTR || t->kind == TY_ARRAY); }

/* Recursively annotate a subtree with result types (children first, then the node itself). Idempotent
 * enough to run over a whole function body; leaves set by the parser (ND_VAR/ND_NUM) are respected. */
void add_type(Node *n) {
	if (!n || n->type) return;
	add_type(n->lhs); add_type(n->rhs);
	add_type(n->cond); add_type(n->then); add_type(n->els);
	for (Node *c = n->body; c; c = c->next) add_type(c);
	for (Node *a = n->args; a; a = a->next) add_type(a);

	switch (n->kind) {
	case ND_NUM: n->type = ty_int; return;
	case ND_ADD: case ND_SUB:
		if (n->lhs->type->kind == TY_ARRAY) n->type = pointer_to(n->lhs->type->base);   /* array decays to pointer */
		else n->type = n->lhs->type;                  /* pointer stays pointer, int stays int */
		return;
	case ND_MUL: case ND_DIV: case ND_MOD:
	case ND_EQ: case ND_NE: case ND_LT: case ND_LE: case ND_GT: case ND_GE:
	case ND_AND: case ND_OR: case ND_NOT:
	case ND_BITAND: case ND_BITOR: case ND_BITXOR: case ND_SHL: case ND_SHR:
	case ND_NEG: case ND_BITNOT: case ND_CALL:
		n->type = ty_int; return;                     /* all yield an int */
	case ND_ASSIGN: n->type = n->lhs->type; return;
	case ND_ADDR:   n->type = pointer_to(n->lhs->type); return;
	case ND_DEREF:
		if (!is_ptr_like(n->lhs->type)) die("cc: cannot dereference a non-pointer");
		n->type = n->lhs->type->base; return;
	default: n->type = ty_int; return;                /* statements: type unused */
	}
}
