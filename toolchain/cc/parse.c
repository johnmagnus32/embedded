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
#define MAXTYPEDEFS 16384   /* a preprocessed kernel TU has thousands of typedefs; at 256 the table overflowed
                             * silently -> a later typedef (e.g. Elf64_Sxword) wasn't recognized as a typename. */
static struct { char name[64]; Type *type; } typedefs[MAXTYPEDEFS]; static int ntypedefs;
static Type *typedef_find(const char *n) { for (int i = 0; i < ntypedefs; i++) if (!strcmp(typedefs[i].name, n)) return typedefs[i].type; return NULL; }
static void  add_typedef(const char *n, Type *t) { if (ntypedefs >= MAXTYPEDEFS) die("cc: too many typedefs (>%d) — raise MAXTYPEDEFS", MAXTYPEDEFS); strncpy(typedefs[ntypedefs].name, n, 63); typedefs[ntypedefs].type = t; ntypedefs++; }
#define MAXENUMC 16384   /* a kernel TU has thousands of enum constants (was 512 -> silently dropped) */
static struct { char name[64]; long val; } enumc[MAXENUMC]; static int nenumc;
static int   enum_find(const char *n, long *v) { for (int i = 0; i < nenumc; i++) if (!strcmp(enumc[i].name, n)) { *v = enumc[i].val; return 1; } return 0; }

static Type *struct_decl(int is_union);
static Type *enum_decl(void);
static int is_typename(void);
static Type *declarator(Type *base, char *name);
static Node *init_of(Node *dest, Type *ty);   /* aggregate brace-initializer (defined later; used by compound literals) */
static long eval_try(Node *n, int *ok);        /* non-dying constant folder (used by __builtin_constant_p) */
static void record_func_sig(const char *name, Type *ret, Type **params, int np, int variadic);
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
		if (consume("register")) { if (sc) *sc |= SC_REGISTER; continue; }   /* tracked: file-scope `register T x asm("rN")` */
		if (consume("const") || consume("volatile") || consume("restrict") || consume("inline")) continue;
		if (consume("__const__") || consume("__const") || consume("__volatile__") || consume("__restrict__") || consume("__restrict") || consume("__inline__") || consume("__inline")) continue;   /* GNU alt spellings */
		if (consume("__extension__")) continue;   /* GNU no-op prefix */
		if (consume("__attribute__")) { skip_attribute(); continue; }
		if (consume("signed") || consume("__signed__")) { saw_signed = 1; seen = 1; continue; }
		if (consume("unsigned")) { is_uns = 1;     seen = 1; continue; }
		if (consume("void"))     { base = B_VOID;  seen = 1; continue; }
		if (consume("_Bool"))    { base = B_CHAR;  is_uns = 1; seen = 1; continue; }   /* _Bool: 1-byte unsigned */
		if (consume("char"))     { base = B_CHAR;  seen = 1; continue; }
		if (consume("short"))    { base = B_SHORT; seen = 1; continue; }
		if (consume("int"))      { if (base != B_SHORT && base != B_LONG && base != B_LLONG) base = B_INT; seen = 1; continue; }
		if (consume("long"))     { base = (base == B_LONG) ? B_LLONG : B_LONG; seen = 1; continue; }
		if (consume("struct")) { tagty = struct_decl(0); seen = 1; continue; }
		if (consume("union"))  { tagty = struct_decl(1); seen = 1; continue; }
		if (consume("enum")) { tagty = enum_decl(); seen = 1; continue; }
		if (consume("typeof") || consume("__typeof__")) {   /* typeof(type) or typeof(expr) -> that type */
			expect("(");
			if (is_typename()) { char d[64]; tagty = declarator(declspec(NULL, NULL), d); }
			else { Node *e = assign(); add_type(e); tagty = e->type ? e->type : ty_int; }   /* unevaluated: type only */
			expect(")"); seen = 1; continue;
		}
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
	while (consume("*")) { base = pointer_to(base); while (consume("const") || consume("volatile") || consume("restrict") || consume("__restrict") || consume("__restrict__")) ; }   /* `char * const` */
	while (consume("__attribute__")) skip_attribute();       /* e.g. `void * __attribute__((...)) name` */
	if (consume("(")) {                                      /* grouped declarator: (*name)... = pointer ; (name)... = plain grouping (e.g. function-type typedef `T (name)(params)`) */
		int ptr = 0;
		while (consume("*")) { ptr = 1; while (consume("const") || consume("volatile") || consume("restrict") || consume("__restrict") || consume("__restrict__")) ; }
		name[0] = 0; if (tk->kind == TK_IDENT) ident(name); expect(")");
		if (is("(")) skip_attribute(); else base = type_suffix(base);   /* skip a function param list, or apply an array suffix */
		while (consume("__attribute__")) skip_attribute();              /* trailing: `void (*f)(args) __attribute__((noreturn))` */
		return ptr ? pointer_to(base) : base;
	}
	name[0] = 0; if (tk->kind == TK_IDENT) ident(name);      /* name omitted => abstract declarator */
	while (consume("__attribute__")) skip_attribute();       /* trailing: `int x __attribute__((aligned(4)))` */
	return type_suffix(base);
}

/* struct-spec = "struct" tag? ( "{" (declspec declarator ("," declarator)* ";")* "}" )?  — a named
 * definition registers the tag; a bare "struct tag" looks it up. Member offsets are assigned with each
 * member aligned to its own alignment, and the struct's size rounded up to its max member alignment. */
#define MAXTAGS 16384   /* a preprocessed kernel TU declares thousands of struct/union tags; at 64 the table
                         * overflowed silently -> a forward decl + its later definition bound to DIFFERENT
                         * Type objects, so `ptr->member` saw an empty (opaque) struct. */
static struct { char name[64]; Type *type; } struct_tags[MAXTAGS]; static int nstruct_tags;
static Type *tag_find(const char *name) { for (int i = 0; i < nstruct_tags; i++) if (!strcmp(struct_tags[i].name, name)) return struct_tags[i].type; return NULL; }
static void  tag_add(const char *name, Type *t) { if (!name[0]) return; if (nstruct_tags >= MAXTAGS) die("cc: too many struct/union tags (>%d) — raise MAXTAGS", MAXTAGS); strncpy(struct_tags[nstruct_tags].name, name, 63); struct_tags[nstruct_tags].type = t; nstruct_tags++; }
/* Assign every member a byte offset (and, for bitfields, a bit offset within its storage unit) and set the
 * struct's size + alignment. Little-endian bit allocation, GCC/SysV rules: a bitfield lives entirely inside
 * one naturally-aligned storage unit of its declared type; `T : 0` forces the next unit boundary; `packed`
 * removes all inter-member padding and caps the struct alignment at 1; `aligned(N)` raises it to N. */
static void layout_struct(Type *ty, int packed, int alignb, int is_union) {
	int bitpos = 0, salign = 1;
	if (is_union) {                         /* every member overlaps at offset 0; size = widest member */
		int maxsz = 0;
		for (Member *m = ty->members; m; m = m->next) {
			int ma = packed ? 1 : align_of(m->type);
			m->offset = 0; if (m->is_bitfield) m->bit_offset = 0;
			if (m->type->size > maxsz) maxsz = m->type->size;
			if (ma > salign) salign = ma;
		}
		if (packed) salign = 1;
		if (alignb > salign) salign = alignb;
		ty->size = (maxsz + salign - 1) & ~(salign - 1);
		ty->align = salign;
		return;
	}
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

static Type *struct_decl(int is_union) {
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
		if (consume(";")) {                                  /* no declarator: an anonymous struct/union member */
			if (base->kind == TY_STRUCT) { Member *m = calloc(1, sizeof *m); m->type = base; m->is_anon = 1; mc = mc->next = m; }
			continue;
		}
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
	layout_struct(ty, packed, alignb, is_union);
	/* Promote members of anonymous struct/union members into this type (accessible directly), at the
	 * anonymous block's offset + the sub-member's own offset. */
	Member *ph = NULL, *pt = NULL;
	for (Member *am = ty->members; am; am = am->next) if (am->is_anon)
		for (Member *sm = am->type->members; sm; sm = sm->next) {
			Member *pm = calloc(1, sizeof *pm); *pm = *sm;
			pm->offset = am->offset + sm->offset; pm->is_anon = 0; pm->next = NULL;
			if (pt) pt->next = pm; else ph = pm; pt = pm;
		}
	if (ph) { Member *t = ty->members; while (t->next) t = t->next; t->next = ph; }
	return ty;
}

/* enum [tag] { NAME [= const] , ... } — registers each constant as an int value; the type is just int. */
static Type *enum_decl(void) {
	if (tk->kind == TK_IDENT) tk = tk->next;               /* optional tag, ignored (enum == int) */
	if (consume("{")) {
		long val = 0;
		while (!is("}")) {
			char nm[64]; ident(nm);
			if (consume("=")) val = eval_const(assign());   /* any const expr: another enum constant, 1<<N, … */
			if (nenumc >= MAXENUMC) die("parse: too many enum constants (>%d) — raise MAXENUMC", MAXENUMC);
			strncpy(enumc[nenumc].name, nm, 63); enumc[nenumc].val = val; nenumc++;
			val++;
			if (!consume(",")) break;
		}
		expect("}");
	}
	return ty_int;
}

/* ---- file-scope objects: globals + string literals ----------------------------------------------- */
Gvar *globals; static Gvar *gtail; static int str_id;
/* File-scope register variables (`register T x asm("rN");`) — x aliases a hard register everywhere. */
static struct { char name[64]; char reg[8]; } gregs[16]; static int ngregs;
static const char *greg_find(const char *name) { for (int i = 0; i < ngregs; i++) if (!strcmp(gregs[i].name, name)) return gregs[i].reg; return NULL; }
static Gvar *add_global(void) { Gvar *g = calloc(1, sizeof *g); if (gtail) gtail->next = g; else globals = g; gtail = g; return g; }
static Gvar *global_find(const char *name) { for (Gvar *g = globals; g; g = g->next) if (!g->is_str && !strcmp(g->name, name)) return g; return NULL; }

/* ---- node constructors --------------------------------------------------------------------------- */
static int is_typename(void) {   /* does a declaration start at the cursor? */
	return is("int") || is("char") || is("void") || is("short") || is("long") || is("signed") || is("unsigned") || is("_Bool")
	    || is("struct") || is("union") || is("enum") || is("typedef") || is("typeof") || is("__typeof__")
	    || is("const") || is("volatile") || is("static") || is("extern") || is("register") || is("inline") || is("__attribute__")
	    || is("__signed__") || is("__const__") || is("__const") || is("__volatile__") || is("__restrict__") || is("__restrict") || is("__inline__") || is("__inline") || is("__extension__") || is("__auto_type")
	    || (tk->kind == TK_IDENT && typedef_find(tk->text));
}
static Node *node(NodeKind k) { Node *n = calloc(1, sizeof *n); n->kind = k; return n; }
static Node *binary(NodeKind k, Node *l, Node *r) { Node *n = node(k); n->lhs = l; n->rhs = r; return n; }
static Node *unary(NodeKind k, Node *e) { Node *n = node(k); n->lhs = e; return n; }
static Node *num(long v) { Node *n = node(ND_NUM); n->val = v; return n; }

/* ---- expression grammar (each returns the parsed subtree; result convention lives in gen.c) ------- */
static Node *expr(void);
static Node *assign(void);
static Node *stmt(void);
static Node *new_add(Node *l, Node *r);       /* +/- with pointer/array scaling (defined below) */
static Node *new_sub(Node *l, Node *r);

	/* C11 _Generic(ctrl, T1: e1, ..., default: eN): yield the association whose type matches ctrl's type
	 * (first match; our long==int etc. means near-identical types tie, but they resolve to the same type). */
	static int types_match(Type *a, Type *b) {
		if (!a || !b || a->kind != b->kind) return 0;
		if (a->kind == TY_PTR || a->kind == TY_ARRAY || a->kind == TY_STRUCT) return 1;   /* approx: any ptr/aggregate */
		return a->size == b->size && a->is_unsigned == b->is_unsigned;
	}
	static Node *primary(void) {
	while (consume("__extension__")) ;   /* GNU no-op prefix, e.g. __extension__ ({...}) */
	if (consume("_Generic")) {
		expect("("); Node *ctrl = assign(); add_type(ctrl);
		Node *chosen = NULL, *deflt = NULL;
		while (consume(",")) {
			if (consume("default")) { expect(":"); Node *e = assign(); deflt = e; }
			else { char d[64]; Type *t = declarator(declspec(NULL, NULL), d); expect(":"); Node *e = assign(); if (!chosen && types_match(ctrl->type, t)) chosen = e; }
		}
		expect(")");
		return chosen ? chosen : (deflt ? deflt : num(0));
	}
	if (consume("(")) {
		if (is("{")) {   /* GNU statement expression ({ stmts...; last-expr; }) — value is the last expr */
			Node *n = node(ND_STMTEXPR); n->body = stmt()->body; expect(")"); return n;
		}
		Node *n = expr(); expect(")"); return n;
	}
	if (tk->kind == TK_NUM) { Node *n = num(tk->val); tk = tk->next; return n; }
	if (tk->kind == TK_STR) {                                /* string literal -> anonymous .rodata array */
		Gvar *g = add_global(); g->is_str = 1; g->type = ty_char;
		snprintf(g->name, sizeof g->name, ".LSTR%d", str_id++);
		size_t len = 0;
		while (tk->kind == TK_STR) {                         /* adjacent string literals concatenate: "a" "b" -> "ab" */
			size_t n = strlen(tk->text);
			if (len + n >= sizeof g->str) die("parse: string literal too long (>%d) — raise Gvar.str", (int)sizeof g->str);
			memcpy(g->str + len, tk->text, n); len += n; tk = tk->next;
		}
		g->str[len] = 0;
		Node *gv = node(ND_GVAR); strncpy(gv->name, g->name, 63); gv->type = ty_char;
		return unary(ND_ADDR, gv);                           /* its value is &(first byte) : char* */
	}
	if (tk->kind == TK_IDENT) {
		char name[64]; ident(name);
		if (!strcmp(name, "__builtin_va_start")) { expect("("); Node *n = node(ND_VA_START); n->lhs = assign(); expect(","); assign(); expect(")"); return n; }
		if (!strcmp(name, "__builtin_va_arg"))   { expect("("); Node *n = node(ND_VA_ARG); n->lhs = assign(); expect(","); char d[64]; n->type = declarator(declspec(NULL, NULL), d); expect(")"); return n; }
		if (!strcmp(name, "__builtin_va_end"))   { expect("("); assign(); expect(")"); return num(0); }
		if (!strcmp(name, "__builtin_unreachable")) { expect("("); expect(")"); return num(0); }   /* no-op, not a call */
		if (!strcmp(name, "__builtin_expect"))    { expect("("); Node *e = assign(); expect(","); assign(); expect(")"); return e; }   /* value is the 1st arg; the hint is ignored */
		if (!strcmp(name, "__builtin_constant_p")) { expect("("); Node *e = assign(); expect(")"); int ok = 1; eval_try(e, &ok); return num(ok ? 1 : 0); }   /* 1 iff the arg folds to an integer constant */
		if (!strcmp(name, "__builtin_choose_expr")) {   /* compile-time ?: — pick an arm by the constant cond; the other is discarded */
			expect("("); Node *c = assign(); expect(","); Node *a = assign(); expect(","); Node *b = assign(); expect(")");
			return eval_const(c) ? a : b;
		}
		if (!strcmp(name, "__builtin_types_compatible_p")) {   /* two TYPE args -> 1 if compatible, else 0 */
			expect("("); char d[64]; Type *ta = declarator(declspec(NULL, NULL), d); expect(","); Type *tb = declarator(declspec(NULL, NULL), d); expect(")");
			return num(types_match(ta, tb) ? 1 : 0);
		}
		{ const char *rg = greg_find(name); if (rg) { Node *n = node(ND_REGVAR); strncpy(n->reg, rg, 7); n->type = ty_uint; return n; } }   /* global register variable */
		if (!strcmp(name, "__builtin_offsetof")) {   /* constant byte offset of a member designator within a type */
			expect("("); char d[64]; Type *t = declarator(declspec(NULL, NULL), d); expect(",");
			long off = 0; char mn[64]; ident(mn);
			Member *m = NULL; for (m = t->members; m; m = m->next) if (!strcmp(m->name, mn)) break;
			if (!m) die("parse: __builtin_offsetof: no member '%s'", mn);
			off = m->offset; t = m->type;
			for (;;) {
				if (consume(".")) { ident(mn); for (m = t->members; m; m = m->next) if (!strcmp(m->name, mn)) break; if (!m) die("parse: __builtin_offsetof: no member '%s'", mn); off += m->offset; t = m->type; }
				else if (consume("[")) { long idx = eval_const(assign()); expect("]"); if (t->base) off += idx * t->base->size, t = t->base; }
				else break;
			}
			expect(")"); return num(off);
		}
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
		/* Otherwise-unresolved identifier = an external symbol (usually a function). Treat it as a function
		 * designator (its address); the linker resolves it. Valid code only reaches here for externals. */
		{ Node *gv = node(ND_GVAR); strncpy(gv->name, name, 63); gv->type = ty_char; return unary(ND_ADDR, gv); }
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
		else if (consume("(")) {   /* call on an arbitrary expr: _Generic(...)(args), (fp)(args), f(x)(y) — indirect via lhs */
			Node *c = node(ND_CALL); c->lhs = n;
			Node argh = {0}, *ac = &argh;
			if (!is(")")) { do { ac = ac->next = assign(); } while (consume(",")); }
			expect(")"); c->args = argh.next; n = c;
		}
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
		if (is("{")) {   /* compound literal (type){init}: an anonymous initialized object, yields its lvalue */
			static int cl_seq;
			char nm[32]; snprintf(nm, sizeof nm, ".Lcl%d", cl_seq++);
			int off = add_local(nm, t);
			Node *v = node(ND_VAR); strncpy(v->name, nm, 63); v->offset = off; v->type = t;
			Node *initb = init_of(v, t);                     /* block of member/element assignments to v */
			Node *y = node(ND_VAR); strncpy(y->name, nm, 63); y->offset = off; y->type = t;
			initb->next = unary(ND_EXPRSTMT, y);             /* ...then the statement-expression yields v */
			Node *se = node(ND_STMTEXPR); se->body = initb; se->type = t; return se;
		}
		Node *n = node(ND_CAST); n->lhs = unary_expr(); n->type = t; return n;
	}
	if (consume("sizeof")) {                                 /* sizeof(type) or sizeof expr -> a constant */
		if (cast_ahead()) { char d[64]; expect("("); Type *t = declarator(declspec(NULL, NULL), d); expect(")"); return num(t->size); }
		Node *e = unary_expr(); add_type(e); return num(e->type ? e->type->size : 4);
	}
	if (consume("__alignof__") || consume("__alignof") || consume("_Alignof") || consume("alignof")) {   /* __alignof__(type|expr) -> a constant */
		if (cast_ahead()) { char d[64]; expect("("); Type *t = declarator(declspec(NULL, NULL), d); expect(")"); return num(align_of(t)); }
		Node *e = unary_expr(); add_type(e); return num(e->type ? align_of(e->type) : 4);
	}
	if (consume("++")) { Node *x = unary_expr(); return binary(ND_ASSIGN, x, new_add(x, num(1))); }   /* ++x */
	if (consume("--")) { Node *x = unary_expr(); return binary(ND_ASSIGN, x, new_sub(x, num(1))); }   /* --x */
	if (consume("&&")) { Node *n = node(ND_LABELADDR); ident(n->name); return n; }   /* &&label : GNU address-of-label */
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
	Node *n = node(ND_COND); n->cond = c;
	if (is(":")) n->then = c;                    /* GNU `a ?: b` == `a ? a : b` (a re-evaluated; fine for side-effect-free) */
	else n->then = expr();
	expect(":"); n->els = conditional(); return n; }
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
		Member *m = ty->members;
		while (!is("}")) {
			if (consume(".")) {                              /* designated: .field = value */
				char mn[64]; ident(mn); consume("=");
				for (m = ty->members; m; m = m->next) if (!strcmp(m->name, mn)) break;
				if (!m) die("parse: struct has no member '%s'", mn);
			}
			if (!m) break;
			Node *dm = node(ND_MEMBER); dm->lhs = dest; dm->offset = m->offset; dm->type = m->type;
			if (m->is_bitfield) { dm->bit_width = m->bit_width; dm->bit_offset = m->bit_offset; }
			c->next = is("{") ? init_of(dm, m->type) : unary(ND_EXPRSTMT, binary(ND_ASSIGN, dm, assign()));
			c = c->next; m = m->next; if (!consume(",")) break;
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
	if (consume(";")) return node(ND_BLOCK);                  /* empty statement (e.g. `while (...) ;`) */
	if (consume("switch")) {                                 /* switch (e) body ; cases attach to it */
		Node *n = node(ND_SWITCH); expect("("); n->cond = expr(); expect(")");
		Node *save = cur_switch; cur_switch = n; n->then = stmt(); cur_switch = save;
		return n;
	}
	if (consume("case")) {                                   /* case CONST: */
		if (!cur_switch) die("parse: 'case' outside switch");
		Node *c = conditional(); if (c->kind != ND_NUM) die("parse: case label must be a constant (line %d)", tk->line);
		Node *n = node(ND_CASE); n->val = c->val;
		if (consume("...")) { Node *hi = conditional(); if (hi->kind != ND_NUM) die("parse: case range end must be a constant (line %d)", tk->line); n->val2 = hi->val; n->is_range = 1; }   /* GCC `case lo ... hi:` */
		expect(":");
		n->case_next = cur_switch->case_list; cur_switch->case_list = n;
		return n;
	}
	if (consume("default")) { if (!cur_switch) die("parse: 'default' outside switch"); expect(":");
		Node *n = node(ND_CASE); n->is_default = 1; n->case_next = cur_switch->case_list; cur_switch->case_list = n; return n; }
	if (consume("break"))    { expect(";"); return node(ND_BREAK); }
	if (consume("continue")) { expect(";"); return node(ND_CONTINUE); }
	if (is("__asm__") || is("asm")) {                        /* __asm__ volatile("tmpl" : outs : ins : clobbers); */
		tk = tk->next; consume("volatile"); consume("__volatile__"); consume("goto");
		expect("("); Node *n = node(ND_ASM);
		char buf[1024]; size_t bl = 0; buf[0] = 0;           /* template: concatenate adjacent string literals */
		while (tk->kind == TK_STR) { size_t l = strlen(tk->text); if (bl + l < sizeof buf - 1) { memcpy(buf + bl, tk->text, l); bl += l; buf[bl] = 0; } tk = tk->next; }
		n->asm_tmpl = malloc(bl + 1); memcpy(n->asm_tmpl, buf, bl + 1);
		Node oh = {0}, *oc = &oh; int nouts = 0;
		if (consume(":")) while (tk->kind == TK_STR) {       /* outputs: "constraint"(lvalue) */
			char c[8]; strncpy(c, tk->text, 7); c[7] = 0; tk = tk->next;
			expect("("); Node *op = assign(); expect(")"); strncpy(op->cons, c, 7);
			oc = oc->next = op; nouts++; if (!consume(",")) break;
		}
		if (consume(":")) while (tk->kind == TK_STR) {       /* inputs: "constraint"(expr) */
			char c[8]; strncpy(c, tk->text, 7); c[7] = 0; tk = tk->next;
			expect("("); Node *op = assign(); expect(")"); strncpy(op->cons, c, 7);
			if (strchr(op->cons, 'i')) op->val = eval_const(op);   /* immediate: fold now, substitute the constant */
			oc = oc->next = op; if (!consume(",")) break;
		}
		if (consume(":")) while (tk->kind == TK_STR) { tk = tk->next; if (!consume(",")) break; }   /* clobbers — ignored (we never keep values in caller-saved regs across asm) */
		expect(")"); expect(";");
		n->args = oh.next; n->val = nouts;                   /* operands: outputs first, then inputs; val = #outputs */
		return n;
	}
	if (consume("__label__")) {   /* GNU local-label declaration: `__label__ a, b;` — labels work regardless, so skip */
		do { if (tk->kind == TK_IDENT) tk = tk->next; } while (consume(","));
		expect(";"); return node(ND_BLOCK);
	}
	if (tk->kind == TK_IDENT && !strcmp(tk->text, "_Static_assert")) {   /* block-scope _Static_assert (e.g. in container_of's stmt-expr) — skip */
		tk = tk->next; skip_attribute(); consume(";"); return node(ND_BLOCK);
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
	if (is("__auto_type")) {   /* GNU __auto_type: the local's type is inferred from its initializer (kernel min/max) */
		tk = tk->next;
		Node blk = {0}, *bc = &blk;
		do {
			char nm[64]; ident(nm); expect("=");
			Node *init = assign(); add_type(init);
			Type *ty = init->type ? init->type : ty_int;
			int off = add_local(nm, ty);
			Node *v = node(ND_VAR); strncpy(v->name, nm, 63); v->offset = off; v->type = ty; strncpy(v->reg, local_reg(nm), 7);
			bc = bc->next = unary(ND_EXPRSTMT, binary(ND_ASSIGN, v, init));
		} while (consume(","));
		expect(";");
		Node *n = node(ND_BLOCK); n->body = blk.next; return n;
	}
	if (is_typename()) {                                     /* local declaration(s): `T a, b = e, c;` */
		int td; Type *base = declspec(&td, NULL);
		if (td) { char nm[64]; Type *ty = declarator(base, nm);
			if (is("(")) { int d = 1; tk = tk->next; while (d && tk->kind != TK_EOF) { if (is("(")) d++; else if (is(")")) d--; tk = tk->next; } }   /* function-type typedef `typedef R name(params)` — skip params, name aliases the return type (used via pointer) */
			add_typedef(nm, ty); expect(";"); return node(ND_BLOCK); }
		if (consume(";")) return node(ND_BLOCK);             /* type-only (e.g. a struct definition) */
		Node blk = {0}, *bc = &blk;                          /* each initializer becomes a statement in a block */
		do {
			char nm[64]; Type *ty = declarator(base, nm);
			if (is("(")) {   /* local function prototype `T name(params);` — record it, no local variable */
				record_func_sig(nm, ty, 0, 0, 1);   /* params unknown -> callers fall back to arg types */
				int d = 0; do { if (is("(")) d++; else if (is(")")) d--; tk = tk->next; } while (d && tk->kind != TK_EOF);
				while (consume("__attribute__")) skip_attribute();   /* trailing: `void h(void) __attribute__((error("...")))` */
				continue;
			}
			int off = add_local(nm, ty);
			if (consume("__asm__") || consume("asm")) { expect("("); strncpy(locals[nlocals - 1].reg, tk->text, 7); tk = tk->next; expect(")"); }   /* register var (both spellings) */
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
			if (consume("...")) { f->variadic = 1; break; }   /* `...` */
			char p[64]; Type *ty = declarator(declspec(NULL, NULL), p);
			if (ty->kind == TY_ARRAY) ty = pointer_to(ty->base);   /* array param decays to pointer */
			if (np >= 16) die("parse: too many function parameters (>16) — raise prm[]");
			strncpy(prm[np].name, p, 63); prm[np].ty = ty; np++;
		} while (consume(","));
	}
	expect(")");
	f->nparams = np;
	{ Type *pts[16]; for (int i = 0; i < np && i < 16; i++) pts[i] = prm[i].ty; record_func_sig(name, ret, pts, np, f->variadic); }   /* publish the signature for callers */
	/* Bind params per AAPCS (64-bit args are even-aligned, may skip a register / pad the stack). A variadic
	 * function spills r0..r3 into a contiguous incoming block, so ALL its params sit at [r11, #8 + 4*word];
	 * a normal function keeps register params in r0..r3 (spilled to negative frame slots in the prologue)
	 * and stack params at [r11, #8 + 4*stackword]. */
	if (f->variadic) {
		int w = 0;
		for (int i = 0; i < np; i++) {
			int nw = (prm[i].ty && prm[i].ty->size == 8) ? 2 : 1;
			if (nw == 2) w = (w + 1) & ~1;                       /* 64-bit even-aligned */
			if (prm[i].name[0]) add_local_at(prm[i].name, prm[i].ty, 8 + 4 * w);
			w += nw;
		}
		f->nfixed_words = w;                                     /* where varargs begin, for va_start */
	} else {
		int is64a[16], onstk[16], word[16];
		for (int i = 0; i < np; i++) is64a[i] = (prm[i].ty && prm[i].ty->size == 8);
		aapcs_layout(is64a, np, onstk, word);
		for (int i = 0; i < np; i++) {
			int nw = is64a[i] ? 2 : 1;
			if (!onstk[i]) {                                     /* register param: spill r{word} to a frame slot */
				int off = prm[i].name[0] ? add_local(prm[i].name, prm[i].ty) : 0;
				for (int k = 0; k < nw && off; k++) f->arg_off[word[i] + k] = off + 4 * k;
				if (word[i] + nw > f->arg_regs) f->arg_regs = word[i] + nw;
			} else if (prm[i].name[0]) {                         /* stack param */
				add_local_at(prm[i].name, prm[i].ty, 8 + word[i] * 4);
			}
		}
	}
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
static long clz_bits(unsigned long long v, int bits) { int c = 0; for (int i = bits - 1; i >= 0; i--) { if (v & (1ULL << i)) break; c++; } return c; }
static long ctz_bits(unsigned long long v, int bits) { if (!v) return bits; int c = 0; while (c < bits && !((v >> c) & 1)) c++; return c; }
/* Non-dying constant folder: sets *ok=0 if `n` is not an integer constant expression (so __builtin_constant_p
 * can probe without aborting). eval_const() is the strict wrapper that die()s on failure. */
static long eval_try(Node *n, int *ok) {
	switch (n->kind) {
	case ND_NUM:    return n->val;
	case ND_NEG:    return -eval_try(n->lhs, ok);
	case ND_BITNOT: return ~eval_try(n->lhs, ok);
	case ND_NOT:    return !eval_try(n->lhs, ok);
	case ND_CAST:   return eval_try(n->lhs, ok);
	case ND_ADD:    return eval_try(n->lhs, ok) +  eval_try(n->rhs, ok);
	case ND_SUB:    return eval_try(n->lhs, ok) -  eval_try(n->rhs, ok);
	case ND_MUL:    return eval_try(n->lhs, ok) *  eval_try(n->rhs, ok);
	case ND_DIV:    { long d = eval_try(n->rhs, ok); return d ? eval_try(n->lhs, ok) / d : (eval_try(n->lhs, ok), 0); }
	case ND_MOD:    { long d = eval_try(n->rhs, ok); return d ? eval_try(n->lhs, ok) % d : (eval_try(n->lhs, ok), 0); }
	case ND_BITAND: return eval_try(n->lhs, ok) &  eval_try(n->rhs, ok);
	case ND_BITOR:  return eval_try(n->lhs, ok) |  eval_try(n->rhs, ok);
	case ND_BITXOR: return eval_try(n->lhs, ok) ^  eval_try(n->rhs, ok);
	case ND_SHL:    return eval_try(n->lhs, ok) << eval_try(n->rhs, ok);
	case ND_SHR:    return eval_try(n->lhs, ok) >> eval_try(n->rhs, ok);
	case ND_EQ:     return eval_try(n->lhs, ok) == eval_try(n->rhs, ok);
	case ND_NE:     return eval_try(n->lhs, ok) != eval_try(n->rhs, ok);
	case ND_LT:     return eval_try(n->lhs, ok) <  eval_try(n->rhs, ok);
	case ND_LE:     return eval_try(n->lhs, ok) <= eval_try(n->rhs, ok);
	case ND_GT:     return eval_try(n->lhs, ok) >  eval_try(n->rhs, ok);
	case ND_GE:     return eval_try(n->lhs, ok) >= eval_try(n->rhs, ok);
	case ND_AND:    return eval_try(n->lhs, ok) && eval_try(n->rhs, ok);
	case ND_OR:     return eval_try(n->lhs, ok) || eval_try(n->rhs, ok);
	case ND_COND:   return eval_try(n->cond, ok) ? eval_try(n->then, ok) : eval_try(n->els, ok);
	case ND_CALL:   /* fold the __attribute__((const)) bit-count builtins over a constant argument */
		if (n->name[0] && n->args) {
			long a = eval_try(n->args, ok);
			if (!strcmp(n->name, "__builtin_clz"))    return clz_bits((unsigned int)a, 32);
			if (!strcmp(n->name, "__builtin_clzll") || !strcmp(n->name, "__builtin_clzl")) return clz_bits((unsigned long long)a, 64);
			if (!strcmp(n->name, "__builtin_ctz"))    return ctz_bits((unsigned int)a, 32);
			if (!strcmp(n->name, "__builtin_ctzll") || !strcmp(n->name, "__builtin_ctzl")) return ctz_bits((unsigned long long)a, 64);
			if (!strcmp(n->name, "__builtin_ffs"))    return a ? ctz_bits((unsigned int)a, 32) + 1 : 0;
			if (!strcmp(n->name, "__builtin_ffsll"))  return a ? ctz_bits((unsigned long long)a, 64) + 1 : 0;
		}
		*ok = 0; return 0;
	default: *ok = 0; return 0;
	}
}
static long eval_const(Node *n) {
	int ok = 1; long v = eval_try(n, &ok);
	if (!ok) die("parse: not a constant expression (node %d, near line %d)", n->kind, tk->line);
	return v;
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
			int cnt = 0;
			while (!is("}") && (ty->len == 0 || cnt < ty->len)) { c->next = global_init(ty->base); while (c->next) c = c->next; cnt++; if (!consume(",")) break; }
			if (ty->len == 0) { ty->len = cnt; ty->size = cnt * ty->base->size; }   /* unsized `[]`: length from initializer count */
			else if (cnt * ty->base->size < ty->size) { c->next = mkinit(INIT_ZERO); c->next->size = ty->size - cnt * ty->base->size; c = c->next; }
		} else { c->next = global_init(ty); while (c->next) c = c->next; }   /* scalar in braces */
		expect("}");
		return head.next;
	}
	/* Scalar: an address constant (`&sym`, or `(cast)&sym`, or a bare function name) emits the symbol's
	 * address; otherwise fold an integer constant. Peel casts + address-of down to the underlying symbol. */
	Node *e = conditional(), *p = e;
	while (p && (p->kind == ND_CAST || p->kind == ND_ADDR)) p = p->lhs;
	if (p && (p->kind == ND_GVAR || p->kind == ND_VAR)) { Init *i = mkinit(INIT_SYM); strncpy(i->sym, p->name, 63); i->size = 4; return i; }
	Init *i = mkinit(INIT_CONST); i->val = eval_const(e); i->size = ty->size; return i;   /* 1/2/4 -> .byte/.hword/.word */
}

/* Function-signature table: a callee's return + parameter types. The return type gives ND_CALL result
 * nodes the right width; the parameter types let a caller place/widen each argument per AAPCS (a 64-bit
 * param needs its arg in an even register pair, and an int arg to a 64-bit param must be widened).
 * Populated for every prototype/definition. */
#define MAXFUNCSIG 32768   /* a preprocessed kernel TU declares thousands of functions (was 512 -> silently dropped) */
static struct { char name[64]; Type *ret; Type *params[16]; int nparams; int variadic; } func_sigs[MAXFUNCSIG]; static int nfunc_sigs;
static void record_func_sig(const char *name, Type *ret, Type **params, int np, int variadic) {
	int idx = -1;
	for (int i = 0; i < nfunc_sigs; i++) if (!strcmp(func_sigs[i].name, name)) { idx = i; break; }
	if (idx < 0) { if (nfunc_sigs >= MAXFUNCSIG) die("cc: too many function signatures (>%d) — raise MAXFUNCSIG", MAXFUNCSIG); idx = nfunc_sigs++; strncpy(func_sigs[idx].name, name, 63); }
	func_sigs[idx].ret = ret; func_sigs[idx].variadic = variadic;
	func_sigs[idx].nparams = np < 16 ? np : 16;
	for (int i = 0; i < func_sigs[idx].nparams; i++) func_sigs[idx].params[i] = params[i];
}
Type *func_ret_type(const char *name) {
	if (name && name[0]) for (int i = 0; i < nfunc_sigs; i++) if (!strcmp(func_sigs[i].name, name)) return func_sigs[i].ret;
	return NULL;
}
Type *func_param_type(const char *name, int i) {
	if (name && name[0]) for (int k = 0; k < nfunc_sigs; k++) if (!strcmp(func_sigs[k].name, name))
		return (i < func_sigs[k].nparams) ? func_sigs[k].params[i] : NULL;   /* NULL => unknown or a vararg */
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
		if (td) { char nm[64]; Type *ty = declarator(base, nm);
			if (is("(")) { int d = 1; tk = tk->next; while (d && tk->kind != TK_EOF) { if (is("(")) d++; else if (is(")")) d--; tk = tk->next; } }   /* function-type typedef `typedef R name(params)` — skip params, name aliases the return type (used via pointer) */
			add_typedef(nm, ty); expect(";"); continue; }
		char name[64]; Type *ty = declarator(base, name);    /* *s + name + array suffix */
		if (is("(")) { Func *fn = function_tail(name, ty); if (fn) { fn->is_static = (sc & SC_STATIC) != 0; cur = cur->next = fn; } continue; }   /* records its own signature; NULL = prototype */
		if (consume("asm") || consume("__asm__")) {          /* `register T x asm("rN")` (global reg var) or an asm rename */
			expect("("); char s[8]; strncpy(s, tk->text, 7); s[7] = 0; if (tk->kind == TK_STR) tk = tk->next; expect(")");
			if ((sc & SC_REGISTER) && ngregs < 16) { strncpy(gregs[ngregs].name, name, 63); strncpy(gregs[ngregs].reg, s, 7); ngregs++; expect(";"); continue; }
			/* else: an asm symbol rename on a normal global — ignore the name, fall through as an ordinary global */
		}
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
