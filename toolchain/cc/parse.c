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
static struct { char name[64]; int offset; } locals[128];
static int nlocals;
static int local_offset(const char *name) { for (int i = 0; i < nlocals; i++) if (!strcmp(locals[i].name, name)) return locals[i].offset; return 0; }
static int local_exists(const char *name) { for (int i = 0; i < nlocals; i++) if (!strcmp(locals[i].name, name)) return 1; return 0; }
static int add_local(const char *name) {
	if (local_exists(name)) die("parse: redeclaration of '%s'", name);
	int off = -4 * (nlocals + 1); strncpy(locals[nlocals].name, name, 63); locals[nlocals].offset = off; nlocals++;
	return off;
}

/* ---- node constructors --------------------------------------------------------------------------- */
static Node *node(NodeKind k) { Node *n = calloc(1, sizeof *n); n->kind = k; return n; }
static Node *binary(NodeKind k, Node *l, Node *r) { Node *n = node(k); n->lhs = l; n->rhs = r; return n; }
static Node *unary(NodeKind k, Node *e) { Node *n = node(k); n->lhs = e; return n; }
static Node *num(long v) { Node *n = node(ND_NUM); n->val = v; return n; }

/* ---- expression grammar (each returns the parsed subtree; result convention lives in gen.c) ------- */
static Node *expr(void);

static Node *primary(void) {
	if (consume("(")) { Node *n = expr(); expect(")"); return n; }
	if (tk->kind == TK_NUM) { Node *n = num(tk->val); tk = tk->next; return n; }
	if (tk->kind == TK_IDENT) {
		char name[64]; ident(name);
		if (consume("(")) {                                  /* function call name(args) */
			Node *n = node(ND_CALL); strncpy(n->name, name, 63);
			Node argh = {0}, *ac = &argh;
			if (!is(")")) { do { ac = ac->next = expr(); } while (consume(",")); }
			expect(")"); n->args = argh.next; return n;
		}
		if (!local_exists(name)) die("parse: use of undeclared '%s' (line %d)", name, tk->line);
		Node *n = node(ND_VAR); strncpy(n->name, name, 63); n->offset = local_offset(name); return n;
	}
	die("parse: unexpected '%s' (line %d)", tk->text, tk->line); return NULL;
}

static Node *unary_expr(void) {
	if (consume("-")) return unary(ND_NEG, unary_expr());
	if (consume("!")) return unary(ND_NOT, unary_expr());
	if (consume("~")) return unary(ND_BITNOT, unary_expr());
	if (consume("+")) return unary_expr();                   /* unary plus is a no-op */
	return primary();
}
static Node *mul(void)   { Node *n = unary_expr(); for (;;) { if (consume("*")) n = binary(ND_MUL, n, unary_expr()); else if (consume("/")) n = binary(ND_DIV, n, unary_expr()); else if (consume("%")) n = binary(ND_MOD, n, unary_expr()); else return n; } }
static Node *add(void)   { Node *n = mul();         for (;;) { if (consume("+")) n = binary(ND_ADD, n, mul()); else if (consume("-")) n = binary(ND_SUB, n, mul()); else return n; } }
static Node *shift(void) { Node *n = add();         for (;;) { if (consume("<<")) n = binary(ND_SHL, n, add()); else if (consume(">>")) n = binary(ND_SHR, n, add()); else return n; } }
static Node *rel(void)   { Node *n = shift();       for (;;) { if (consume("<")) n = binary(ND_LT, n, shift()); else if (consume("<=")) n = binary(ND_LE, n, shift()); else if (consume(">")) n = binary(ND_GT, n, shift()); else if (consume(">=")) n = binary(ND_GE, n, shift()); else return n; } }
static Node *eq(void)    { Node *n = rel();         for (;;) { if (consume("==")) n = binary(ND_EQ, n, rel()); else if (consume("!=")) n = binary(ND_NE, n, rel()); else return n; } }
static Node *bitand(void){ Node *n = eq();          while (consume("&")) n = binary(ND_BITAND, n, eq()); return n; }
static Node *bitxor(void){ Node *n = bitand();      while (consume("^")) n = binary(ND_BITXOR, n, bitand()); return n; }
static Node *bitor(void) { Node *n = bitxor();      while (consume("|")) n = binary(ND_BITOR, n, bitxor()); return n; }
static Node *logand(void){ Node *n = bitor();       while (consume("&&")) n = binary(ND_AND, n, bitor()); return n; }
static Node *logor(void) { Node *n = logand();      while (consume("||")) n = binary(ND_OR, n, logand()); return n; }
static Node *assign(void){ Node *n = logor(); if (consume("=")) { if (n->kind != ND_VAR) die("parse: assignment to non-lvalue"); n = binary(ND_ASSIGN, n, assign()); } return n; }
static Node *expr(void)  { return assign(); }

/* ---- statements ---------------------------------------------------------------------------------- */
static Node *stmt(void) {
	if (consume("return")) { Node *n = unary(ND_RETURN, expr()); expect(";"); return n; }
	if (consume("if")) { Node *n = node(ND_IF); expect("("); n->cond = expr(); expect(")"); n->then = stmt(); if (consume("else")) n->els = stmt(); return n; }
	if (consume("while")) { Node *n = node(ND_WHILE); expect("("); n->cond = expr(); expect(")"); n->body = stmt(); return n; }
	if (consume("{")) { Node *n = node(ND_BLOCK); Node h = {0}, *c = &h; while (!consume("}")) c = c->next = stmt(); n->body = h.next; return n; }
	if (is("int")) {                                         /* `int name [= expr];` local declaration */
		tk = tk->next; char name[64]; ident(name); int off = add_local(name);
		Node *n;
		if (consume("=")) { Node *v = node(ND_VAR); strncpy(v->name, name, 63); v->offset = off; n = unary(ND_EXPRSTMT, binary(ND_ASSIGN, v, expr())); }
		else n = node(ND_BLOCK);                             /* bare declaration: no code */
		expect(";"); return n;
	}
	Node *n = unary(ND_EXPRSTMT, expr()); expect(";"); return n;
}

/* ---- functions ----------------------------------------------------------------------------------- */
static Func *function(void) {
	if (!consume("int") && !consume("void")) die("parse: expected 'int'/'void' return type (line %d)", tk->line);
	Func *f = calloc(1, sizeof *f); ident(f->name);
	nlocals = 0;
	expect("(");
	if (!is(")") && !is("void")) { do { expect("int"); char p[64]; ident(p); add_local(p); f->nparams++; } while (consume(",")); }
	else consume("void");
	expect(")");
	expect("{");
	Node h = {0}, *c = &h; while (!consume("}")) c = c->next = stmt();
	f->body = h.next;
	f->frame = (nlocals * 4 + 7) & ~7;                       /* 8-byte aligned frame for locals+params */
	return f;
}

Func *parse(Token *tok) {
	tk = tok;
	Func head = {0}, *cur = &head;
	while (tk->kind != TK_EOF) cur = cur->next = function();
	return head.next;
}
