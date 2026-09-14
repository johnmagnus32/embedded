/*
 * parse.c — recursive-descent parser: tokens -> a list of Funcs, each with an AST body. Operator
 * precedence follows C (assignment lowest, then || && | ^ & == < << +  * unary). Local variables (params
 * and `int` declarations) are assigned stack slots as they're seen: the Nth local lives at [fp, #-4*(N+1)],
 * and the function's frame is 4*nlocals rounded up to an 8-byte (AAPCS) boundary.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cc.h"

static Token *tk;                                  /* the parse cursor */

/* ---- token helpers ------------------------------------------------------------------------------- */
static int is(const char *s)     { return (tk->kind == TK_PUNCT || tk->kind == TK_KW) && !strcmp(tk->text, s); }
static int consume(const char *s){ if (is(s)) { tk = tk->next; return 1; } return 0; }
static void expect(const char *s){ if (!consume(s)) die("parse: expected '%s' but got '%s' (line %d)", s, tk->text, tk->line); }
static void ident(char *out)     { if (tk->kind != TK_IDENT) die("parse: expected identifier, got '%s' (line %d)", tk->text, tk->line);
                                   strncpy(out, tk->text, 63); out[63] = 0; tk = tk->next; }

/* ---- locals (per function) ----------------------------------------------------------------------- */
static struct { char name[64]; int offset; Type *type; } locals[128];
static int nlocals, local_bytes;   /* local_bytes = total frame bytes used by locals+params so far */
static int local_offset(const char *name) { for (int i = 0; i < nlocals; i++) if (!strcmp(locals[i].name, name)) return locals[i].offset; return 0; }
static Type *local_type(const char *name) { for (int i = 0; i < nlocals; i++) if (!strcmp(locals[i].name, name)) return locals[i].type; return ty_int; }
static int local_exists(const char *name) { for (int i = 0; i < nlocals; i++) if (!strcmp(locals[i].name, name)) return 1; return 0; }
static int add_local(const char *name, Type *ty) {
	if (local_exists(name)) die("parse: redeclaration of '%s'", name);
	local_bytes += (ty->size + 3) & ~3;   /* a 4-aligned slot big enough for the whole object (arrays too) */
	int off = -local_bytes;               /* offset points at the object's first (lowest) byte */
	strncpy(locals[nlocals].name, name, 63); locals[nlocals].offset = off; locals[nlocals].type = ty; nlocals++;
	return off;
}

/* type = ("int"|"char") "*"* name ("[" num "]")* ; declarator reads the name and any array suffix. */
static Type *declspec(void) { if (consume("int")) return ty_int; if (consume("char")) return ty_char; die("parse: expected a type (line %d)", tk->line); return NULL; }
static Type *type_suffix(Type *base) {
	if (consume("[")) { if (tk->kind != TK_NUM) die("parse: array length must be an integer (line %d)", tk->line);
		int n = tk->val; tk = tk->next; expect("]"); return array_of(type_suffix(base), n); }   /* outer dim wraps inner */
	return base;
}
static Type *declarator(Type *base, char *name) { while (consume("*")) base = pointer_to(base); ident(name); return type_suffix(base); }

/* ---- file-scope objects: globals + string literals ----------------------------------------------- */
Gvar *globals; static Gvar *gtail; static int str_id;
static Gvar *add_global(void) { Gvar *g = calloc(1, sizeof *g); if (gtail) gtail->next = g; else globals = g; gtail = g; return g; }
static Gvar *global_find(const char *name) { for (Gvar *g = globals; g; g = g->next) if (!g->is_str && !strcmp(g->name, name)) return g; return NULL; }

/* ---- node constructors --------------------------------------------------------------------------- */
static Node *node(NodeKind k) { Node *n = calloc(1, sizeof *n); n->kind = k; return n; }
static Node *binary(NodeKind k, Node *l, Node *r) { Node *n = node(k); n->lhs = l; n->rhs = r; return n; }
static Node *unary(NodeKind k, Node *e) { Node *n = node(k); n->lhs = e; return n; }
static Node *num(long v) { Node *n = node(ND_NUM); n->val = v; return n; }

/* ---- expression grammar (each returns the parsed subtree; result convention lives in gen.c) ------- */
static Node *expr(void);
static Node *new_add(Node *l, Node *r);       /* +/- with pointer/array scaling (defined below) */

static Node *primary(void) {
	if (consume("(")) { Node *n = expr(); expect(")"); return n; }
	if (tk->kind == TK_NUM) { Node *n = num(tk->val); tk = tk->next; return n; }
	if (tk->kind == TK_STR) {                                /* string literal -> anonymous .rodata array */
		Gvar *g = add_global(); g->is_str = 1; g->type = ty_char;
		snprintf(g->name, sizeof g->name, ".LSTR%d", str_id++);
		strncpy(g->str, tk->text, sizeof g->str - 1); tk = tk->next;
		Node *gv = node(ND_GVAR); strncpy(gv->name, g->name, 63); gv->type = ty_char;
		return unary(ND_ADDR, gv);                           /* its value is &(first byte) : char* */
	}
	if (tk->kind == TK_IDENT) {
		char name[64]; ident(name);
		if (consume("(")) {                                  /* function call name(args) */
			Node *n = node(ND_CALL); strncpy(n->name, name, 63);
			Node argh = {0}, *ac = &argh;
			if (!is(")")) { do { ac = ac->next = expr(); } while (consume(",")); }
			expect(")"); n->args = argh.next; return n;
		}
		if (local_exists(name)) { Node *n = node(ND_VAR); strncpy(n->name, name, 63); n->offset = local_offset(name); n->type = local_type(name); return n; }
		Gvar *g = global_find(name);                         /* locals shadow globals */
		if (g) { Node *n = node(ND_GVAR); strncpy(n->name, name, 63); n->type = g->type; return n; }
		die("parse: use of undeclared '%s' (line %d)", name, tk->line);
	}
	die("parse: unexpected '%s' (line %d)", tk->text, tk->line); return NULL;
}

/* postfix := primary ("[" expr "]")* ; a[i] is sugar for *(a + i), so it rides on new_add + deref. */
static Node *postfix(void) {
	Node *n = primary();
	while (consume("[")) { Node *idx = expr(); expect("]"); n = unary(ND_DEREF, new_add(n, idx)); }
	return n;
}

static Node *unary_expr(void) {
	if (consume("&")) return unary(ND_ADDR, unary_expr());   /* address-of */
	if (consume("*")) return unary(ND_DEREF, unary_expr());  /* dereference */
	if (consume("-")) return unary(ND_NEG, unary_expr());
	if (consume("!")) return unary(ND_NOT, unary_expr());
	if (consume("~")) return unary(ND_BITNOT, unary_expr());
	if (consume("+")) return unary_expr();                   /* unary plus is a no-op */
	return postfix();
}

/* +/- with C pointer semantics: `ptr + int` scales the int by the pointee size; `int + ptr` is
 * commuted to `ptr + int`; `ptr - ptr` is the element distance (difference / pointee size). */
static Node *new_add(Node *l, Node *r) {
	add_type(l); add_type(r);
	if (is_ptr_like(l->type) && is_ptr_like(r->type)) die("parse: cannot add two pointers");
	if (!is_ptr_like(l->type) && is_ptr_like(r->type)) { Node *t = l; l = r; r = t; }
	if (is_ptr_like(l->type)) r = binary(ND_MUL, r, num(l->type->base->size));   /* scale by element size */
	return binary(ND_ADD, l, r);
}
static Node *new_sub(Node *l, Node *r) {
	add_type(l); add_type(r);
	if (is_ptr_like(l->type) && is_ptr_like(r->type)) return binary(ND_DIV, binary(ND_SUB, l, r), num(l->type->base->size));
	if (is_ptr_like(l->type)) r = binary(ND_MUL, r, num(l->type->base->size));
	return binary(ND_SUB, l, r);
}
static Node *mul(void)   { Node *n = unary_expr(); for (;;) { if (consume("*")) n = binary(ND_MUL, n, unary_expr()); else if (consume("/")) n = binary(ND_DIV, n, unary_expr()); else if (consume("%")) n = binary(ND_MOD, n, unary_expr()); else return n; } }
static Node *add(void)   { Node *n = mul();         for (;;) { if (consume("+")) n = new_add(n, mul()); else if (consume("-")) n = new_sub(n, mul()); else return n; } }
static Node *shift(void) { Node *n = add();         for (;;) { if (consume("<<")) n = binary(ND_SHL, n, add()); else if (consume(">>")) n = binary(ND_SHR, n, add()); else return n; } }
static Node *rel(void)   { Node *n = shift();       for (;;) { if (consume("<")) n = binary(ND_LT, n, shift()); else if (consume("<=")) n = binary(ND_LE, n, shift()); else if (consume(">")) n = binary(ND_GT, n, shift()); else if (consume(">=")) n = binary(ND_GE, n, shift()); else return n; } }
static Node *eq(void)    { Node *n = rel();         for (;;) { if (consume("==")) n = binary(ND_EQ, n, rel()); else if (consume("!=")) n = binary(ND_NE, n, rel()); else return n; } }
static Node *bitand(void){ Node *n = eq();          while (consume("&")) n = binary(ND_BITAND, n, eq()); return n; }
static Node *bitxor(void){ Node *n = bitand();      while (consume("^")) n = binary(ND_BITXOR, n, bitand()); return n; }
static Node *bitor(void) { Node *n = bitxor();      while (consume("|")) n = binary(ND_BITOR, n, bitxor()); return n; }
static Node *logand(void){ Node *n = bitor();       while (consume("&&")) n = binary(ND_AND, n, bitor()); return n; }
static Node *logor(void) { Node *n = logand();      while (consume("||")) n = binary(ND_OR, n, logand()); return n; }
static Node *assign(void){ Node *n = logor(); if (consume("=")) { if (n->kind != ND_VAR && n->kind != ND_GVAR && n->kind != ND_DEREF) die("parse: assignment to non-lvalue"); n = binary(ND_ASSIGN, n, assign()); } return n; }
static Node *expr(void)  { return assign(); }

/* ---- statements ---------------------------------------------------------------------------------- */
static Node *stmt(void) {
	if (consume("return")) { Node *n = unary(ND_RETURN, expr()); expect(";"); return n; }
	if (consume("if")) { Node *n = node(ND_IF); expect("("); n->cond = expr(); expect(")"); n->then = stmt(); if (consume("else")) n->els = stmt(); return n; }
	if (consume("while")) { Node *n = node(ND_WHILE); expect("("); n->cond = expr(); expect(")"); n->body = stmt(); return n; }
	if (consume("{")) { Node *n = node(ND_BLOCK); Node h = {0}, *c = &h; while (!consume("}")) c = c->next = stmt(); n->body = h.next; return n; }
	if (is("int") || is("char")) {                           /* `T *…* name [= expr];` local declaration */
		char name[64]; Type *ty = declarator(declspec(), name); int off = add_local(name, ty);
		Node *n;
		if (consume("=")) { Node *v = node(ND_VAR); strncpy(v->name, name, 63); v->offset = off; v->type = ty; n = unary(ND_EXPRSTMT, binary(ND_ASSIGN, v, expr())); }
		else n = node(ND_BLOCK);                             /* bare declaration: no code */
		expect(";"); return n;
	}
	Node *n = unary(ND_EXPRSTMT, expr()); expect(";"); return n;
}

/* ---- functions ----------------------------------------------------------------------------------- */
/* The name + return type have already been read; the cursor is at "(". Parse params + body. */
static Func *function_tail(const char *name) {
	Func *f = calloc(1, sizeof *f); strncpy(f->name, name, 63);
	nlocals = 0; local_bytes = 0;
	expect("(");
	if (!is(")") && !is("void")) { do { char p[64]; Type *ty = declarator(declspec(), p); add_local(p, ty); f->nparams++; } while (consume(",")); }
	else consume("void");
	expect(")");
	expect("{");
	Node h = {0}, *c = &h; while (!consume("}")) c = c->next = stmt();
	f->body = h.next;
	for (Node *s = f->body; s; s = s->next) add_type(s);      /* annotate every node with its result type */
	f->frame = (local_bytes + 7) & ~7;                       /* 8-byte aligned frame (locals+params, arrays sized) */
	return f;
}

/* Top level: read a type + name, then dispatch — "(" means a function, anything else a global variable
 * (optionally with a constant integer initializer). `void` is only valid as a function return type. */
Func *parse(Token *tok) {
	tk = tok;
	Func head = {0}, *cur = &head;
	while (tk->kind != TK_EOF) {
		char name[64]; Type *ty;
		if (consume("void")) { ty = NULL; ident(name); }     /* void return */
		else ty = declarator(declspec(), name);              /* int/char (+ *s) then the name */
		if (is("(")) { cur = cur->next = function_tail(name); continue; }
		if (!ty) die("parse: 'void' variable '%s'", name);
		Gvar *g = add_global(); strncpy(g->name, name, 63); g->type = ty;   /* a global variable */
		if (consume("=")) { if (tk->kind != TK_NUM) die("parse: global initializer must be an integer constant (line %d)", tk->line);
			g->has_init = 1; g->init = tk->val; tk = tk->next; }
		expect(";");
	}
	return head.next;
}
