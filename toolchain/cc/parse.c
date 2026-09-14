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
static Node *cur_switch;                           /* innermost switch, so case/default can attach to it */

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
/* Bind a name to an explicit offset without allocating frame space — for params 5+ that live in the
 * CALLER's frame (above our saved r11/lr), at [r11, #8 + 4*(i-4)]. */
static void add_local_at(const char *name, Type *ty, int off) {
	if (local_exists(name)) die("parse: redeclaration of '%s'", name);
	strncpy(locals[nlocals].name, name, 63); locals[nlocals].offset = off; locals[nlocals].type = ty; nlocals++;
}

/* ---- typedef names + enum constants (both resolved at parse time) -------------------------------- */
static struct { char name[64]; Type *type; } typedefs[256]; static int ntypedefs;
static Type *typedef_find(const char *n) { for (int i = 0; i < ntypedefs; i++) if (!strcmp(typedefs[i].name, n)) return typedefs[i].type; return NULL; }
static void  add_typedef(const char *n, Type *t) { if (ntypedefs < 256) { strncpy(typedefs[ntypedefs].name, n, 63); typedefs[ntypedefs].type = t; ntypedefs++; } }
static struct { char name[64]; long val; } enumc[512]; static int nenumc;
static int   enum_find(const char *n, long *v) { for (int i = 0; i < nenumc; i++) if (!strcmp(enumc[i].name, n)) { *v = enumc[i].val; return 1; } return 0; }

static Type *struct_decl(void);
static Type *enum_decl(void);
static void skip_attribute(void) { expect("("); int d = 1; while (d && tk->kind != TK_EOF) { if (is("(")) d++; else if (is(")")) d--; tk = tk->next; } }

/* declaration-specifiers: fold type keywords, qualifiers, and storage classes. M1 approximations:
 * short/int/long/signed/unsigned all become 4-byte int; void ~ char (so void* scales like char*, per the
 * GNU extension); const/volatile/static/extern/register/inline and __attribute__ are consumed and ignored.
 * `td` (may be NULL) is set to 1 iff `typedef` appears — an out-param, NOT a global, because parsing a
 * struct member type recursively calls declspec and would otherwise clobber the outer typedef flag. */
static Type *declspec(int *td) {
	if (td) *td = 0;
	Type *ty = ty_int; int seen = 0;
	for (;;) {
		if (consume("typedef")) { if (td) *td = 1; continue; }
		if (consume("const") || consume("volatile") || consume("restrict") || consume("static") || consume("extern") || consume("register") || consume("inline") || consume("signed")) continue;
		if (consume("__attribute__")) { skip_attribute(); continue; }
		if (consume("unsigned")) { ty = ty_int;  seen = 1; continue; }
		if (consume("void"))     { ty = ty_char; seen = 1; continue; }
		if (consume("char"))     { ty = ty_char; seen = 1; continue; }
		if (consume("short") || consume("int") || consume("long")) { ty = ty_int; seen = 1; continue; }
		if (consume("struct") || consume("union")) { ty = struct_decl(); seen = 1; continue; }
		if (consume("enum")) { ty = enum_decl(); seen = 1; continue; }
		if (!seen && tk->kind == TK_IDENT && typedef_find(tk->text)) { ty = typedef_find(tk->text); tk = tk->next; seen = 1; continue; }
		break;
	}
	return ty;
}
static Type *type_suffix(Type *base) {
	if (consume("[")) { int n = 0; if (tk->kind == TK_NUM) { n = tk->val; tk = tk->next; }   /* [] (param) allowed */
		expect("]"); return array_of(type_suffix(base), n); }                                 /* outer dim wraps inner */
	return base;
}
/* declarator = "*"* ( "(" "*" name? ")" fn-or-array-suffix | name? ) array-suffix ; the name is optional
 * (abstract declarators in prototypes/casts). A "(*name)(...)" grouping is a function pointer — we don't
 * model function types, so we treat it as a plain 4-byte pointer and skip the pointed-to parameter list. */
static Type *declarator(Type *base, char *name) {
	while (consume("*")) { base = pointer_to(base); while (consume("const") || consume("volatile") || consume("restrict")) ; }   /* `char * const` */
	if (consume("(")) {                                      /* (*name)(...) : pointer to function/array */
		expect("*"); name[0] = 0; if (tk->kind == TK_IDENT) ident(name); expect(")");
		if (is("(")) skip_attribute(); else base = type_suffix(base);   /* skip the function's params */
		return pointer_to(base);
	}
	name[0] = 0; if (tk->kind == TK_IDENT) ident(name);      /* name omitted => abstract declarator */
	return type_suffix(base);
}

/* struct-spec = "struct" tag? ( "{" (declspec declarator ("," declarator)* ";")* "}" )?  — a named
 * definition registers the tag; a bare "struct tag" looks it up. Member offsets are assigned with each
 * member aligned to its own alignment, and the struct's size rounded up to its max member alignment. */
static struct { char name[64]; Type *type; } struct_tags[64]; static int nstruct_tags;
static Type *tag_find(const char *name) { for (int i = 0; i < nstruct_tags; i++) if (!strcmp(struct_tags[i].name, name)) return struct_tags[i].type; return NULL; }
static void  tag_add(const char *name, Type *t) { if (name[0] && nstruct_tags < 64) { strncpy(struct_tags[nstruct_tags].name, name, 63); struct_tags[nstruct_tags].type = t; nstruct_tags++; } }
static Type *struct_decl(void) {
	char tag[64] = ""; if (tk->kind == TK_IDENT) ident(tag);
	if (!is("{")) {                                          /* a reference — forward-declare an incomplete type if new */
		Type *t = tag_find(tag);
		if (!t) { t = calloc(1, sizeof *t); t->kind = TY_STRUCT; tag_add(tag, t); }   /* opaque; pointers to it still work */
		return t;
	}
	Type *ty = tag[0] ? tag_find(tag) : NULL;                /* a definition — fill an existing forward decl in place */
	if (!ty) { ty = calloc(1, sizeof *ty); ty->kind = TY_STRUCT; tag_add(tag, ty); }
	expect("{");
	Member mh = {0}, *mc = &mh; int off = 0, salign = 1;
	while (!consume("}")) {
		Type *base = declspec(NULL);
		do {
			char mname[64]; Type *mt = declarator(base, mname);
			int a = align_of(mt); off = (off + a - 1) & ~(a - 1);   /* align this member */
			Member *m = calloc(1, sizeof *m); strncpy(m->name, mname, 63); m->type = mt; m->offset = off;
			off += mt->size; if (a > salign) salign = a; mc = mc->next = m;
		} while (consume(","));
		expect(";");
	}
	ty->members = mh.next; ty->size = (off + salign - 1) & ~(salign - 1);   /* fills the forward decl in place */
	return ty;
}

/* enum [tag] { NAME [= const] , ... } — registers each constant as an int value; the type is just int. */
static Type *enum_decl(void) {
	if (tk->kind == TK_IDENT) tk = tk->next;               /* optional tag, ignored (enum == int) */
	if (consume("{")) {
		long val = 0;
		while (!is("}")) {
			char nm[64]; ident(nm);
			if (consume("=")) { if (tk->kind != TK_NUM) die("parse: enum value must be an integer constant (line %d)", tk->line); val = tk->val; tk = tk->next; }
			if (nenumc < 512) { strncpy(enumc[nenumc].name, nm, 63); enumc[nenumc].val = val; nenumc++; }
			val++;
			if (!consume(",")) break;
		}
		expect("}");
	}
	return ty_int;
}

/* ---- file-scope objects: globals + string literals ----------------------------------------------- */
Gvar *globals; static Gvar *gtail; static int str_id;
static Gvar *add_global(void) { Gvar *g = calloc(1, sizeof *g); if (gtail) gtail->next = g; else globals = g; gtail = g; return g; }
static Gvar *global_find(const char *name) { for (Gvar *g = globals; g; g = g->next) if (!g->is_str && !strcmp(g->name, name)) return g; return NULL; }

/* ---- node constructors --------------------------------------------------------------------------- */
static int is_typename(void) {   /* does a declaration start at the cursor? */
	return is("int") || is("char") || is("void") || is("short") || is("long") || is("signed") || is("unsigned")
	    || is("struct") || is("union") || is("enum") || is("typedef")
	    || is("const") || is("volatile") || is("static") || is("extern") || is("register") || is("inline") || is("__attribute__")
	    || (tk->kind == TK_IDENT && typedef_find(tk->text));
}
static Node *node(NodeKind k) { Node *n = calloc(1, sizeof *n); n->kind = k; return n; }
static Node *binary(NodeKind k, Node *l, Node *r) { Node *n = node(k); n->lhs = l; n->rhs = r; return n; }
static Node *unary(NodeKind k, Node *e) { Node *n = node(k); n->lhs = e; return n; }
static Node *num(long v) { Node *n = node(ND_NUM); n->val = v; return n; }

/* ---- expression grammar (each returns the parsed subtree; result convention lives in gen.c) ------- */
static Node *expr(void);
static Node *assign(void);
static Node *new_add(Node *l, Node *r);       /* +/- with pointer/array scaling (defined below) */
static Node *new_sub(Node *l, Node *r);

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
			if (!is(")")) { do { ac = ac->next = assign(); } while (consume(",")); }   /* assign(), so ',' separates args */
			expect(")"); n->args = argh.next; return n;
		}
		if (local_exists(name)) { Node *n = node(ND_VAR); strncpy(n->name, name, 63); n->offset = local_offset(name); n->type = local_type(name); return n; }
		Gvar *g = global_find(name);                         /* locals shadow globals */
		if (g) { Node *n = node(ND_GVAR); strncpy(n->name, name, 63); n->type = g->type; return n; }
		long ev; if (enum_find(name, &ev)) return num(ev);   /* enum constant -> integer literal */
		die("parse: use of undeclared '%s' (line %d)", name, tk->line);
	}
	die("parse: unexpected '%s' (line %d)", tk->text, tk->line); return NULL;
}

/* base.member — resolve the member's offset+type on the struct; ND_MEMBER holds the base lvalue. */
static Node *struct_member(Node *base, const char *mname) {
	add_type(base);
	if (!base->type || base->type->kind != TY_STRUCT) die("parse: '.%s' on a non-struct", mname);
	for (Member *m = base->type->members; m; m = m->next) if (!strcmp(m->name, mname)) {
		Node *n = node(ND_MEMBER); n->lhs = base; n->offset = m->offset; n->type = m->type; return n;
	}
	die("parse: struct has no member '%s'", mname); return NULL;
}

/* postfix := primary ( "[" expr "]" | "." ident | "->" ident )* ; a[i] = *(a+i), p->m = (*p).m. */
static Node *postfix(void) {
	Node *n = primary();
	for (;;) {
		if (consume("[")) { Node *idx = expr(); expect("]"); n = unary(ND_DEREF, new_add(n, idx)); }
		else if (consume(".")) { char m[64]; ident(m); n = struct_member(n, m); }
		else if (consume("->")) { char m[64]; ident(m); n = struct_member(unary(ND_DEREF, n), m); }
		else if (consume("++")) n = new_sub(binary(ND_ASSIGN, n, new_add(n, num(1))), num(1));   /* x++ = (x=x+1)-1 */
		else if (consume("--")) n = new_add(binary(ND_ASSIGN, n, new_sub(n, num(1))), num(1));   /* x-- = (x=x-1)+1 */
		else return n;
	}
}

/* Is the cursor at a cast `(type)`? Peek past "(" without consuming. */
static int cast_ahead(void) {
	if (!is("(")) return 0;
	Token *save = tk; tk = tk->next; int r = is_typename(); tk = save; return r;
}

static Node *unary_expr(void) {
	if (cast_ahead()) {                                      /* (type) expr — re-types the operand */
		expect("("); Type *t = declspec(NULL); while (consume("*")) t = pointer_to(t); expect(")");
		Node *n = node(ND_CAST); n->lhs = unary_expr(); n->type = t; return n;
	}
	if (consume("sizeof")) {                                 /* sizeof(type) or sizeof expr -> a constant */
		if (cast_ahead()) { expect("("); Type *t = declspec(NULL); while (consume("*")) t = pointer_to(t); t = type_suffix(t); expect(")"); return num(t->size); }
		Node *e = unary_expr(); add_type(e); return num(e->type ? e->type->size : 4);
	}
	if (consume("++")) { Node *x = unary_expr(); return binary(ND_ASSIGN, x, new_add(x, num(1))); }   /* ++x */
	if (consume("--")) { Node *x = unary_expr(); return binary(ND_ASSIGN, x, new_sub(x, num(1))); }   /* --x */
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
static Node *conditional(void){ Node *c = logor(); if (!consume("?")) return c;   /* c ? then : els */
	Node *n = node(ND_COND); n->cond = c; n->then = expr(); expect(":"); n->els = conditional(); return n; }
/* assignment, incl. compound forms desugared to `a = a OP b` (new_add/new_sub keep pointer scaling). */
static Node *assign(void) {
	Node *n = conditional();
	if (!(is("=") || is("+=") || is("-=") || is("*=") || is("/=") || is("%=") || is("&=") || is("|=") || is("^=") || is("<<=") || is(">>="))) return n;
	if (n->kind != ND_VAR && n->kind != ND_GVAR && n->kind != ND_DEREF && n->kind != ND_MEMBER) die("parse: assignment to non-lvalue (line %d)", tk->line);
	char op[4]; strncpy(op, tk->text, 3); op[3] = 0; tk = tk->next;
	Node *rhs = assign(), *val;
	if      (!strcmp(op, "="))   val = rhs;
	else if (!strcmp(op, "+="))  val = new_add(n, rhs);
	else if (!strcmp(op, "-="))  val = new_sub(n, rhs);
	else if (!strcmp(op, "*="))  val = binary(ND_MUL, n, rhs);
	else if (!strcmp(op, "/="))  val = binary(ND_DIV, n, rhs);
	else if (!strcmp(op, "%="))  val = binary(ND_MOD, n, rhs);
	else if (!strcmp(op, "&="))  val = binary(ND_BITAND, n, rhs);
	else if (!strcmp(op, "|="))  val = binary(ND_BITOR, n, rhs);
	else if (!strcmp(op, "^="))  val = binary(ND_BITXOR, n, rhs);
	else if (!strcmp(op, "<<=")) val = binary(ND_SHL, n, rhs);
	else                         val = binary(ND_SHR, n, rhs);   /* >>= */
	return binary(ND_ASSIGN, n, val);
}
static Node *expr(void)  { Node *n = assign(); while (consume(",")) n = binary(ND_COMMA, n, assign()); return n; }   /* comma operator */

/* ---- statements ---------------------------------------------------------------------------------- */
static Node *stmt(void) {
	if (consume("switch")) {                                 /* switch (e) body ; cases attach to it */
		Node *n = node(ND_SWITCH); expect("("); n->cond = expr(); expect(")");
		Node *save = cur_switch; cur_switch = n; n->then = stmt(); cur_switch = save;
		return n;
	}
	if (consume("case")) {                                   /* case CONST: */
		if (!cur_switch) die("parse: 'case' outside switch");
		Node *c = conditional(); if (c->kind != ND_NUM) die("parse: case label must be a constant (line %d)", tk->line);
		expect(":");
		Node *n = node(ND_CASE); n->val = c->val; n->case_next = cur_switch->case_list; cur_switch->case_list = n;
		return n;
	}
	if (consume("default")) { if (!cur_switch) die("parse: 'default' outside switch"); expect(":");
		Node *n = node(ND_CASE); n->is_default = 1; n->case_next = cur_switch->case_list; cur_switch->case_list = n; return n; }
	if (consume("break"))    { expect(";"); return node(ND_BREAK); }
	if (consume("continue")) { expect(";"); return node(ND_CONTINUE); }
	if (consume("return")) { Node *n = unary(ND_RETURN, expr()); expect(";"); return n; }
	if (consume("if")) { Node *n = node(ND_IF); expect("("); n->cond = expr(); expect(")"); n->then = stmt(); if (consume("else")) n->els = stmt(); return n; }
	if (consume("while")) { Node *n = node(ND_WHILE); expect("("); n->cond = expr(); expect(")"); n->body = stmt(); return n; }
	if (consume("for")) {                                    /* for (init; cond; inc) body — any part may be empty */
		Node *n = node(ND_FOR); expect("(");
		if (is_typename()) n->init = stmt();       /* declaration eats its own ; */
		else if (!consume(";")) { n->init = unary(ND_EXPRSTMT, expr()); expect(";"); }
		if (!consume(";")) { n->cond = expr(); expect(";"); }
		if (!is(")")) n->inc = expr();
		expect(")"); n->body = stmt(); return n;
	}
	if (consume("{")) { Node *n = node(ND_BLOCK); Node h = {0}, *c = &h; while (!consume("}")) c = c->next = stmt(); n->body = h.next; return n; }
	if (is_typename()) {                                     /* local declaration(s): `T a, b = e, c;` */
		int td; Type *base = declspec(&td);
		if (td) { char nm[64]; Type *ty = declarator(base, nm); add_typedef(nm, ty); expect(";"); return node(ND_BLOCK); }
		if (consume(";")) return node(ND_BLOCK);             /* type-only (e.g. a struct definition) */
		Node blk = {0}, *bc = &blk;                          /* each initializer becomes a statement in a block */
		do {
			char nm[64]; Type *ty = declarator(base, nm); int off = add_local(nm, ty);
			if (consume("=")) { Node *v = node(ND_VAR); strncpy(v->name, nm, 63); v->offset = off; v->type = ty;
				bc = bc->next = unary(ND_EXPRSTMT, binary(ND_ASSIGN, v, assign())); }   /* assign(): ',' separates declarators */
		} while (consume(","));
		expect(";");
		Node *n = node(ND_BLOCK); n->body = blk.next; return n;
	}
	Node *n = unary(ND_EXPRSTMT, expr()); expect(";"); return n;
}

/* ---- functions ----------------------------------------------------------------------------------- */
/* The name + return type have already been read; the cursor is at "(". Parse params + body. */
static Func *function_tail(const char *name) {
	Func *f = calloc(1, sizeof *f); strncpy(f->name, name, 63);
	nlocals = 0; local_bytes = 0;
	expect("(");
	if (is("void") && !strcmp(tk->next->text, ")")) tk = tk->next;   /* (void) = no params */
	else if (!is(")")) {
		do {
			if (is(".")) { while (consume(".")) ; break; }   /* variadic `...` — parsed, not yet implemented */
			char p[64]; Type *ty = declarator(declspec(NULL), p);
			if (ty->kind == TY_ARRAY) ty = pointer_to(ty->base);   /* array param decays to pointer */
			if (p[0]) { if (f->nparams < 4) add_local(p, ty); else add_local_at(p, ty, 8 + 4 * (f->nparams - 4)); }
			f->nparams++;
		} while (consume(","));
	}
	expect(")");
	while (consume("__attribute__")) skip_attribute();       /* e.g. int f(void) __attribute__((noreturn)) { … } */
	if (consume(";")) return NULL;                           /* a prototype — no body to compile */
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
		if (tk->kind == TK_IDENT && !strcmp(tk->text, "_Static_assert")) { tk = tk->next; skip_attribute(); consume(";"); continue; }
		int td; Type *base = declspec(&td);                  /* type keywords/qualifiers (struct/enum defs register) */
		if (consume(";")) continue;                          /* type-only declaration, e.g. `struct P { ... };`   */
		if (td) { char nm[64]; Type *ty = declarator(base, nm); add_typedef(nm, ty); expect(";"); continue; }
		char name[64]; Type *ty = declarator(base, name);    /* *s + name + array suffix */
		if (is("(")) { Func *fn = function_tail(name); if (fn) cur = cur->next = fn; continue; }   /* NULL = prototype */
		for (;;) {                                           /* global variable(s), comma-separated */
			Gvar *g = add_global(); strncpy(g->name, name, 63); g->type = ty;
			if (consume("=")) { if (tk->kind != TK_NUM) die("parse: global initializer must be an integer constant (line %d)", tk->line);
				g->has_init = 1; g->init = tk->val; tk = tk->next; }
			if (!consume(",")) break;
			ty = declarator(base, name);
		}
		expect(";");
	}
	return head.next;
}
