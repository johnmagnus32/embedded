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
static struct { char name[64]; int offset; Type *type; char reg[8]; } locals[1024];
static int nlocals, local_bytes;   /* local_bytes = total frame bytes used by locals+params so far */
/* Lookups scan newest-first, so a redeclared name (a different block, no true block scoping here) or a
 * local shadowing a param resolves to the most recent binding — correct for disjoint/nested blocks; we
 * just never reclaim an inner block's frame space. */
static int local_offset(const char *name) { for (int i = nlocals - 1; i >= 0; i--) if (!strcmp(locals[i].name, name)) return locals[i].offset; return 0; }
static Type *local_type(const char *name) { for (int i = nlocals - 1; i >= 0; i--) if (!strcmp(locals[i].name, name)) return locals[i].type; return ty_int; }
static const char *local_reg(const char *name) { for (int i = nlocals - 1; i >= 0; i--) if (!strcmp(locals[i].name, name)) return locals[i].reg; return ""; }
static int local_exists(const char *name) { for (int i = nlocals - 1; i >= 0; i--) if (!strcmp(locals[i].name, name)) return 1; return 0; }
static int add_local(const char *name, Type *ty) {
	if (nlocals >= 1024) die("parse: too many locals in one function");
	local_bytes += (ty->size + 3) & ~3;   /* a 4-aligned slot big enough for the whole object (arrays too) */
	int off = -local_bytes;               /* offset points at the object's first (lowest) byte */
	strncpy(locals[nlocals].name, name, 63); locals[nlocals].offset = off; locals[nlocals].type = ty; nlocals++;
	return off;
}
/* Bind a name to an explicit offset without allocating frame space — for params 5+ that live in the
 * CALLER's frame (above our saved r11/lr), at [r11, #8 + 4*(i-4)]. */
static void add_local_at(const char *name, Type *ty, int off) {
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
static long eval_const(Node *n); static Node *assign(void);
/* Parse `__attribute__((...))` (the `__attribute__` already consumed) for the LAYOUT attributes we honor:
 * `packed` -> *packed=1, `aligned(N)` -> *alignb=N. Unknown attributes (with any (...) payload) are skipped.
 * A name may be spelled bare or double-underscored (packed / __packed__). */
static void parse_attribute(int *packed, int *alignb) {
	expect("("); expect("(");
	while (!is(")") && tk->kind != TK_EOF) {   /* attribute names are plain identifiers, so match on text */
		if (!strcmp(tk->text, "packed") || !strcmp(tk->text, "__packed__")) { if (packed) *packed = 1; tk = tk->next; }
		else if (!strcmp(tk->text, "aligned") || !strcmp(tk->text, "__aligned__")) { tk = tk->next; if (consume("(")) { int n = (int)eval_const(assign()); if (alignb && n > *alignb) *alignb = n; expect(")"); } }
		else { tk = tk->next; if (is("(")) { int d = 0; do { if (is("(")) d++; else if (is(")")) d--; tk = tk->next; } while (d && tk->kind != TK_EOF); } }
		if (!consume(",")) break;
	}
	expect(")"); expect(")");
}

/* declaration-specifiers: fold type keywords, qualifiers, and storage classes. Width comes from the
 * base keyword (char=1, short=2, int/long=4 — `long` is 32-bit on ARM32; `long long`/64-bit is TODO),
 * SIGN from signed/unsigned (plain char defaults to unsigned, ARM's default); void ~ unsigned char (so
 * void* scales like char*). const/volatile/register/inline and __attribute__ are consumed and ignored.
 * `td` is set iff `typedef` appears; `sc` collects the storage-class bits (SC_EXTERN/SC_STATIC). Both are
 * out-params (may be NULL), NOT globals — a recursive declspec (struct members) would clobber a global. */
static Type *declspec(int *td, int *sc) {
	if (td) *td = 0;
	if (sc) *sc = 0;
	enum { B_NONE, B_VOID, B_CHAR, B_SHORT, B_INT, B_LONG, B_LLONG } base = B_NONE;
	int is_uns = 0, saw_signed = 0, seen = 0;
	Type *tagty = NULL;                                              /* struct/union/enum/typedef: a complete type */
	for (;;) {
		if (consume("typedef")) { if (td) *td = 1; continue; }
		if (consume("extern")) { if (sc) *sc |= SC_EXTERN; continue; }   /* file-scope: a reference, not a definition */
		if (consume("static")) { if (sc) *sc |= SC_STATIC; continue; }   /* file-local symbol (no .global) */
		if (consume("const") || consume("volatile") || consume("restrict") || consume("register") || consume("inline")) continue;
		if (consume("__attribute__")) { skip_attribute(); continue; }
		if (consume("signed"))   { saw_signed = 1; seen = 1; continue; }
		if (consume("unsigned")) { is_uns = 1;     seen = 1; continue; }
		if (consume("void"))     { base = B_VOID;  seen = 1; continue; }
		if (consume("char"))     { base = B_CHAR;  seen = 1; continue; }
		if (consume("short"))    { base = B_SHORT; seen = 1; continue; }
		if (consume("int"))      { if (base != B_SHORT && base != B_LONG && base != B_LLONG) base = B_INT; seen = 1; continue; }
		if (consume("long"))     { base = (base == B_LONG) ? B_LLONG : B_LONG; seen = 1; continue; }
		if (consume("struct") || consume("union")) { tagty = struct_decl(); seen = 1; continue; }
		if (consume("enum")) { tagty = enum_decl(); seen = 1; continue; }
		if (!seen && tk->kind == TK_IDENT && typedef_find(tk->text)) { tagty = typedef_find(tk->text); tk = tk->next; seen = 1; continue; }
		break;
	}
	if (tagty) return tagty;
	switch (base) {
	case B_VOID:  return ty_char;                                   /* void ~ unsigned char (void* scales by 1) */
	case B_CHAR:  return is_uns ? ty_char : (saw_signed ? ty_schar : ty_char);   /* plain char = unsigned (ARM) */
	case B_SHORT: return is_uns ? ty_ushort : ty_short;
	case B_LLONG: return is_uns ? ty_ullong : ty_llong;             /* long long = 64-bit (register pair) */
	default:      return is_uns ? ty_uint : ty_int;                 /* int / long (32-bit on ARM32) */
	}
}
static Node *assign(void);        /* fwd: array bounds may be a constant expression, e.g. [52 + 8*32] */
static long eval_const(Node *n);
static Type *type_suffix(Type *base) {
	if (consume("[")) {                                       /* [] (param) allowed; else a constant expr */
		int n = 0;
		if (!consume("]")) { n = (int)eval_const(assign()); expect("]"); }
		return array_of(type_suffix(base), n);                /* outer dim wraps inner */
	}
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
/* Assign every member a byte offset (and, for bitfields, a bit offset within its storage unit) and set the
 * struct's size + alignment. Little-endian bit allocation, GCC/SysV rules: a bitfield lives entirely inside
 * one naturally-aligned storage unit of its declared type; `T : 0` forces the next unit boundary; `packed`
 * removes all inter-member padding and caps the struct alignment at 1; `aligned(N)` raises it to N. */
static void layout_struct(Type *ty, int packed, int alignb) {
	int bitpos = 0, salign = 1;
	for (Member *m = ty->members; m; m = m->next) {
		int msz = m->type->size, ma = packed ? 1 : align_of(m->type);
		if (m->is_bitfield) {
			int unit = ma * 8;
			if (m->bit_width == 0) { bitpos = (bitpos + unit - 1) / unit * unit; continue; }   /* :0 -> align, no storage */
			if (!packed && (bitpos % unit) + m->bit_width > msz * 8)     /* would straddle the storage unit */
				bitpos = (bitpos + unit - 1) / unit * unit;
			m->offset = (bitpos / unit) * ma;
			m->bit_offset = bitpos - m->offset * 8;
			bitpos += m->bit_width;
		} else {
			int byte = ((bitpos + 7) / 8 + ma - 1) & ~(ma - 1);        /* next byte, aligned to the member */
			m->offset = byte;
			bitpos = (byte + msz) * 8;
		}
		if (ma > salign) salign = ma;
	}
	if (packed) salign = 1;
	if (alignb > salign) salign = alignb;
	int bytes = (bitpos + 7) / 8;
	ty->size = (bytes + salign - 1) & ~(salign - 1);
	ty->align = salign;
}

static Type *struct_decl(void) {
	int packed = 0, alignb = 0;
	while (consume("__attribute__")) parse_attribute(&packed, &alignb);   /* struct __attribute__((packed)) S */
	char tag[64] = ""; if (tk->kind == TK_IDENT) ident(tag);
	if (!is("{")) {                                          /* a reference — forward-declare an incomplete type if new */
		Type *t = tag_find(tag);
		if (!t) { t = calloc(1, sizeof *t); t->kind = TY_STRUCT; tag_add(tag, t); }   /* opaque; pointers to it still work */
		return t;
	}
	Type *ty = tag[0] ? tag_find(tag) : NULL;                /* a definition — fill an existing forward decl in place */
	if (!ty) { ty = calloc(1, sizeof *ty); ty->kind = TY_STRUCT; tag_add(tag, ty); }
	expect("{");
	/* Collect the members first (with any bitfield widths), THEN lay them out — because `packed` may be
	 * written after the closing brace (`struct {...} __packed;`, the common kernel form) and must repack. */
	Member mh = {0}, *mc = &mh;
	while (!consume("}")) {
		Type *base = declspec(NULL, NULL);
		if (consume(";")) continue;                          /* anonymous member of a nested struct/union def */
		do {
			char mname[64]; Type *mt = declarator(base, mname);
			Member *m = calloc(1, sizeof *m); strncpy(m->name, mname, 63); m->type = mt;
			if (consume(":")) { m->is_bitfield = 1; m->bit_width = (int)eval_const(assign()); }   /* type name : width */
			mc = mc->next = m;
		} while (consume(","));
		expect(";");
	}
	while (consume("__attribute__")) parse_attribute(&packed, &alignb);   /* struct {...} __attribute__((packed)) */
	ty->members = mh.next;
	layout_struct(ty, packed, alignb);
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
		if (!strcmp(name, "__builtin_va_start")) { expect("("); Node *n = node(ND_VA_START); n->lhs = assign(); expect(","); assign(); expect(")"); return n; }
		if (!strcmp(name, "__builtin_va_arg"))   { expect("("); Node *n = node(ND_VA_ARG); n->lhs = assign(); expect(","); char d[64]; n->type = declarator(declspec(NULL, NULL), d); expect(")"); return n; }
		if (!strcmp(name, "__builtin_va_end"))   { expect("("); assign(); expect(")"); return num(0); }
		if (!strcmp(name, "__builtin_unreachable")) { expect("("); expect(")"); return num(0); }   /* no-op, not a call */
		if (consume("(")) {                                  /* call: name(args) */
			Node *n = node(ND_CALL);
			/* Direct `bl name` if `name` is a function; INDIRECT (through the value) if it's a
			 * variable holding a function pointer — a param/local, or a global. n->lhs = the callee. */
			if (local_exists(name)) {
				Node *c = node(ND_VAR); strncpy(c->name, name, 63);
				c->offset = local_offset(name); c->type = local_type(name); strncpy(c->reg, local_reg(name), 7);
				n->lhs = c;
			} else {
				Gvar *gv = global_find(name);
				if (gv) { Node *c = node(ND_GVAR); strncpy(c->name, name, 63); c->type = gv->type; n->lhs = c; }
				else strncpy(n->name, name, 63);            /* a function name -> direct call */
			}
			Node argh = {0}, *ac = &argh;
			if (!is(")")) { do { ac = ac->next = assign(); } while (consume(",")); }   /* assign(), so ',' separates args */
			expect(")"); n->args = argh.next; return n;
		}
		if (local_exists(name)) { Node *n = node(ND_VAR); strncpy(n->name, name, 63); n->offset = local_offset(name); n->type = local_type(name); strncpy(n->reg, local_reg(name), 7); return n; }
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
		Node *n = node(ND_MEMBER); n->lhs = base; n->offset = m->offset; n->type = m->type;
		if (m->is_bitfield) { n->bit_width = m->bit_width; n->bit_offset = m->bit_offset; }
		return n;
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
		char d[64]; expect("("); Type *t = declarator(declspec(NULL, NULL), d); expect(")");
		Node *n = node(ND_CAST); n->lhs = unary_expr(); n->type = t; return n;
	}
	if (consume("sizeof")) {                                 /* sizeof(type) or sizeof expr -> a constant */
		if (cast_ahead()) { char d[64]; expect("("); Type *t = declarator(declspec(NULL, NULL), d); expect(")"); return num(t->size); }
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
/* Aggregate/brace initializer for a local: `{ e0, e1, ... }` -> a block of member/element assignments to
 * `dest` (an lvalue). Recurses for nested braces; a scalar with braces takes the first element. Partial
 * initializers just stop (the rest is left as-is — no zero-fill, an M1 simplification). */
static Node *init_of(Node *dest, Type *ty) {
	expect("{");
	Node blk = {0}, *c = &blk;
	if (ty->kind == TY_STRUCT) {
		for (Member *m = ty->members; m && !is("}"); m = m->next) {
			Node *dm = node(ND_MEMBER); dm->lhs = dest; dm->offset = m->offset; dm->type = m->type;
			c->next = is("{") ? init_of(dm, m->type) : unary(ND_EXPRSTMT, binary(ND_ASSIGN, dm, assign()));
			c = c->next; if (!consume(",")) break;
		}
	} else if (ty->kind == TY_ARRAY) {
		for (int i = 0; i < ty->len && !is("}"); i++) {
			Node *de = unary(ND_DEREF, new_add(dest, num(i)));   /* dest[i] */
			c->next = is("{") ? init_of(de, ty->base) : unary(ND_EXPRSTMT, binary(ND_ASSIGN, de, assign()));
			c = c->next; if (!consume(",")) break;
		}
	} else {   /* scalar in braces: {e} */
		c = c->next = unary(ND_EXPRSTMT, binary(ND_ASSIGN, dest, assign()));
		consume(",");
	}
	expect("}");
	Node *n = node(ND_BLOCK); n->body = blk.next; return n;
}

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
	if (is("__asm__") || is("asm")) {                        /* __asm__ volatile("tmpl" : outs : ins : clobbers); */
		tk = tk->next; consume("volatile"); consume("__volatile__");
		expect("("); Node *n = node(ND_ASM); strncpy(n->name, tk->text, 63); tk = tk->next;   /* template string */
		Node oh = {0}, *oc = &oh; int nouts = 0;
		if (consume(":")) while (tk->kind == TK_STR) { tk = tk->next; expect("("); oc = oc->next = assign(); expect(")"); nouts++; if (!consume(",")) break; }
		if (consume(":")) while (tk->kind == TK_STR) { tk = tk->next; expect("("); oc = oc->next = assign(); expect(")"); if (!consume(",")) break; }
		if (consume(":")) while (tk->kind == TK_STR) { tk = tk->next; if (!consume(",")) break; }   /* clobbers — ignored */
		expect(")"); expect(";");
		n->args = oh.next; n->val = nouts;                   /* operands: outputs first, then inputs; val = #outputs */
		return n;
	}
	if (consume("goto"))     { Node *n = node(ND_GOTO); ident(n->name); expect(";"); return n; }
	if (tk->kind == TK_IDENT && tk->next && tk->next->kind == TK_PUNCT && !strcmp(tk->next->text, ":")) {   /* label: */
		Node *n = node(ND_LABEL); ident(n->name); expect(":"); return n;
	}
	if (consume("return")) { Node *n = node(ND_RETURN); if (!is(";")) n->lhs = expr(); expect(";"); return n; }   /* `return;` allowed */
	if (consume("if")) { Node *n = node(ND_IF); expect("("); n->cond = expr(); expect(")"); n->then = stmt(); if (consume("else")) n->els = stmt(); return n; }
	if (consume("while")) { Node *n = node(ND_WHILE); expect("("); n->cond = expr(); expect(")"); n->body = stmt(); return n; }
	if (consume("do")) { Node *n = node(ND_DOWHILE); n->body = stmt(); expect("while"); expect("("); n->cond = expr(); expect(")"); expect(";"); return n; }
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
		int td; Type *base = declspec(&td, NULL);
		if (td) { char nm[64]; Type *ty = declarator(base, nm); add_typedef(nm, ty); expect(";"); return node(ND_BLOCK); }
		if (consume(";")) return node(ND_BLOCK);             /* type-only (e.g. a struct definition) */
		Node blk = {0}, *bc = &blk;                          /* each initializer becomes a statement in a block */
		do {
			char nm[64]; Type *ty = declarator(base, nm); int off = add_local(nm, ty);
			if (consume("__asm__")) { expect("("); strncpy(locals[nlocals - 1].reg, tk->text, 7); tk = tk->next; expect(")"); }   /* register var */
			if (consume("=")) { Node *v = node(ND_VAR); strncpy(v->name, nm, 63); v->offset = off; v->type = ty; strncpy(v->reg, local_reg(nm), 7);
				if (is("{")) bc = bc->next = init_of(v, ty);              /* aggregate initializer */
				else bc = bc->next = unary(ND_EXPRSTMT, binary(ND_ASSIGN, v, assign())); }
		} while (consume(","));
		expect(";");
		Node *n = node(ND_BLOCK); n->body = blk.next; return n;
	}
	Node *n = unary(ND_EXPRSTMT, expr()); expect(";"); return n;
}

/* ---- functions ----------------------------------------------------------------------------------- */
/* The name + return type have already been read; the cursor is at "(". Parse params + body. */
static Func *function_tail(const char *name, Type *ret) {
	Func *f = calloc(1, sizeof *f); strncpy(f->name, name, 63); f->ret_type = ret;
	nlocals = 0; local_bytes = 0;
	expect("(");
	struct { char name[64]; Type *ty; } prm[16]; int np = 0;   /* collect params, then assign offsets by kind */
	if (is("void") && !strcmp(tk->next->text, ")")) tk = tk->next;   /* (void) = no params */
	else if (!is(")")) {
		do {
			if (is(".")) { while (consume(".")) ; f->variadic = 1; break; }   /* `...` */
			char p[64]; Type *ty = declarator(declspec(NULL, NULL), p);
			if (ty->kind == TY_ARRAY) ty = pointer_to(ty->base);   /* array param decays to pointer */
			if (np < 16) { strncpy(prm[np].name, p, 63); prm[np].ty = ty; np++; }
		} while (consume(","));
	}
	expect(")");
	f->nparams = np;
	/* Assign each param to argument WORDS (64-bit = 2 words). A variadic function spills r0..r3 into a
	 * contiguous incoming-arg block, so ALL params sit at [r11, #8 + 4*word]. A normal function keeps the
	 * first 4 words in r0..r3 (spilled to negative frame slots in the prologue), later words at +8. */
	int word = 0;
	for (int i = 0; i < np; i++) {
		int nw = (prm[i].ty && prm[i].ty->size == 8) ? 2 : 1;
		if (f->variadic) {
			if (prm[i].name[0]) add_local_at(prm[i].name, prm[i].ty, 8 + 4 * word);
		} else if (word + nw <= 4) {                             /* fully in registers r{word}..r{word+nw-1} */
			int off = prm[i].name[0] ? add_local(prm[i].name, prm[i].ty) : 0;
			for (int k = 0; k < nw && off; k++) f->arg_off[word + k] = off + 4 * k;   /* spill targets */
			f->arg_regs = word + nw;
		} else if (word >= 4) {                                  /* fully on the stack */
			if (prm[i].name[0]) add_local_at(prm[i].name, prm[i].ty, 8 + 4 * (word - 4));
		} else {
			die("cc: 64-bit parameter split across registers and stack (not supported)");
		}
		word += nw;
	}
	f->nfixed_words = word;                                      /* words of fixed params, for va_start */
	while (consume("__attribute__")) skip_attribute();       /* e.g. int f(void) __attribute__((noreturn)) { … } */
	if (consume(";")) return NULL;                           /* a prototype — no body to compile */
	expect("{");
	Node h = {0}, *c = &h; while (!consume("}")) c = c->next = stmt();
	f->body = h.next;
	f->frame = (local_bytes + 7) & ~7;                       /* 8-byte aligned frame (locals+params, arrays sized) */
	return f;   /* add_type runs in a final pass (parse()), once every function's return type is recorded */
}

/* Top level: read a type + name, then dispatch — "(" means a function, anything else a global variable
 * (optionally with a constant integer initializer). `void` is only valid as a function return type. */
/* Parse a static initializer for `ty` into a flat item list (constants, &symbol addresses, zero padding),
 * following the type's layout: struct members get padding for alignment gaps + a zero tail for missing
 * fields; array elements likewise. Recurses for nested braces. */
/* Fold a constant expression (for initializers, case labels, enum values, array sizes). */
static long eval_const(Node *n) {
	switch (n->kind) {
	case ND_NUM:    return n->val;
	case ND_NEG:    return -eval_const(n->lhs);
	case ND_BITNOT: return ~eval_const(n->lhs);
	case ND_NOT:    return !eval_const(n->lhs);
	case ND_CAST:   return eval_const(n->lhs);
	case ND_ADD:    return eval_const(n->lhs) +  eval_const(n->rhs);
	case ND_SUB:    return eval_const(n->lhs) -  eval_const(n->rhs);
	case ND_MUL:    return eval_const(n->lhs) *  eval_const(n->rhs);
	case ND_DIV:    return eval_const(n->lhs) /  eval_const(n->rhs);
	case ND_MOD:    return eval_const(n->lhs) %  eval_const(n->rhs);
	case ND_BITAND: return eval_const(n->lhs) &  eval_const(n->rhs);
	case ND_BITOR:  return eval_const(n->lhs) |  eval_const(n->rhs);
	case ND_BITXOR: return eval_const(n->lhs) ^  eval_const(n->rhs);
	case ND_SHL:    return eval_const(n->lhs) << eval_const(n->rhs);
	case ND_SHR:    return eval_const(n->lhs) >> eval_const(n->rhs);
	case ND_EQ:     return eval_const(n->lhs) == eval_const(n->rhs);
	case ND_NE:     return eval_const(n->lhs) != eval_const(n->rhs);
	case ND_LT:     return eval_const(n->lhs) <  eval_const(n->rhs);
	case ND_LE:     return eval_const(n->lhs) <= eval_const(n->rhs);
	case ND_GT:     return eval_const(n->lhs) >  eval_const(n->rhs);
	case ND_GE:     return eval_const(n->lhs) >= eval_const(n->rhs);
	case ND_COND:   return eval_const(n->cond) ? eval_const(n->then) : eval_const(n->els);
	default: die("parse: not a constant expression"); return 0;
	}
}
static Init *mkinit(int kind) { Init *i = calloc(1, sizeof *i); i->kind = kind; return i; }
static Init *global_init(Type *ty) {
	if (is("{")) {
		expect("{");
		Init head = {0}, *c = &head;
		if (ty->kind == TY_STRUCT) {
			int cur = 0;
			for (Member *m = ty->members; m && !is("}"); m = m->next) {
				if (m->offset > cur) { c->next = mkinit(INIT_ZERO); c->next->size = m->offset - cur; c = c->next; }
				c->next = global_init(m->type); while (c->next) c = c->next;   /* append member's items */
				cur = m->offset + m->type->size;
				if (!consume(",")) break;
			}
			if (cur < ty->size) { c->next = mkinit(INIT_ZERO); c->next->size = ty->size - cur; c = c->next; }
		} else if (ty->kind == TY_ARRAY) {
			int i = 0;
			for (; i < ty->len && !is("}"); i++) { c->next = global_init(ty->base); while (c->next) c = c->next; if (!consume(",")) break; }
			if (i * ty->base->size < ty->size) { c->next = mkinit(INIT_ZERO); c->next->size = ty->size - i * ty->base->size; c = c->next; }
		} else { c->next = global_init(ty); while (c->next) c = c->next; }   /* scalar in braces */
		expect("}");
		return head.next;
	}
	if (consume("&")) { Init *i = mkinit(INIT_SYM); ident(i->sym); i->size = 4; return i; }   /* address of a global */
	Init *i = mkinit(INIT_CONST); i->val = eval_const(conditional()); i->size = ty->size; return i;   /* 1/2/4 -> .byte/.hword/.word */
}

/* Function-signature table: a called function's return type, so ND_CALL result nodes get the right width
 * (a 64-bit return must not be truncated). Populated for every prototype/definition, consulted by add_type. */
static struct { char name[64]; Type *ret; } func_sigs[512]; static int nfunc_sigs;
static void record_func_sig(const char *name, Type *ret) {
	for (int i = 0; i < nfunc_sigs; i++) if (!strcmp(func_sigs[i].name, name)) { func_sigs[i].ret = ret; return; }
	if (nfunc_sigs < 512) { strncpy(func_sigs[nfunc_sigs].name, name, 63); func_sigs[nfunc_sigs].ret = ret; nfunc_sigs++; }
}
Type *func_ret_type(const char *name) {
	if (name && name[0]) for (int i = 0; i < nfunc_sigs; i++) if (!strcmp(func_sigs[i].name, name)) return func_sigs[i].ret;
	return NULL;
}

Func *parse(Token *tok) {
	tk = tok;
	add_typedef("__builtin_va_list", pointer_to(ty_char));   /* va_list is a char* walking the arg block */
	Func head = {0}, *cur = &head;
	while (tk->kind != TK_EOF) {
		if (tk->kind == TK_IDENT && !strcmp(tk->text, "_Static_assert")) { tk = tk->next; skip_attribute(); consume(";"); continue; }
		int td, sc; Type *base = declspec(&td, &sc);               /* type keywords/qualifiers + storage class; struct/enum defs register */
		if (consume(";")) continue;                          /* type-only declaration, e.g. `struct P { ... };`   */
		if (td) { char nm[64]; Type *ty = declarator(base, nm); add_typedef(nm, ty); expect(";"); continue; }
		char name[64]; Type *ty = declarator(base, name);    /* *s + name + array suffix */
		if (is("(")) { record_func_sig(name, ty); Func *fn = function_tail(name, ty); if (fn) { fn->is_static = (sc & SC_STATIC) != 0; cur = cur->next = fn; } continue; }   /* NULL = prototype */
		for (;;) {                                           /* global variable(s), comma-separated */
			Gvar *g = add_global(); strncpy(g->name, name, 63); g->type = ty;
			g->is_extern = (sc & SC_EXTERN) != 0; g->is_static = (sc & SC_STATIC) != 0;
			if (consume("=")) g->init = global_init(ty);
			if (!consume(",")) break;
			ty = declarator(base, name);
		}
		expect(";");
	}
	for (Func *f = head.next; f; f = f->next)                /* type every body now that all signatures are known */
		for (Node *s = f->body; s; s = s->next) add_type(s);
	return head.next;
}
