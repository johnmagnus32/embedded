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
#include <ctype.h>
#include "cc.h"
#include "strmap.h"

static Token *tk;                                  /* the parse cursor */
static Node *cur_switch;                           /* innermost switch, so case/default can attach to it */
static char cur_func_name[64];                     /* name of the function being parsed, for `__func__` */
static Type *cur_fn_ret;                           /* ...and its return type */
typedef struct { Token *params; Type *ret; } InlineDef;   /* an always_inline function expanded at its calls (see inline_expand) */
static StrMap inline_defs;
static int decl_sc_inline_extern;                  /* the function being defined is `extern inline` (GNU: inline-only with gnu_inline) */
typedef struct InlineCtx { const char *name; Type *ret; Node *retvar; char end[64]; Node **pack; int npack; int id; struct InlineCtx *up; } InlineCtx;
static InlineCtx *inline_ctx;                      /* the expansion being parsed (innermost) */
static int saw_va_pack;                            /* the function being parsed uses __builtin_va_arg_pack */
static Node *inline_expand(const char *name, InlineDef *def, Node *args);
static Node *call_args(void);
static int stmtexpr_body;                          /* the next block is a ({...})'s: its last expression is the value */
static int unevaluated;                            /* parsing sizeof's operand: only its type matters (no objects made) */
static int in_proto;                               /* parsing a prototype's parameter types (their VLA bounds are never evaluated) */
static int in_params;                              /* parsing any parameter list (C99 `[static N]`, `[const]`, `[*]` allowed) */
static int fn_depth;                               /* function nesting while parsing: 0 file scope, 1 a function, 2+ nested */
/* __label__ renames, innermost last; owner/depth: the function that declared it (a nested function's goto to it is non-local) */
static struct { char from[64], to[64], owner[64]; int depth; } lscope[512]; static int nlscope, lscope_seq;
static int inline_label_id = -1;                   /* in an always_inline expansion: its labels get this suffix (apart per call) */
static int map_label(char *name) {
	for (int i = nlscope - 1; i >= 0; i--) if (!strcmp(lscope[i].from, name)) { strcpy(name, lscope[i].to); return i; }
	if (inline_label_id >= 0) { char t[64]; snprintf(t, sizeof t, "%.48s.inl%d", name, inline_label_id); strcpy(name, t); }
	return -1;
}

/* ---- token helpers ------------------------------------------------------------------------------- */
static int is(const char *s)     { return (tk->kind == TK_PUNCT || tk->kind == TK_KW) && !strcmp(tk->text, s); }
static int consume(const char *s){ if (is(s)) { tk = tk->next; return 1; } return 0; }
static void expect(const char *s){ if (!consume(s)) die("parse: expected '%s' but got '%s' (line %d)", s, tk->text, tk->line); }
static void ident(char *out)     { if (tk->kind != TK_IDENT) die("parse: expected identifier, got '%s' (line %d)", tk->text, tk->line);
                                   strncpy(out, tk->text, 63); out[63] = 0; tk = tk->next; }

/* ---- scopes (C11 6.2.1) ------------------------------------------------------------------------- */
/* Each block (and each function, its parameters and body together) is a scope with two name spaces:
 * ORDINARY identifiers — local objects, typedef names, enum constants — and TAGS (struct/union/enum). A
 * lookup walks from the innermost scope out to file scope, so an inner declaration shadows an outer one and
 * ends with its block. File-scope objects and functions are found in the globals and signature tables, under
 * every scope's ordinary identifiers (a local, a typedef or an enum constant hides a global of the name). */
enum { ID_LOCAL, ID_TYPEDEF, ID_ENUMC, ID_NESTFN };
struct Tramp;
typedef struct { int kind; int local; Type *type; long val; int depth; char *fname; struct Tramp *tramp; } Ident;   /* local: its locals[] index;
                                                                                                * depth: the function it belongs to (a local, a nested
                                                                                                * function's definer); fname: a nested function's symbol;
                                                                                                * tramp: its trampoline, once its address is taken */
typedef struct Scope { StrMap ids, tags; struct Scope *up; } Scope;
static Scope file_scope, *scope = &file_scope;
static void scope_push(void) { Scope *sc = calloc(1, sizeof *sc); sc->up = scope; scope = sc; }
static void scope_pop(void) { Scope *sc = scope; scope = sc->up; strmap_clear(&sc->ids); strmap_clear(&sc->tags); free(sc); }
static Ident *ident_find(const char *name) {
	for (Scope *sc = scope; sc; sc = sc->up) { Ident *d = strmap_get(&sc->ids, name); if (d) return d; }
	return NULL;
}
static Ident *ident_add(const char *name, int kind) {   /* in the current scope (a same-scope redeclaration replaces) */
	Ident *d = calloc(1, sizeof *d); d->kind = kind;
	strmap_put(&scope->ids, strdup(name), d);
	return d;
}

/* ---- locals (per function) ----------------------------------------------------------------------- */
/* gname: a block-scope `static`/`extern` name bound to a GLOBAL symbol (no frame slot). */
static struct Local { char name[64]; int offset; Type *type; char reg[8]; char gname[64]; int vla; } *locals;
static int nlocals, local_bytes, locals_cap;   /* local_bytes = total frame bytes used by locals+params so far */
static void locals_room(void) { if (nlocals == locals_cap) locals = realloc(locals, (locals_cap = locals_cap ? 2 * locals_cap : 256) * sizeof *locals); }
/* A block restores nlocals at its `}` (its names end with its scope) but never reclaims its frame space. */
static int local_index(const char *name) { Ident *d = ident_find(name); return d && d->kind == ID_LOCAL ? d->local : -1; }
static const char *local_reg(const char *name) { int i = local_index(name); return i >= 0 ? locals[i].reg : ""; }
static int local_exists(const char *name) { return local_index(name) >= 0; }
static Node *node(NodeKind kind);
static Node *binary(NodeKind k, Node *l, Node *r);
static Node *unary(NodeKind k, Node *l);
static Node *tnum(long long v, Type *t);
static Node *cast_to(Type *t, Node *e);
static Node *new_add(Node *l, Node *r);
static Node *num(long v);
static Init *global_init(Type *ty);
static Gvar *global_find(const char *name);
static void tls_mark(Node *n, Gvar *g) { if (g && g->is_tls) { n->tls = 1; n->tls_local = !g->is_extern; } }
static Node *local_ref(const char *name) {   /* the node for a block-scope name: its frame slot, or its global */
	Ident *d = ident_find(name); int i = d && d->kind == ID_LOCAL ? d->local : -1;
	if (i < 0) die("parse: internal: no local '%s'", name);
	Node *n = node(locals[i].gname[0] ? ND_GVAR : ND_VAR); n->type = locals[i].type;
	if (locals[i].gname[0]) { strncpy(n->name, locals[i].gname, 63); tls_mark(n, global_find(locals[i].gname)); return n; }
	strncpy(n->name, name, 63); n->offset = locals[i].offset; strncpy(n->reg, locals[i].reg, 7); n->vla_obj = locals[i].vla;
	if (d->depth < fn_depth) {   /* an enclosing function's: in its frame, reached through the static chain */
		if (n->reg[0]) die("parse: nested function uses '%s', a register variable of the enclosing function (line %d)", name, tk->line);
		n->chain = fn_depth - d->depth;
	}
	return n;
}
/* A frame slot for a local (or, nameless, a temporary) aligned to `align` (at least 4): r11 is 8-aligned, so up to 8
 * holds; more is the declaration's to arrange (at runtime). The offset points at the object's first (lowest) byte. */
static int add_local_aligned(const char *name, Type *ty, int align) {
	if (align < 4) align = 4;
	if (align > 8) die("parse: internal: a %d-aligned frame slot", align);
	local_bytes = (local_bytes + ty->size + align - 1) & ~(align - 1);
	int off = -local_bytes;
	if (!name[0]) return off;             /* a temporary: no name to bind */
	locals_room();
	locals[nlocals].gname[0] = locals[nlocals].reg[0] = 0; locals[nlocals].vla = 0;
	strncpy(locals[nlocals].name, name, 63); locals[nlocals].offset = off; locals[nlocals].type = ty;
	Ident *d = ident_add(name, ID_LOCAL); d->local = nlocals; d->depth = fn_depth;
	nlocals++;
	return off;
}
static int add_local(const char *name, Type *ty) { int a = align_of(ty); return add_local_aligned(name, ty, a > 8 ? 8 : a); }
/* Bind a name to an explicit offset without allocating frame space — for params 5+ that live in the
 * CALLER's frame (above our saved r11/lr), at [r11, #8 + 4*(i-4)]. */
static void add_local_at(const char *name, Type *ty, int off) {
	locals_room();
	locals[nlocals].gname[0] = locals[nlocals].reg[0] = 0; locals[nlocals].vla = 0; strncpy(locals[nlocals].name, name, 63); locals[nlocals].offset = off; locals[nlocals].type = ty;
	if (name[0]) { Ident *d = ident_add(name, ID_LOCAL); d->local = nlocals; d->depth = fn_depth; }
	nlocals++;
}

/* ---- typedef names + enum constants (both resolved at parse time, scoped like any identifier) ----- */
static Type *typedef_find(const char *n) { Ident *d = ident_find(n); return d && d->kind == ID_TYPEDEF ? d->type : NULL; }
static void  add_typedef(const char *n, Type *t) { ident_add(n, ID_TYPEDEF)->type = t; }
static int   enum_find(const char *n, long *v) { Ident *d = ident_find(n); if (!d || d->kind != ID_ENUMC) return 0; *v = d->val; return 1; }

static Type *struct_decl(int is_union);
static Type *enum_decl(void);
static int is_typename(void);
static Type *declarator(Type *base, char *name);
static Node *init_of(Node *dest, Type *ty);   /* aggregate brace-initializer (defined later; used by compound literals) */
typedef struct InitPlace InitPlace;
static InitPlace *init_places(Type *ty);
static void typedef_decl(Type *base, Attr battr);
static int proto_params(Type ***pts, int *np);
static int str_decode(const char *s, unsigned char *out, int cap);

/* One shared initializer traversal (like a real compiler's InitListChecker): parse the initializer syntax
 * ONCE into a neutral list of scalar leaf placements, then lower to either a .data byte image (globals) or
 * a block of runtime stores (locals). Kills the old global_init/init_of fork that drifted in capability. */
struct InitPlace { int off; Type *ty; Node *expr; int bit_width, bit_offset, sso; struct InitPlace *next; };   /* sso: in a
                                                                                                             * reverse-storage-order struct */
static int init_sso;                                     /* parse_init: the struct whose members are being placed is reversed */
static int   parse_init(Type *ty, int base, InitPlace **tail);
static InitPlace *pi_append(InitPlace **tail, int off, Type *ty, Node *expr, int bw, int bo);
static Init *lower_global(InitPlace *places, int total);
static Node *lower_local(Node *dest, InitPlace *places, int total);
static long eval_try(Node *n, int *ok);        /* non-dying constant folder (used by __builtin_constant_p) */
static double eval_fp(Node *n, int *ok);
static void record_func_sig(const char *name, Type *ret, Type **params, int np, int variadic);
static Node *nested_address(Ident *nf);
static Type *fn_type_of(const char *name);
static int func_unprototyped(const char *name);
static void sig_attrs(const char *name, const Attr *a);
static const Attr *decl_attrs_of(const char *name);
static int  sig_align(const char *name, int align);
static int  decl_align(Node *e);
/* Declaration attributes collected since the last reset; declarations snapshot/reset it (see Attr). */
static Attr decl_attr;
static long eval_const(Node *n); static Node *assign(void);
/* Names declared weak on a PROTOTYPE (`void f(void) __weak;`): the later definition is weak too (GCC). */
static StrMap weak_names;
int name_is_weak(const char *name) { return strmap_get(&weak_names, name) != NULL; }
static unsigned char *strs_bytes(size_t *len);
static unsigned *strs_wide(int *len);
/* Asm labels: `T x __asm__("sym")` names x's symbol sym (musl's time64 redirects: `__typeof__(stat) stat
 * __asm__("__stat_time64")`). The AST keeps C names; codegen prints asm_name() wherever it emits a symbol, so a label
 * given after a first use still holds, as in GCC. Two different labels for one name are an error. */
static StrMap asm_labels, alias_targets;
int is_alias_target(const char *name) { return strmap_get(&alias_targets, name) != NULL; }
const char *asm_name(const char *name) { const char *a = strmap_get(&asm_labels, name); return a ? a : name; }
static void asm_label_for(const char *name) {   /* `asm ( "string"... )`, the `asm` consumed */
	expect("("); char buf[128]; size_t bl = 0; buf[0] = 0;
	if (tk->kind != TK_STR) die("parse: an asm label needs a string (line %d)", tk->line);
	while (tk->kind == TK_STR) { size_t n = strlen(tk->sval); if (bl + n >= sizeof buf) die("parse: asm label too long (line %d)", tk->line); memcpy(buf + bl, tk->sval, n); bl += n; buf[bl] = 0; tk = tk->next; }
	expect(")");
	const char *old = strmap_get(&asm_labels, name);
	if (old && strcmp(old, buf)) die("parse: '%s' relabeled: asm(\"%s\") after asm(\"%s\") (line %d)", name, buf, old, tk->line);
	if (!buf[0]) die("parse: an empty asm label for '%s' (line %d)", name, tk->line);
	if (!old) strmap_put(&asm_labels, strdup(name), strdup(buf));
}
static Gvar *add_global(void);
static void topasm(const char *fmt, const char *a, const char *b) {   /* a file-scope directive, emitted verbatim */
	Gvar *g = add_global(); g->is_topasm = 1; size_t n = strlen(fmt) + strlen(a) + strlen(b) + 1; g->str = malloc(n); snprintf(g->str, n, fmt, a, b);
}
/* A declaration with no storage of its own (a prototype, an extern): weak -> `.weak name`; alias -> the symbol
 * `name` is defined as `alias` (`.set`), global unless static. */
static void decl_symbol_attrs(const char *cname, const Attr *a, int is_static) {
	const char *name = asm_name(cname);
	if (!is_static && vis_directive(a->vis)) topasm("\t%s %s", vis_directive(a->vis), name);   /* a declaration's own visibility */
	if (a->weak) { strmap_put(&weak_names, strdup(cname), (void *)1); topasm("\t.weak %s%s", name, ""); }
	if (a->alias[0]) { if (!is_static && !a->weak) topasm("\t.global %s%s", name, ""); topasm("\t.set %s, %s", name, asm_name(a->alias)); strmap_put(&alias_targets, strdup(a->alias), (void *)1); }
}
static void attr_merge(Attr *to, const Attr *a) {
	to->always_inline |= a->always_inline; to->gnu_inline |= a->gnu_inline;
	to->weak |= a->weak; to->used |= a->used; to->common |= a->common; if (a->align > to->align) to->align = a->align; if (a->pcs) to->pcs = a->pcs;
	if (a->vis) to->vis = a->vis;
	if (a->section[0]) strcpy(to->section, a->section);
	if (a->alias[0]) strcpy(to->alias, a->alias);
}
/* TYPE attributes, parsed by attribute() and applied by whoever parsed the type they follow (declspec, declarator):
 * vector_size(N) makes a GCC vector; mode(M) resizes an integer (or picks a floating type); transparent_union marks
 * a union (a typedef's: `typedef union {...} T __attribute__((transparent_union))`). */
static long pend_vsize; static int pend_mode, pend_tunion;
static int mode_of(const char *m) {   /* an integer mode's byte size; a floating one's, negated */
	static const struct { const char *n; int v; } md[] = { {"QI", 1}, {"HI", 2}, {"SI", 4}, {"DI", 8}, {"byte", 1}, {"word", 4}, {"pointer", 4}, {"SF", -4}, {"DF", -8} };
	char b[64]; size_t L = strlen(m);
	if (L > 4 && !strncmp(m, "__", 2) && !strcmp(m + L - 2, "__")) snprintf(b, sizeof b, "%.*s", (int)(L - 4), m + 2); else snprintf(b, sizeof b, "%s", m);
	for (unsigned i = 0; i < sizeof md / sizeof *md; i++) if (!strcmp(b, md[i].n)) return md[i].v;
	die("parse: unsupported mode(%s) (line %d)", m, tk->line); return 0;
}
/* Attributes with no effect on the code this compiler generates — optimization hints, diagnostics, sanitizer and
 * instrumentation controls: accepted and dropped (with any payload). An attribute neither handled nor listed here
 * is an error, never silently ignored: it may change layout or behavior (cleanup, packed, constructor, naked...). */
static int attr_ignorable(const char *nm) {
	static const char *const ok[] = {
		"noreturn", "unused", "maybe_unused", "deprecated", "unavailable", "warn_unused_result", "format", "format_arg",
		"nonnull", "returns_nonnull", "sentinel", "nonstring", "access", "warning", "error", "designated_init", "fallthrough",
		"may_alias", "warn_if_not_aligned", "const", "pure", "malloc", "alloc_size", "alloc_align", "assume_aligned", "leaf",
		"nothrow", "returns_twice", "noinline", "always_inline", "gnu_inline", "flatten", "noclone", "noipa", "hot", "cold",
		"optimize", "artificial", "externally_visible", "no_reorder", "retain", "nocommon", "tls_model",
		"no_instrument_function", "no_profile_instrument_function", "no_sanitize", "no_sanitize_address",
		"no_address_safety_analysis", "no_sanitize_thread", "no_sanitize_undefined", "no_sanitize_coverage",
		"no_stack_protector", "no_split_stack", "zero_call_used_regs", "randomize_layout", "no_randomize_layout",
		"simd",   /* SIMD clones: GCC has no vector-function ABI for 32-bit ARM, so makes none */
		NULL };
	for (int i = 0; ok[i]; i++) if (!strcmp(nm, ok[i])) return 1;
	return 0;
}
/* `__attribute__((a, b(x), ...))` (the keyword already consumed): record the declaration attributes we honor
 * into decl_attr (the type attributes into pend_*); the ignorable ones are skipped with their payload; any other is
 * an error. */
static void attribute(void) {
	expect("("); expect("(");
	while (!is(")") && tk->kind != TK_EOF) {
		char nm[64]; const char *t = tk->text; size_t L = strlen(t);
		if (L > 4 && !strncmp(t, "__", 2) && !strcmp(t + L - 2, "__")) { snprintf(nm, sizeof nm, "%.*s", (int)(L - 4), t + 2); } else snprintf(nm, sizeof nm, "%s", t);
		tk = tk->next;
		if (!strcmp(nm, "weak")) decl_attr.weak = 1;
		else if (!strcmp(nm, "always_inline")) decl_attr.always_inline = 1;
		else if (!strcmp(nm, "gnu_inline")) decl_attr.gnu_inline = 1;
		else if (!strcmp(nm, "pcs") && consume("(")) {   /* the RTABI helpers are pcs("aapcs"): core-register args/results under hard float */
			if (tk->kind != TK_STR) die("parse: __attribute__((pcs)) needs a string (line %d)", tk->line);
			if (!strcmp(tk->sval, "aapcs")) decl_attr.pcs = 1; else if (!strcmp(tk->sval, "aapcs-vfp")) decl_attr.pcs = 2;
			else die("parse: unknown pcs \"%s\"", tk->sval);
			tk = tk->next; expect(")");
		}
		else if (!strcmp(nm, "used")) decl_attr.used = 1;
		else if (!strcmp(nm, "vector_size") && consume("(")) {
			pend_vsize = eval_const(assign()); expect(")");
			if (pend_vsize <= 0) die("parse: vector_size(%ld) (line %d)", pend_vsize, tk->line);
		}
		else if (!strcmp(nm, "mode") && consume("(")) { char m[64]; ident(m); expect(")"); pend_mode = mode_of(m); }
		else if (!strcmp(nm, "transparent_union")) pend_tunion = 1;
		else if (!strcmp(nm, "visibility") && consume("(")) {   /* the symbol's ELF visibility (st_other) */
			if (tk->kind != TK_STR) die("parse: __attribute__((visibility)) needs a string (line %d)", tk->line);
			const char *v = tk->sval;
			decl_attr.vis = !strcmp(v, "default") ? VIS_DEFAULT : !strcmp(v, "hidden") ? VIS_HIDDEN : !strcmp(v, "internal") ? VIS_INTERNAL
			              : !strcmp(v, "protected") ? VIS_PROTECTED : (die("parse: unknown visibility \"%s\"", v), 0);
			tk = tk->next; expect(")");
		}
		else if ((!strcmp(nm, "section") || !strcmp(nm, "alias")) && consume("(")) {
			char buf[64] = ""; size_t bl = 0;
			if (tk->kind != TK_STR) die("parse: __attribute__((%s)) needs a string (line %d)", nm, tk->line);
			while (tk->kind == TK_STR) { size_t n = strlen(tk->sval); if (bl + n >= sizeof buf) die("parse: %s name too long", nm); memcpy(buf + bl, tk->sval, n); bl += n; buf[bl] = 0; tk = tk->next; }
			strcpy(nm[0] == 's' ? decl_attr.section : decl_attr.alias, buf);
			expect(")");
		}
		else if (!strcmp(nm, "aligned")) {
			int n = 8;   /* bare `aligned` = the target's biggest alignment */
			if (consume("(")) { n = (int)eval_const(assign()); expect(")"); }
			if (n & (n - 1)) die("parse: aligned(%d) is not a power of two", n);
			if (n > decl_attr.align) decl_attr.align = n;
		}
		else if (!strcmp(nm, "packed")) decl_attr.packed = 1;   /* a member's / an enum's (on an object or typedef GCC ignores it) */
		else if (!strcmp(nm, "cleanup") && consume("(")) { ident(decl_attr.cleanup); expect(")"); }
		else if (!strcmp(nm, "copy") && consume("(")) {   /* another declaration's attributes — not the ones tying a symbol to
		                                                 * its own definition (alias, weak, visibility) nor inlining (GCC) */
			char from[128]; ident(from); expect(")");
			const Attr *a = decl_attrs_of(from);
			if (!a) die("parse: copy(%s): not a declared function or file-scope object (line %d)", from, tk->line);
			Attr c = *a; c.alias[0] = 0; c.weak = c.vis = c.always_inline = c.gnu_inline = 0; attr_merge(&decl_attr, &c);
			if (a->packed) decl_attr.packed = 1;
			if (a->cleanup[0] && !decl_attr.cleanup[0]) strcpy(decl_attr.cleanup, a->cleanup);
		}
		else if (!strcmp(nm, "common")) decl_attr.common = 1;
		else if (!attr_ignorable(nm)) die("parse: unsupported attribute '%s' (line %d)", nm, tk->line);
		else if (is("(")) { int d = 0; do { if (is("(")) d++; else if (is(")")) d--; tk = tk->next; } while (d && tk->kind != TK_EOF); }
		if (!consume(",")) break;
	}
	expect(")"); expect(")");
}
static void skip_parens(void) { expect("("); int d = 1; while (d && tk->kind != TK_EOF) { if (is("(")) d++; else if (is(")")) d--; tk = tk->next; } }   /* skips a balanced (...) */
/* Parse `__attribute__((...))` (the `__attribute__` already consumed) for the LAYOUT attributes we honor:
 * `packed` -> *packed=1, `aligned(N)` -> *alignb=N, scalar_storage_order, transparent_union. The ignorable ones
 * (attr_ignorable) are skipped with any (...) payload; any other is an error.
 * A name may be spelled bare or double-underscored (packed / __packed__). */
static void parse_attribute(int *packed, int *alignb, int *sso, int *tunion) {
	expect("("); expect("(");
	while (!is(")") && tk->kind != TK_EOF) {   /* attribute names are plain identifiers, so match on text */
		if (!strcmp(tk->text, "packed") || !strcmp(tk->text, "__packed__")) { if (packed) *packed = 1; tk = tk->next; }
		else if (!strcmp(tk->text, "transparent_union") || !strcmp(tk->text, "__transparent_union__")) { *tunion = 1; tk = tk->next; }
		else if (!strcmp(tk->text, "scalar_storage_order") || !strcmp(tk->text, "__scalar_storage_order__")) {   /* GCC: byte order of the scalars */
			tk = tk->next; expect("(");
			if (tk->kind != TK_STR || (strcmp(tk->sval, "big-endian") && strcmp(tk->sval, "little-endian")))
				die("parse: scalar_storage_order needs \"big-endian\" or \"little-endian\" (line %d)", tk->line);
			*sso = !strcmp(tk->sval, "big-endian");      /* little-endian is this target's own order */
			tk = tk->next; expect(")");
		}
		else if (!strcmp(tk->text, "aligned") || !strcmp(tk->text, "__aligned__")) {   /* bare: the target's biggest alignment, 8 */
			tk = tk->next; int n = 8; if (consume("(")) { n = (int)eval_const(assign()); expect(")"); }
			if (n > *alignb) *alignb = n;
		}
		else {
			char nm[64]; const char *t = tk->text; size_t L = strlen(t);
			if (L > 4 && !strncmp(t, "__", 2) && !strcmp(t + L - 2, "__")) snprintf(nm, sizeof nm, "%.*s", (int)(L - 4), t + 2); else snprintf(nm, sizeof nm, "%s", t);
			if (!attr_ignorable(nm)) die("parse: unsupported attribute '%s' on a struct/union (line %d)", nm, tk->line);
			tk = tk->next; if (is("(")) { int d = 0; do { if (is("(")) d++; else if (is(")")) d--; tk = tk->next; } while (d && tk->kind != TK_EOF); }
		}
		if (!consume(",")) break;
	}
	expect(")"); expect(")");
}

static Type *qualify(Type *t, int q) {   /* a const/volatile-qualified copy (identical otherwise) */
	if (!q || (t->quals & q) == q) return t;
	if (t->kind == TY_STRUCT) return t;   /* a struct keeps its identity: it may still be incomplete (completed in place later) */
	Type *c = calloc(1, sizeof *c); *c = *t; c->quals |= q; return c;
}
/* A type in mode m: an integer keeps its signedness (and enum identity) at m's width; SF/DF name float/double. */
static Type *mode_type(Type *t, int m) {
	if (m < 0) { if (!is_fp(t)) die("parse: a floating mode on a non-floating type (line %d)", tk->line); return qualify(m == -4 ? ty_float : ty_double, t->quals); }
	if (t->kind == TY_PTR) { if (m != 4) die("parse: a pointer in a %d-byte mode (line %d)", m, tk->line); return t; }   /* SI/pointer: itself */
	if (t->kind >= TY_PTR || is_fp(t) || t->is_bool) die("parse: an integer mode on a type that is not an integer (line %d)", tk->line);
	int u = t->is_unsigned;
	Type *r = m == 1 ? (u ? ty_char : ty_schar) : m == 2 ? (u ? ty_ushort : ty_short) : m == 4 ? (u ? ty_uint : ty_int) : (u ? ty_ullong : ty_llong);
	if (t->tag) { Type *c = calloc(1, sizeof *c); *c = *r; c->tag = t->tag; r = c; }
	return qualify(r, t->quals);
}
/* vector_size applies, as in GCC, through pointers and arrays to the innermost type: `int *p
 * __attribute__((vector_size(16)))` is a pointer to a vector. */
static Type *vector_innermost(Type *t, long vsize) {
	if (t->kind == TY_PTR || t->kind == TY_ARRAY) {
		if (t->vsize_off) die("parse: vector_size on a variable-length array (line %d)", tk->line);
		Type *c = calloc(1, sizeof *c); *c = *t; c->base = vector_innermost(t->base, vsize);
		if (t->kind == TY_ARRAY) c->size = c->base->size * c->len;
		return c;
	}
	if (t->fn_ret) die("parse: vector_size on a function declarator is not supported (line %d)", tk->line);
	return qualify(vector_of(t, vsize), t->quals);
}
static Type *with_type_attrs(Type *t, long vsize, int mode) {
	if (mode) t = mode_type(t, mode);
	return vsize ? vector_innermost(t, vsize) : t;
}
/* GCC: a transparent union's members all have its size; a call passes it (any member's type) as its first member. */
static void make_transparent(Type *t) {   /* GCC drops the attribute ("union cannot be made transparent") unless the first
                                         * member has the union's size and alignment (its machine mode) */
	if (t->kind != TY_STRUCT) die("parse: transparent_union on a type that is not a union (line %d)", tk->line);
	Member *f = t->members;
	if (!f || f->is_bitfield || f->type->size != t->size || align_of(f->type) != align_of(t)) return;
	t->transparent = 1;
}
static Type *take_type_attrs(Type *t) {
	long v = pend_vsize; int m = pend_mode, tu = pend_tunion; pend_vsize = pend_mode = pend_tunion = 0;
	if (tu) make_transparent(t);
	return with_type_attrs(t, v, m);
}
static void no_type_attrs(void) { if (pend_vsize || pend_mode || pend_tunion) die("parse: a vector_size/mode/transparent_union attribute in this position is not supported (line %d)", tk->line); }
/* declaration-specifiers: fold type keywords, qualifiers, and storage classes. Width comes from the
 * base keyword (char=1, short=2, int/long=4 — `long` is 32-bit on ARM32; `long long`/64-bit is TODO),
 * SIGN from signed/unsigned (plain char defaults to unsigned, ARM's default); void ~ unsigned char (so
 * void* scales like char*). const/volatile/register/inline and __attribute__ are consumed and ignored.
 * `td` is set iff `typedef` appears; `sc` collects the storage-class bits (SC_EXTERN/SC_STATIC). Both are
 * out-params (may be NULL), NOT globals — a recursive declspec (struct members) would clobber a global. */
static Type *declspec(int *td, int *sc) {
	if (td) *td = 0;
	if (sc) *sc = 0;
	enum { B_NONE, B_VOID, B_CHAR, B_SHORT, B_INT, B_LONG, B_LLONG, B_BOOL, B_FLOAT, B_DOUBLE } base = B_NONE;
	int is_uns = 0, saw_signed = 0, seen = 0, saw_long = 0, quals = 0, mode = 0, tunion = 0, cplx = 0; long vsize = 0;   /* + the type attributes */
	Type *tagty = NULL;                                              /* struct/union/enum/typedef: a complete type */
	for (;;) {
		if (consume("typedef")) { if (td) *td = 1; continue; }
		if (consume("extern")) { if (sc) *sc |= SC_EXTERN; continue; }   /* file-scope: a reference, not a definition */
		if (consume("static")) { if (sc) *sc |= SC_STATIC; continue; }   /* file-local symbol (no .global) */
		if (consume("register")) { if (sc) *sc |= SC_REGISTER; continue; }
		if (consume("auto")) { if (sc) *sc |= SC_AUTO; continue; }   /* (a block-scope function: a nested one's forward declaration) */
		if (consume("_Thread_local")) { if (!sc) die("parse: _Thread_local here (line %d)", tk->line); *sc |= SC_TLS; continue; }   /* tracked: file-scope `register T x asm("rN")` */
		if (consume("const")) { quals |= 1; continue; }
		if (consume("volatile")) { quals |= 2; continue; }
		if (consume("restrict")) continue;
		if (consume("inline")) { if (sc) *sc |= SC_INLINE; continue; }
		if (consume("_Noreturn")) continue;   /* C11: the function doesn't return — no effect on the code we generate */
		if (consume("_Alignas")) {            /* C11 `_Alignas(N)` / `_Alignas(type)`: the declaration's alignment */
			expect("(");
			int a;
			if (is_typename()) { char d[64]; Type *t = declarator(declspec(NULL, NULL), d); a = align_of(t); }
			else a = (int)eval_const(assign());
			expect(")");
			if (a <= 0 || (a & (a - 1))) die("parse: _Alignas(%d) is not a positive power of two (line %d)", a, tk->line);
			if (a > decl_attr.align) decl_attr.align = a;
			continue;
		}
		if (consume("__extension__")) continue;   /* GNU no-op prefix */
		if (consume("__attribute__")) {   /* a type attribute here applies to the whole specifier's type, wherever it stands */
			attribute(); if (pend_vsize) vsize = pend_vsize; if (pend_mode) mode = pend_mode; tunion |= pend_tunion; pend_vsize = pend_mode = pend_tunion = 0; continue; }
		if (consume("signed")) { saw_signed = 1; seen = 1; continue; }
		if (consume("unsigned")) { is_uns = 1;     seen = 1; continue; }
		if (consume("void"))     { base = B_VOID;  seen = 1; continue; }
		if (consume("_Bool"))    { base = B_BOOL;  seen = 1; continue; }
		if (consume("_Complex")) { cplx = 1; continue; }   /* not `seen`: a typedef name may still follow (`_Complex T`) */
		if (consume("char"))     { base = B_CHAR;  seen = 1; continue; }
		if (consume("short"))    { base = B_SHORT; seen = 1; continue; }
		if (consume("int"))      { if (base != B_SHORT && base != B_LONG && base != B_LLONG) base = B_INT; seen = 1; continue; }
		if (consume("long"))     { saw_long = 1; base = base == B_DOUBLE ? B_DOUBLE : base == B_LONG ? B_LLONG : B_LONG; seen = 1; continue; }
		if (consume("float"))    { base = B_FLOAT; seen = 1; continue; }
		if (consume("double"))   { base = B_DOUBLE; seen = 1; continue; }   /* `long double` == double (ARM EABI) */
		if (consume("struct")) { tagty = struct_decl(0); seen = 1; continue; }
		if (consume("union"))  { tagty = struct_decl(1); seen = 1; continue; }
		if (consume("enum")) { tagty = enum_decl(); seen = 1; continue; }
		if (consume("typeof")) {   /* typeof(type) or typeof(expr) -> that type */
			expect("(");
			if (is_typename()) { char d[64]; tagty = declarator(declspec(NULL, NULL), d); }
			else {   /* unevaluated: type only; a function's name is its function type (not the address it decays to) */
				Node *e = assign(); add_type(e); tagty = e->type ? e->type : ty_int;
				if (e->kind == ND_ADDR && e->lhs->kind == ND_GVAR && func_declared(e->lhs->name)) tagty = fn_type_of(e->lhs->name);
			}
			expect(")"); seen = 1; continue;
		}
		if (!seen && tk->kind == TK_IDENT && typedef_find(tk->text)) { tagty = typedef_find(tk->text); tk = tk->next; seen = 1; continue; }
		break;
	}
	Type *r;
	if (tagty) r = tagty;
	else switch (base) {
	case B_VOID:  r = ty_char; break;                                   /* void ~ unsigned char (void* scales by 1) */
	case B_BOOL:  r = ty_bool; break;
	case B_FLOAT: r = ty_float; break;
	case B_DOUBLE: r = saw_long ? ty_ldouble : ty_double; break;
	case B_CHAR:  r = is_uns ? ty_char : (saw_signed ? ty_schar : ty_char); break;   /* plain char = unsigned (ARM) */
	case B_SHORT: r = is_uns ? ty_ushort : ty_short; break;
	case B_LLONG: r = is_uns ? ty_ullong : ty_llong; break;             /* long long = 64-bit (register pair) */
	default:      r = is_uns ? ty_uint : ty_int; break;                 /* int / long (32-bit on ARM32) */
	}
	if (cplx) r = complex_of(tagty || base != B_NONE ? r : ty_double);   /* `_Complex` alone: _Complex double */
	if (tunion) make_transparent(r);
	return qualify(with_type_attrs(r, vsize, mode), quals);
}
static Node *assign(void);        /* fwd: array bounds may be a constant expression, e.g. [52 + 8*32] */
static long eval_const(Node *n);
/* ---- variable-length arrays ----
 * A VLA type's byte size lives in a frame slot, assigned by a statement queued (vla_pending) where the
 * declarator is parsed; the declaration/typedef/function prologue/expression that parsed it emits the queue
 * (take_vla_pending) so the size is computed at that point, as C requires. */
static int in_func;                                      /* parsing a function (params or body): VLAs allowed */
static Node vla_ph, *vla_pt = &vla_ph;
static Node *take_vla_pending(void) { Node *h = vla_ph.next; vla_ph.next = NULL; vla_pt = &vla_ph; return h; }
static Node *vsize_node(Type *t) {   /* sizeof(t): a constant, or a VLA's size slot (maybe an enclosing function's) */
	if (!t->vsize_off) return num(t->size);
	Node *v = node(ND_VAR); strcpy(v->name, "__vla_size"); v->offset = t->vsize_off; v->type = ty_uint; v->chain = fn_depth - t->vsize_depth; return v;
}
static Node *with_vla_pending(Node *val) {   /* an expression whose type parse queued VLA sizes: ({ sizes; val; }) */
	Node *p = take_vla_pending(); if (!p) return val;
	Node *se = node(ND_STMTEXPR), **pp = &se->body; for (Node *b = p; b; b = b->next) pp = &b->next;
	se->body = p; *pp = unary(ND_EXPRSTMT, val); add_type(val); se->type = val->type; return se;
}
static Type *type_suffix(Type *base) {
	if (consume("[")) {                                       /* [] (param) allowed; else a constant expr, or a VLA bound */
		/* a parameter's `[static N]` / `[const]` / `[restrict]`: a hint and the decayed pointer's qualifiers (this
		 * compiler doesn't check const writes), so dropped; `[*]`: a VLA of unspecified size (prototypes) */
		int quals = 0;
		for (;;) {
			if (consume("static")) quals = 1;
			else if (consume("const") || consume("volatile") || consume("restrict") || consume("__restrict") || consume("__restrict__")
			         || consume("_Atomic")) quals = 1;
			else break;
		}
		if (quals && !in_params) die("parse: an array qualifier or `static` outside a parameter (line %d)", tk->line);
		if (is("*") && tk->next && !strcmp(tk->next->text, "]")) {
			if (!in_params) die("parse: `[*]` outside a prototype (line %d)", tk->line);
			tk = tk->next->next; return array_of(type_suffix(base), 0);
		}
		if (consume("]")) return array_of(type_suffix(base), 0);
		Node *e = assign(); expect("]");
		Type *inner = type_suffix(base);                        /* outer dim wraps inner */
		int ok = 1; long n = eval_try(e, &ok);
		if (ok && !inner->vsize_off) return array_of(inner, (int)n);
		if (!in_func && in_proto) return array_of(inner, 0);   /* a prototype's parameter: the bound is never evaluated ([*]) */
		if (!in_func) die("parse: variable-length array outside a function (line %d)", tk->line);
		Type *t = array_of(inner, 0); t->size = 0;
		t->vsize_off = add_local("", ty_uint); t->vsize_depth = fn_depth;
		Node *cnt = node(ND_CAST); cnt->lhs = ok ? num(n) : e; cnt->type = ty_uint;
		vla_pt = vla_pt->next = unary(ND_EXPRSTMT, binary(ND_ASSIGN, vsize_node(t), binary(ND_MUL, cnt, vsize_node(inner))));
		return t;
	}
	return base;
}
/* A FUNCTION type returning `ret`: sized like ret (so existing `fn_t *p` code keeps working), marked by fn_ret.
 * Parameters aren't modeled (calls through it place args by their own types). *fp of a pointer to one is the
 * function itself (no load), and an indirect call through it has ret's type. */
static Type *aligned_type(Type *t, int align) {   /* `typedef T name __attribute__((aligned(N)))` */
	if (!align) return t;
	Type *c = calloc(1, sizeof *c); *c = *t; c->align = align > align_of(t) ? align : align_of(t); return c;
}
static Type *func_type(Type *ret) { Type *ft = calloc(1, sizeof *ft); *ft = *ret; ft->fn_ret = ret; ft->params = NULL; ft->nparams = 0; ft->variadic = 2; return ft; }
/* `ret (params)` with the cursor at "(": a function type that knows its prototype (calls through a pointer to
 * it place FP args by the parameter types, and a `...` one uses the base PCS). */
static Type *func_proto(Type *ret) {
	Type **pts; int np = 0, va = proto_params(&pts, &np);
	Type *ft = func_type(ret); ft->variadic = va; ft->nparams = np; ft->params = np ? pts : NULL;
	return ft;
}
/* declarator = ( "*" quals | attribute )* ( "(" declarator ")" | name? ) suffix* ; recursive, so any nesting works:
 * `void (*signal(int, void (*)(int)))(int)`. The suffixes after a group apply to the base first, then the group's own
 * declarator: `(*p)[3]` points to an array, `*p[3]` is an array of pointers. The name is optional (abstract declarators
 * in prototypes and casts). A parameter list directly after the name at the outermost level is left for the caller
 * (a definition binds the parameters' names, a prototype records them); inside a group it's consumed here, and the
 * last one parsed is noted in decl_fparams — the declared function's own when the result is a function type
 * (`void (*f(void))(float) { ... }`: the definition re-parses f's (void)). A "(" that starts a parameter list (a type
 * or ")" follows) isn't a group: `int (int)` is an abstract function declarator. */
static Token *decl_fparams;
static Token *skip_balanced(Token *t) { int d = 0; do { if (t->kind == TK_PUNCT && !strcmp(t->text, "(")) d++; else if (t->kind == TK_PUNCT && !strcmp(t->text, ")")) d--; t = t->next; } while (d && t->kind != TK_EOF); return t; }
static int group_ahead(void) {   /* cursor at "(": a grouped declarator, not a parameter list? */
	Token *t = tk->next;
	while (t->kind == TK_KW && !strcmp(t->text, "__attribute__")) { t = t->next; if (t->kind == TK_PUNCT && !strcmp(t->text, "(")) t = skip_balanced(t); }
	if (t->kind == TK_PUNCT) return !strcmp(t->text, "*") || !strcmp(t->text, "(");
	return t->kind == TK_IDENT && !typedef_find(t->text);
}
static Type *declarator1(Type *base, char *name, int depth) {
	for (;;) {   /* pointers (each with its qualifiers) and attributes, in any order: `int *__attribute__((x)) *p` */
		if (consume("*")) { base = pointer_to(base); for (;;) { if (consume("const")) base = qualify(base, 1); else if (consume("volatile")) base = qualify(base, 2); else if (!consume("restrict")) break; } }
		else if (consume("__attribute__")) { attribute(); base = take_type_attrs(base); }
		else break;
	}
	if (is("(") && group_ahead()) {
		Token *in = tk->next; tk = skip_balanced(tk);
		if (is("(")) { Token *pt = tk; base = func_proto(base); decl_fparams = pt; }   /* after a group: a parameter list (noted
		                                                     * after: its parameters' own declarators reset decl_fparams) */
		else base = type_suffix(base);
		while (consume("__attribute__")) attribute();              /* trailing: `void (*f)(args) __attribute__((noreturn))` */
		base = take_type_attrs(base);
		Token *resume = tk; tk = in;
		base = declarator1(base, name, depth + 1);
		expect(")"); tk = resume;
		return base;
	}
	name[0] = 0; if (tk->kind == TK_IDENT) ident(name);      /* name omitted => abstract declarator */
	while (consume("__attribute__")) attribute();       /* trailing attr before the suffix: `int __attribute__((x)) v` */
	base = take_type_attrs(base);
	if (depth > 0 && is("(")) { Token *pt = tk; base = func_proto(base); decl_fparams = pt; }   /* inside a group: `(*f(void))` */
	base = type_suffix(base);
	while (consume("__attribute__")) attribute();       /* trailing attr after the suffix: `int v[N] __attribute__((aligned(64)))` */
	return take_type_attrs(base);
}
static Type *declarator(Type *base, char *name) { decl_fparams = NULL; return declarator1(base, name, 0); }

/* struct-spec = "struct" tag? ( "{" (declspec declarator ("," declarator)* ";")* "}" )?  — a named
 * definition registers the tag; a bare "struct tag" looks it up. Member offsets are assigned with each
 * member aligned to its own alignment, and the struct's size rounded up to its max member alignment. */
static Type *tag_find(const char *name) {                /* the innermost visible tag */
	for (Scope *sc = scope; sc; sc = sc->up) { Type *t = strmap_get(&sc->tags, name); if (t) return t; }
	return NULL;
}
static Type *tag_here(const char *name) { return strmap_get(&scope->tags, name); }   /* declared in THIS scope */
static void  tag_add(const char *name, Type *t) { if (name[0]) strmap_put(&scope->tags, strdup(name), t); }
/* Assign every member a byte offset (and, for bitfields, a bit offset within its storage unit) and set the
 * struct's size + alignment. Little-endian bit allocation, GCC/SysV rules: a bitfield lives entirely inside
 * one naturally-aligned storage unit of its declared type; `T : 0` forces the next unit boundary; `packed` (on
 * the struct, or on one member) makes the members' alignment 1 — no padding — unless one has aligned(N), which
 * still holds (and so still aligns the struct); `aligned(N)` on the struct raises it to N. */
static void layout_struct(Type *ty, int packed, int alignb, int is_union, int maxalign) {   /* maxalign: #pragma pack(n) */
	long long bitpos = 0; int salign = 1;   /* in BITS: 64-bit so a ~2 GB member can't overflow */
	if (is_union) {                         /* every member overlaps at offset 0; size = widest member */
		int maxsz = 0;
		for (Member *m = ty->members; m; m = m->next) {
			int ma = packed || m->packed ? 1 : align_of(m->type); if (m->align > ma) ma = m->align;
			if (maxalign && ma > maxalign) ma = maxalign;   /* #pragma pack caps even an aligned(N) member (GCC) */
			m->offset = 0; m->ealign = ma; if (m->is_bitfield) m->bit_offset = 0;
			if (m->type->size > maxsz) maxsz = m->type->size;
			if (ma > salign) salign = ma;
		}
		if (alignb > salign) salign = alignb;
		ty->size = (maxsz + salign - 1) & ~(salign - 1);
		ty->align = salign;
		return;
	}
	for (Member *m = ty->members; m; m = m->next) {
		int pk = packed || m->packed, msz = m->type->size, ma = pk ? 1 : align_of(m->type); if (m->align > ma) ma = m->align;   /* aligned(N) holds even when packed */
		if (maxalign && ma > maxalign) ma = maxalign;        /* #pragma pack(n): no member aligned beyond n */
		m->ealign = ma;
		if (ma > salign) salign = ma;                       /* the member's alignment is the struct's, even when it packs */
		if (m->is_bitfield) {
			if (maxalign) pk = 1;                           /* under any #pragma pack, bitfields pack densely (GCC drops the
			                                                 * straddle rule, even where pack doesn't cap their alignment) */
			int unit = ma * 8;
			if (m->bit_width == 0) { bitpos = (bitpos + unit - 1) / unit * unit; continue; }   /* :0 -> align, no storage */
			if (pk) {                                       /* bit by bit: the byte holding its first bit */
				m->offset = (int)(bitpos / 8);
				m->bit_offset = (int)(bitpos % 8);
				bitpos += m->bit_width;
				continue;
			}
			if ((bitpos % unit) + m->bit_width > msz * 8)   /* would straddle the storage unit */
				bitpos = (bitpos + unit - 1) / unit * unit;
			m->offset = (int)((bitpos / unit) * ma);
			m->bit_offset = (int)(bitpos - m->offset * 8LL);
			bitpos += m->bit_width;
		} else {
			long long byte = ((bitpos + 7) / 8 + ma - 1) & ~(long long)(ma - 1);        /* next byte, aligned to the member */
			m->offset = (int)byte;
			bitpos = (byte + msz) * 8;
		}
	}
	if (alignb > salign) salign = alignb;
	long long bytes = (bitpos + 7) / 8;
	if (bytes > 0x7fffffffLL) die("parse: struct too large (%lld bytes)", bytes);
	ty->size = (int)((bytes + salign - 1) & ~(long long)(salign - 1));
	ty->align = salign;
}

/* A struct with a variable-length member (GNU, block scope): its layout from that member on depends on the sizes, so
 * it's computed at run time where the type is declared (queued with the VLA sizes): each later member's offset in a
 * frame slot of its own (Member.voff), the struct's size in the type's size slot. The same rules as layout_struct,
 * with the position a runtime byte count plus constant bits: a bit-field after the member must be packed (then its
 * bit offset is still a constant). `first` is the first variable-length member (its offset is still constant). */
static Node *slot_ref(int off, int depth) { Node *v = node(ND_VAR); strcpy(v->name, "__vla_layout"); v->offset = off; v->type = ty_uint; v->chain = fn_depth - depth; return v; }
static Node *align_up_node(Node *x, int a) { return a <= 1 ? x : binary(ND_BITAND, binary(ND_ADD, x, tnum(a - 1, ty_uint)), tnum(-a, ty_uint)); }
static void layout_vla_struct(Type *ty, Member *first, int is_union) {
	if (is_union) die("parse: a union with a variable-length member is not supported (line %d)", tk->line);
	if (!in_func) die("parse: a variable-length member outside a function (line %d)", tk->line);
	int pos = add_local("", ty_uint), bits = 0;           /* the running position: [pos] bytes + `bits` */
	#define QUEUE(lhs, rhs) (vla_pt = vla_pt->next = unary(ND_EXPRSTMT, binary(ND_ASSIGN, lhs, rhs)))
	QUEUE(slot_ref(pos, fn_depth), tnum(first->offset, ty_uint));
	for (Member *m = first; m; m = m->next) {
		if (m->promoted || m->is_anon) die("parse: an anonymous member after a variable-length one is not supported (line %d)", tk->line);
		if (m->is_bitfield) {
			if (m->ealign != 1 || !m->bit_width) die("parse: a bit-field after a variable-length member must be packed and named (line %d)", tk->line);
			m->voff = add_local("", ty_uint); m->vdepth = fn_depth; m->offset = 0; m->bit_offset = bits % 8;
			QUEUE(slot_ref(m->voff, fn_depth), binary(ND_ADD, slot_ref(pos, fn_depth), tnum(bits / 8, ty_uint)));
			bits += m->bit_width;
			continue;
		}
		if (m != first) {
			QUEUE(slot_ref(pos, fn_depth), align_up_node(binary(ND_ADD, slot_ref(pos, fn_depth), tnum((bits + 7) / 8, ty_uint)), m->ealign));
			bits = 0;
			m->voff = add_local("", ty_uint); m->vdepth = fn_depth; m->offset = 0;
			QUEUE(slot_ref(m->voff, fn_depth), slot_ref(pos, fn_depth));
		}
		QUEUE(slot_ref(pos, fn_depth), binary(ND_ADD, slot_ref(pos, fn_depth), vsize_node(m->type)));
	}
	ty->size = 0; ty->vsize_off = add_local("", ty_uint); ty->vsize_depth = fn_depth;
	QUEUE(vsize_node(ty), align_up_node(binary(ND_ADD, slot_ref(pos, fn_depth), tnum((bits + 7) / 8, ty_uint)), ty->align));
	#undef QUEUE
}
static Type *struct_decl1(int is_union);
static Type *struct_decl(int is_union) {   /* (members aren't parameters, even in a parameter's type) */
	int ip = in_params; in_params = 0; Type *t = struct_decl1(is_union); in_params = ip; return t;
}
static Type *struct_decl1(int is_union) {
	int packed = 0, alignb = 0;
	int sso = 0, tunion = 0;
	while (consume("__attribute__")) parse_attribute(&packed, &alignb, &sso, &tunion);   /* struct __attribute__((packed)) S */
	char tag[64] = ""; if (tk->kind == TK_IDENT) ident(tag);
	if (!is("{")) {                                          /* a reference: the visible tag, else a new incomplete type */
		/* `struct tag;` alone declares a NEW incomplete type in this scope, hiding an outer one (C11 6.7.2.3p7). */
		Type *t = is(";") ? tag_here(tag) : tag_find(tag);
		if (!t) { t = calloc(1, sizeof *t); t->kind = TY_STRUCT; tag_add(tag, t); }   /* opaque; pointers to it still work */
		return t;
	}
	Type *ty = tag[0] ? tag_here(tag) : NULL;                /* a definition completes this scope's forward decl in place, */
	if (ty && ty->members) die("parse: redefinition of '%s %s' (line %d)", is_union ? "union" : "struct", tag, tk->line);
	if (!ty) { ty = calloc(1, sizeof *ty); ty->kind = TY_STRUCT; tag_add(tag, ty); }   /* ... else declares a new type here */
	expect("{");
	/* Collect the members first (with any bitfield widths), THEN lay them out — because `packed` may be
	 * written after the closing brace (`struct {...} __packed;`, the common kernel form) and must repack. */
	Member mh = {0}, *mc = &mh;
	Attr outer = decl_attr;   /* member attributes are the members' own, not the enclosing declaration's */
	int maxalign = 0;         /* #pragma pack in force at the closing brace (where GCC lays the struct out) */
	for (;;) {
		if (is("}")) { maxalign = tk->pack; tk = tk->next; break; }
		decl_attr = (Attr){0};
		Type *base = declspec(NULL, NULL);
		Attr mbase = decl_attr;
		if (consume(";")) {                                  /* no declarator: an anonymous struct/union member */
			if (base->kind == TY_STRUCT) { Member *m = calloc(1, sizeof *m); m->type = base; m->is_anon = 1; mc = mc->next = m; }
			continue;
		}
		do {
			decl_attr = mbase;
			char mname[64]; Type *mt = declarator(base, mname);
			Member *m = calloc(1, sizeof *m); strncpy(m->name, mname, 63); m->type = mt;
			if (consume(":")) { m->is_bitfield = 1; m->bit_width = (int)eval_const(assign()); }   /* type name : width */
			while (consume("__attribute__")) attribute();
			no_type_attrs();
			m->align = decl_attr.align; m->packed = decl_attr.packed;
			if (decl_attr.cleanup[0]) die("parse: cleanup on struct member '%s' (line %d)", mname, tk->line);
			mc = mc->next = m;
		} while (consume(","));
		if (!is("}")) expect(";");   /* GCC accepts the last member without one */
	}
	decl_attr = outer;
	while (consume("__attribute__")) parse_attribute(&packed, &alignb, &sso, &tunion);   /* struct {...} __attribute__((packed)) */
	ty->members = mh.next;
	layout_struct(ty, packed, alignb, is_union, maxalign);
	for (Member *m = ty->members; m; m = m->next) if (m->type->vsize_off) {
		if (maxalign) die("parse: a variable-size struct member under #pragma pack (line %d)", tk->line);
		layout_vla_struct(ty, m, is_union); break;
	}
	if (tunion) make_transparent(ty);
	if ((ty->sso = sso))                                 /* scalars and arrays of scalars reverse; nested aggregates aren't modelled */
		for (Member *m = ty->members; m; m = m->next)
			if (is_aggr(m->type) || (m->type->kind == TY_ARRAY && (m->type->base->kind == TY_ARRAY || is_aggr(m->type->base))))
				die("parse: scalar_storage_order struct '%s': member '%s' is a nested aggregate (not supported)", tag, m->name);
	/* Promote members of anonymous struct/union members into this type (accessible directly), at the
	 * anonymous block's offset + the sub-member's own offset. */
	Member *ph = NULL, *pt = NULL;
	for (Member *am = ty->members; am; am = am->next) if (am->is_anon)
		for (Member *sm = am->type->members; sm; sm = sm->next) {
			Member *pm = calloc(1, sizeof *pm); *pm = *sm;
			pm->offset = am->offset + sm->offset; pm->is_anon = 0; pm->promoted = 1; pm->next = NULL;
			if (pt) pt->next = pm; else ph = pm; pt = pm;
		}
	if (ph) { Member *t = ty->members; while (t->next) t = t->next; t->next = ph; }
	return ty;
}

/* enum [tag] { NAME [= const] , ... } — registers each constant's value. The enum TYPE is GCC's: unsigned int
 * when no value is negative, else int; (unsigned) long long if a value needs more than 32 bits; a packed enum the
 * smallest (unsigned) char/short/int that holds its values. A tag
 * remembers its type (`enum E x;` after the definition), so an enum bitfield `E f : 2` holding 3 reads 3. */
static int enum_attrs(int *mode) {   /* attributes on the enum type itself (before its tag / after its `}`): packed, mode, or ignorable */
	Attr outer = decl_attr; decl_attr = (Attr){0};
	while (consume("__attribute__")) attribute();
	if (pend_mode) { *mode = pend_mode; pend_mode = 0; }
	Attr a = decl_attr; decl_attr = outer; no_type_attrs();
	if (a.weak || a.used || a.align || a.pcs || a.vis || a.section[0] || a.alias[0] || a.cleanup[0]) die("parse: an unsupported attribute on an enum type (line %d)", tk->line);
	return a.packed;
}
static Type *enum_decl(void) {
	int mode = 0, packed = enum_attrs(&mode);
	char tag[70] = ""; if (tk->kind == TK_IDENT) { snprintf(tag, sizeof tag, "enum %s", tk->text); tk = tk->next; }
	if (!consume("{")) { Type *t = tag[0] ? tag_find(tag) : NULL; return t ? t : ty_uint; }   /* forward/unknown: GCC's unsigned default */
	long val = 0, lo = 0, hi = 0;
	while (!is("}")) {
		char nm[64]; ident(nm);
		while (consume("__attribute__")) attribute();
		no_type_attrs();
		if (consume("=")) val = eval_const(assign());   /* any const expr: another enum constant, 1<<N, … */
		ident_add(nm, ID_ENUMC)->val = val;
		if (val < lo) lo = val;
		if (val > hi) hi = val;
		val++;
		if (!consume(",")) break;
	}
	expect("}");
	packed |= enum_attrs(&mode);
	Type *t = lo < 0 ? (lo < -2147483648L || hi > 2147483647L ? ty_llong : ty_int) : (hi > 4294967295L ? ty_ullong : ty_uint);
	if (packed)   /* GCC: the smallest integer type holding every value (signed only if one is negative) */
		t = lo < 0 ? (lo >= -128 && hi <= 127 ? ty_schar : lo >= -32768 && hi <= 32767 ? ty_short : t)
		           : (hi <= 255 ? ty_char : hi <= 65535 ? ty_ushort : t);
	if (mode) t = mode_type(t, mode);   /* `enum {...} __attribute__((mode(QI)))` */
	static int enum_seq; Type *et = calloc(1, sizeof *et); *et = *t; et->tag = ++enum_seq; t = et;   /* each enum is its own type */
	if (tag[0]) tag_add(tag, t);
	return t;
}

/* ---- file-scope objects: globals + string literals ----------------------------------------------- */
Gvar *globals; static Gvar *gtail; static int str_id;
/* File-scope register variables (`register T x asm("rN");`) — x aliases a hard register everywhere. */
static struct { char name[64]; char reg[8]; } gregs[16]; static int ngregs;
static const char *greg_find(const char *name) { for (int i = 0; i < ngregs; i++) if (!strcmp(gregs[i].name, name)) return gregs[i].reg; return NULL; }
static StrMap global_map;                                /* name -> its Gvar (strings and file-scope asm have none) */
static Gvar *add_global(void) { Gvar *g = calloc(1, sizeof *g); if (gtail) gtail->next = g; else globals = g; gtail = g; return g; }
static Gvar *new_global(const char *name) { Gvar *g = add_global(); strncpy(g->name, name, 63); strmap_put(&global_map, g->name, g); return g; }
static Gvar *global_find(const char *name) { return strmap_get(&global_map, name); }

/* ---- node constructors --------------------------------------------------------------------------- */
static int is_typename(void) {   /* does a declaration start at the cursor? */
	return is("int") || is("char") || is("void") || is("short") || is("long") || is("signed") || is("unsigned") || is("_Bool") || is("float") || is("double") || is("_Complex")
	    || is("struct") || is("union") || is("enum") || is("typedef") || is("typeof")
	    || is("const") || is("volatile") || is("static") || is("extern") || is("register") || is("inline") || is("__attribute__") || is("_Thread_local")
	    || is("_Noreturn") || is("_Alignas")
	    || is("__extension__") || is("__auto_type") || is("auto")
	    || (tk->kind == TK_IDENT && typedef_find(tk->text));
}
static Node *node(NodeKind k) { Node *n = calloc(1, sizeof *n); n->kind = k; return n; }
static Node *binary(NodeKind k, Node *l, Node *r) { Node *n = node(k); n->lhs = l; n->rhs = r; return n; }
static Node *unary(NodeKind k, Node *e) { Node *n = node(k); n->lhs = e; return n; }
static Node *num(long v) { Node *n = node(ND_NUM); n->val = v; return n; }
/* A floating constant of type t (float/double): fval for folding, val = its IEEE bit pattern for codegen/data. */
static Node *fnum(double v, Type *t) {
	Node *n = node(ND_NUM); n->type = t; n->fval = t->kind == TY_FLOAT ? (double)(float)v : v;
	if (t->kind == TY_FLOAT) { float f = (float)v; unsigned u; memcpy(&u, &f, 4); n->val = u; }
	else { unsigned long long u; memcpy(&u, &v, 8); n->val = (long)u; }
	return n;
}

/* ---- expression grammar (each returns the parsed subtree; result convention lives in gen.c) ------- */
static Node *expr(void);
static Node *assign(void);
static Node *stmt(void);
static Node *labeled(Node *lab);
static Node *new_add(Node *l, Node *r);       /* +/- with pointer/array scaling (defined below) */
static Node *new_sub(Node *l, Node *r);
static Node *rmw(Node *lv, NodeKind op, Node *rhs, int post);   /* op= / ++ / -- (defined below) */

	/* C11 _Generic(ctrl, T1: e1, ..., default: eN): yield the association whose type matches ctrl's type
	 * (first match; our long==int etc. means near-identical types tie, but they resolve to the same type). */
	static int types_match_q(Type *a, Type *b, int top) {   /* C type compatibility; top-level qualifiers ignored */
		if (!a || !b || a->kind != b->kind) return 0;
		if (!top && a->quals != b->quals) return 0;
		if (a->tag != b->tag || a->is_bool != b->is_bool) return 0;          /* enums / long double are distinct */
		if (a->kind == TY_STRUCT) return a->members == b->members && a->size == b->size;   /* the same declaration */
		if (a->kind == TY_VECTOR) return a->len == b->len && types_match_q(a->base, b->base, 1);
		if (a->kind == TY_COMPLEX) return types_match_q(a->base, b->base, 1);
		if (a->kind == TY_PTR) return types_match_q(a->base, b->base, 0);
		if (a->kind == TY_ARRAY) return (!a->len || !b->len || a->len == b->len) && types_match_q(a->base, b->base, 0);
		if (a->fn_ret || b->fn_ret) return a->fn_ret && b->fn_ret && types_match_q(a->fn_ret, b->fn_ret, 1);
		return a->size == b->size && a->is_unsigned == b->is_unsigned;
	}
	static int types_match(Type *a, Type *b) { return types_match_q(a, b, 1); }
/* ---- builtins lowered in the parser ----------------------------------------------------------------
 * Each already-parsed operand is bound ONCE to a fresh temporary; the lowering is then built directly as an
 * expression tree over those temporaries — the same node kinds the parser makes for the equivalent C. */
static Node *tnum(long long v, Type *t) { Node *n = num((long)v); n->type = t; return n; }
static Node *cast_to(Type *t, Node *e) { Node *n = node(ND_CAST); n->lhs = e; n->type = t; return n; }
static Node *ref(Node *v) { Node *n = node(ND_VAR); *n = *v; n->next = NULL; return n; }   /* another read of a temporary */
static Node *cond_of(Node *c, Node *a, Node *b) { Node *n = node(ND_COND); n->cond = c; n->then = a; n->els = b; return n; }
static Node *is_zero_cmp(NodeKind k, Node *e) { return binary(k, e, tnum(0, ty_int)); }   /* e < 0, e != 0, … */
/* A fresh temporary of type t (arrays decay) initialized with e: appends `tmp = e;` to the statement list at
 * *tail and returns the variable (read it again with ref()). */
static Node *bind(Node ***tail, Type *t, Node *e) {
	if (!in_func) die("parse: an expression that needs a temporary outside a function (line %d)", tk->line);
	add_type(e);
	if (!t) t = e->type->kind == TY_ARRAY ? pointer_to(e->type->base) : e->type;
	Node *v = node(ND_VAR); strcpy(v->name, "__builtin_tmp"); v->offset = add_local("", t); v->type = t;
	**tail = unary(ND_EXPRSTMT, binary(ND_ASSIGN, ref(v), e)); *tail = &(**tail)->next;
	return v;
}
static Node *stmtexpr_of(Node *binds, Node *val) {   /* ({ binds...; val; }) */
	Node *se = node(ND_STMTEXPR), **pp = &se->body;
	for (Node *b = binds; b; ) { Node *nx = b->next; b->next = NULL; *pp = b; pp = &b->next; b = nx; }
	*pp = unary(ND_EXPRSTMT, val); add_type(val); se->type = val->type; return se;
}
/* ---- GCC vector extension -------------------------------------------------------------------------------
 * A vector (vector_size) is held and copied by value like a struct, and codegen has no vector instructions: every
 * operator is lowered here, lane by lane, to the scalar code for each lane over temporaries.
 *   a OP b     + - * / % & | ^ << >> per lane. A scalar operand is converted to the element type and used for every
 *              lane (a vector's scalar shift count as it is). The result has the left vector operand's type.
 *   a CMP b    lanes of the same-width signed integer: -1 where true, 0 where false — an opaque vector (it converts
 *              implicitly to any vector of its size)
 *   -a ~a      per lane; OP= / ++ / -- update the lvalue (evaluated once) through a pointer
 *   v[i]       lane i, an lvalue when v is one
 *   (T)v       the same bytes as another vector, or an integer, of the same size
 *   __builtin_shuffle(a[, b], mask)   lane i = lane mask[i] of a (of a then b), modulo the lane count
 * GCC's rules are errors here too: two vector operands have the same lane count and element kind and width
 * (signedness may differ); a scalar operand must convert to the element type without truncation. */
static Type *type_of(Node *n) { if (!n->type) add_type(n); return n->type; }
static int is_lval(Node *n) { return n->kind == ND_VAR || n->kind == ND_GVAR || n->kind == ND_DEREF || n->kind == ND_MEMBER; }
static Node *vec_temp(Type *t) {   /* a fresh uninitialized temporary */
	if (!in_func) die("parse: a vector operation outside a function (line %d)", tk->line);
	Node *v = node(ND_VAR); strcpy(v->name, "__vec_tmp"); v->offset = add_local("", t); v->type = t; return v;
}
static Node *lane(Node *v, int i) {   /* lane i of vector variable v */
	Node *n = node(ND_MEMBER); n->lhs = ref(v); n->offset = i * v->type->base->size; n->type = v->type->base; return n;
}
static Node *vec_var(Node ***tail, Node *e) {   /* e if a plain variable (its lanes read in place), else a temporary holding it */
	return (e->kind == ND_VAR && !e->vla_obj) || e->kind == ND_GVAR ? e : bind(tail, e->type, e);
}
static void vec_assign_ok(Type *to, Type *from, const char *what) {
	if ((is_vec(to) || is_vec(from)) && !vec_convertible(to, from)) die("parse: incompatible vector types in %s (line %d)", what, tk->line);
}
static void vec_scalar_type(Type *st) {   /* an arithmetic scalar — not a pointer, _Bool or enum-typed value (GCC) */
	if (st->kind >= TY_PTR || st->is_bool || st->tag > 0) die("parse: invalid scalar operand with a vector (line %d)", tk->line);
}
/* GCC: the scalar s converts to vector vt's element type without truncation. An integer constant must fit (a sign
 * change alone is allowed); a floating one must be exact, and only into floating lanes; a variable's type must not
 * be wider (for floating lanes: no more value bits than the significand). */
static void vec_scalar_ok(Type *vt, Node *s) {
	Type *e = vt->base, *st = type_of(s); int ok = 1, mant = e->kind == TY_FLOAT ? 24 : 53;
	vec_scalar_type(st);
	if (is_fp(st)) {
		double d = eval_fp(s, &ok);
		if (is_fp(e) && (ok ? e->kind == TY_DOUBLE || d != d || (double)(float)d == d : st->size <= e->size)) return;
		die("parse: converting the floating scalar to the vector's elements truncates it (line %d)", tk->line);
	}
	long v = eval_try(s, &ok); int fits;
	if (!ok) fits = is_fp(e) ? 8 * st->size - !st->is_unsigned <= mant : st->size <= e->size;
	else if (is_fp(e)) {   /* exact: the magnitude's significant bits fit the significand */
		unsigned long long m = st->is_unsigned || v >= 0 ? (unsigned long long)v : -(unsigned long long)v;
		while (m && !(m & 1)) m >>= 1;
		fits = !(m >> mant);
	} else {
		int bits = 8 * e->size; unsigned long long lim = bits == 64 ? ~0ULL : (1ULL << (bits - !e->is_unsigned)) - 1;   /* the largest lane value */
		fits = st->is_unsigned ? !e->is_unsigned || (unsigned long long)v <= lim
		     : v < 0 ? e->is_unsigned || bits == 64 || v >= -(long)(1ULL << (bits - 1)) : (unsigned long long)v <= lim;
	}
	if (!fits) die("parse: converting the scalar to the vector's elements truncates it (line %d)", tk->line);
}
static Node *vec_binary(NodeKind k, Node *a, Node *b) {
	Type *ta = type_of(a), *tb = type_of(b), *vt = is_vec(ta) ? ta : tb;
	int cmp = k == ND_EQ || k == ND_NE || k == ND_LT || k == ND_LE || k == ND_GT || k == ND_GE, shift = k == ND_SHL || k == ND_SHR;
	if (is_vec(ta) && is_vec(tb) && (ta->len != tb->len || ta->size != tb->size || is_fp(ta->base) != is_fp(tb->base)))
		die("parse: invalid vector operands: different lane counts or element types (line %d)", tk->line);
	if (is_fp(vt->base) && (shift || k == ND_MOD || k == ND_BITAND || k == ND_BITOR || k == ND_BITXOR))
		die("parse: invalid operator on a floating vector (line %d)", tk->line);
	Node *h = NULL, **t = &h, *x, *y;
	if (is_vec(ta)) x = vec_var(&t, a); else { vec_scalar_ok(vt, a); x = bind(&t, vt->base, a); }
	if (is_vec(tb)) y = vec_var(&t, b);
	else if (shift && is_vec(ta)) { vec_scalar_type(tb); if (is_fp(tb)) die("parse: a floating shift count (line %d)", tk->line); y = bind(&t, NULL, b); }
	else { vec_scalar_ok(vt, b); y = bind(&t, vt->base, b); }
	Type *rt = vt;
	if (cmp) {
		int w = vt->base->size;
		rt = vector_of(w == 1 ? ty_schar : w == 2 ? ty_short : w == 4 ? ty_int : ty_llong, vt->size); rt->opaque = 1;
	}
	Node *r = vec_temp(rt);
	for (int i = 0; i < vt->len; i++) {
		Node *v = binary(k, is_vec(ta) ? lane(x, i) : ref(x), is_vec(tb) ? lane(y, i) : ref(y));
		*t = unary(ND_EXPRSTMT, binary(ND_ASSIGN, lane(r, i), cmp ? unary(ND_NEG, v) : v)); t = &(*t)->next;
	}
	return stmtexpr_of(h, ref(r));
}
static Node *vec_unary(NodeKind k, Node *a) {
	Type *vt = type_of(a);
	if (k == ND_NOT) die("parse: '!' on a vector (line %d)", tk->line);
	if (k == ND_BITNOT && is_fp(vt->base)) die("parse: '~' on a floating vector (line %d)", tk->line);
	Node *h = NULL, **t = &h, *x = vec_var(&t, a), *r = vec_temp(vt);
	for (int i = 0; i < vt->len; i++) { *t = unary(ND_EXPRSTMT, binary(ND_ASSIGN, lane(r, i), unary(k, lane(x, i)))); t = &(*t)->next; }
	return stmtexpr_of(h, ref(r));
}
/* lv OP= e, ++/--: the lvalue's address once, then the operand (GCC's order), then the old value. */
static Node *vec_rmw(Node *lv, NodeKind op, Node *e, int post) {
	Type *vt = lv->type, *et = type_of(e); Node *h = NULL, **t = &h, *opd;
	Node *p = bind(&t, pointer_to(vt), unary(ND_ADDR, lv));
	if (is_vec(et) || op == ND_SHL || op == ND_SHR) opd = bind(&t, NULL, e);   /* vec_binary checks it */
	else { vec_scalar_ok(vt, e); opd = bind(&t, vt->base, e); }
	Node *old = bind(&t, vt, unary(ND_DEREF, ref(p)));
	*t = unary(ND_EXPRSTMT, binary(ND_ASSIGN, unary(ND_DEREF, ref(p)), vec_binary(op, ref(old), ref(opd)))); t = &(*t)->next;
	return stmtexpr_of(h, post ? ref(old) : unary(ND_DEREF, ref(p)));
}
static Node *vec_index(Node *v, Node *idx) {
	Type *vt = type_of(v); Node *h = NULL, **t = &h;
	Node *at = is_lval(v) ? v : bind(&t, vt, v);   /* an rvalue's lanes: from a temporary */
	Node *el = unary(ND_DEREF, new_add(cast_to(pointer_to(vt->base), unary(ND_ADDR, at)), idx));
	return h ? stmtexpr_of(h, el) : el;
}
static Node *vec_cast(Type *to, Node *e) {
	Type *f = type_of(e);
	if (!(is_vec(f) || (f->kind < TY_PTR && !is_fp(f))) || !(is_vec(to) || (to->kind < TY_PTR && !is_fp(to))))
		die("parse: a vector converts only to/from a vector or an integer (line %d)", tk->line);
	if (to->size != f->size) die("parse: cannot convert between a %d-byte and a %d-byte type involving a vector (line %d)", f->size, to->size, tk->line);
	Node *h = NULL, **t = &h, *v = bind(&t, f, e);
	return stmtexpr_of(h, unary(ND_DEREF, cast_to(pointer_to(to), unary(ND_ADDR, v))));
}
static Node *vec_shuffle(Node *a, Node *b, Node *mask) {
	Type *va = type_of(a), *vm = type_of(mask);
	if (!is_vec(va) || (b && !(is_vec(type_of(b)) && types_match(b->type, va))))
		die("parse: __builtin_shuffle: the operands must be vectors of one type (line %d)", tk->line);
	if (!is_vec(vm) || is_fp(vm->base) || vm->len != va->len || vm->base->size != va->base->size)
		die("parse: __builtin_shuffle: the mask must be an integer vector of the operands' lane count and width (line %d)", tk->line);
	int n = b ? 2 * va->len : va->len;
	Node *h = NULL, **t = &h, *m = vec_var(&t, mask), *src = vec_temp(array_of(va->base, n)), *r = vec_temp(va);
	for (int k = 0; k < (b ? 2 : 1); k++) {   /* the lanes to pick from, in one array: a then b */
		Node *dst = node(ND_MEMBER); dst->lhs = ref(src); dst->offset = k * va->size; dst->type = va;
		*t = unary(ND_EXPRSTMT, binary(ND_ASSIGN, dst, k ? b : a)); t = &(*t)->next;
	}
	for (int i = 0; i < va->len; i++) {
		Node *ix = cast_to(ty_int, binary(ND_BITAND, lane(m, i), num(n - 1)));
		*t = unary(ND_EXPRSTMT, binary(ND_ASSIGN, lane(r, i), unary(ND_DEREF, new_add(ref(src), ix)))); t = &(*t)->next;
	}
	return stmtexpr_of(h, ref(r));
}
/* ---- _Complex ------------------------------------------------------------------------------------------
 * A complex value is held by value like a struct — its real then imaginary part, of the element type — and its
 * operators are lowered here to scalar code on the parts; ND_CPAIR makes a complex from two scalar expressions
 * (codegen gives it a frame temporary). The rules are GCC's (C99 Annex G): a real operand of + - * / is not widened
 * (x + (a+bi) is (x+a) + bi, x * (a+bi) is xa + xbi); floating complex * and / call the runtime's __mul?c3 /
 * __div?c3 (inf/NaN recovery, scaled division) as GCC does, integer ones use the textbook formulas; complex
 * integer types keep their element type (_Complex char + _Complex char is _Complex char). The parts of constant
 * operands fold, so `1.0 + 2.0i` is a constant (static initializers). */
static int is_const(Node *e) { int ok = 1; add_type(e); if (is_fp(e->type)) eval_fp(e, &ok); else eval_try(e, &ok); return ok; }
static Node *clone(Node *e) {   /* a copy of a constant expression, to use it again */
	if (!e) return NULL;
	Node *c = node(e->kind); *c = *e; c->next = NULL;
	c->lhs = clone(e->lhs); c->rhs = clone(e->rhs); c->cond = clone(e->cond); c->then = clone(e->then); c->els = clone(e->els);
	Node h = {0}, *t = &h; for (Node *a = e->args; a; a = a->next) t = t->next = clone(a); c->args = h.next;
	return c;
}
static Node *cpair(Type *t, Node *re, Node *im) { Node *n = node(ND_CPAIR); n->lhs = re; n->rhs = im; n->type = t; return n; }
static Node *zero_of(Type *t) { return is_fp(t) ? fnum(0.0, t) : tnum(0, t); }
static int const_cpair(Node *e) { return e->kind == ND_CPAIR && is_const(e->lhs) && is_const(e->rhs); }
/* An operand held for reading more than once: a plain variable or a constant as it is, else a temporary. */
static Node *hold(Node ***tail, Node *e) {
	type_of(e);
	if ((e->kind == ND_VAR && !e->vla_obj) || e->kind == ND_GVAR || const_cpair(e) || (!is_cplx(e->type) && is_const(e))) return e;
	return bind(tail, NULL, e);
}
static Node *use(Node *h) { return h->kind == ND_VAR || h->kind == ND_GVAR ? ref(h) : clone(h); }
static Node *part(Node *h, int i) {   /* part i of a held complex; of a held real, i = 0 is itself (1: absent) */
	if (!is_cplx(h->type)) return i ? NULL : use(h);
	return h->kind == ND_CPAIR ? clone(i ? h->rhs : h->lhs) : lane(h, i);
}
static Type *cplx_elem(Type *a, Type *b) {   /* GCC's common element type: an equal type stays, else the wider (the unsigned one) */
	if (a->kind == b->kind && a->size == b->size && a->is_unsigned == b->is_unsigned) return a;
	if (is_fp(a) || is_fp(b)) return usual_arith(a, b);
	return a->size != b->size ? (a->size > b->size ? a : b) : a->is_unsigned ? a : b;
}
static void arith_operand(Type *t) { if (!is_cplx(t) && (t->kind >= TY_PTR || t->is_bool)) die("parse: invalid operand with a complex value (line %d)", tk->line); }
/* e converted to type `to` (either may be complex): complex -> complex converts both parts; a real becomes the real
 * part (imaginary 0); complex -> real takes the real part, -> _Bool is true when either part is nonzero. */
static Node *cplx_convert(Type *to, Node *e) {
	Type *f = type_of(e);
	if (same_cplx(to, f)) return e;
	if (!is_cplx(to) && !is_cplx(f)) return cast_to(to, e);
	if (to->is_bool) return unary(ND_NOT, unary(ND_NOT, e));   /* codegen tests both parts */
	if (to->kind >= TY_PTR && !is_cplx(to)) die("parse: a complex value converted to a non-arithmetic type (line %d)", tk->line);
	arith_operand(f);
	if (!is_cplx(to)) {   /* the real part; the imaginary one is still evaluated */
		if (e->kind == ND_CPAIR) return cast_to(to, binary(ND_COMMA, e->rhs, e->lhs));
		Node *h = NULL, **t = &h, *x = is_lval(e) ? e : bind(&t, NULL, e);
		return h ? stmtexpr_of(h, cast_to(to, lane(x, 0))) : cast_to(to, lane(x, 0));
	}
	if (!is_cplx(f)) return cpair(to, cast_to(to->base, e), zero_of(to->base));
	Node *h = NULL, **t = &h, *x = hold(&t, e);
	Node *r = cpair(to, cast_to(to->base, part(x, 0)), cast_to(to->base, part(x, 1)));
	return h ? stmtexpr_of(h, r) : r;
}
static Node *cplx_libcall(const char *fn, Type *rt, Node *a, Node *b, Node *c, Node *d) {   /* __mulsc3(a, b, c, d) ... */
	Type *e = rt->base, *pts[4] = { e, e, e, e };
	if (!func_declared(fn)) record_func_sig(fn, rt, pts, 4, 0);
	Node *n = node(ND_CALL); strcpy(n->name, fn); n->type = rt;
	n->args = cast_to(e, a); n->args->next = cast_to(e, b); n->args->next->next = cast_to(e, c); n->args->next->next->next = cast_to(e, d);
	return n;
}
static Node *cplx_binary(NodeKind k, Node *a, Node *b) {
	Type *ta = type_of(a), *tb = type_of(b); int ca = is_cplx(ta), cb = is_cplx(tb);
	arith_operand(ta); arith_operand(tb);
	if (k != ND_ADD && k != ND_SUB && k != ND_MUL && k != ND_DIV && k != ND_EQ && k != ND_NE) die("parse: invalid operator on a complex value (line %d)", tk->line);
	Type *e = cplx_elem(ca ? ta->base : ta, cb ? tb->base : tb), *rt = complex_of(e);
	Node *h = NULL, **t = &h, *x = hold(&t, a), *y = hold(&t, b), *re, *im;
	#define P(v, i) cast_to(e, part(v, i))
	switch (k) {
	case ND_ADD: case ND_SUB:   /* a missing (real operand's) imaginary part is not a zero: -(bi) for x - (a+bi) */
		re = binary(k, P(x, 0), P(y, 0));
		im = ca && cb ? binary(k, P(x, 1), P(y, 1)) : ca ? P(x, 1) : k == ND_SUB ? unary(ND_NEG, P(y, 1)) : P(y, 1);
		break;
	case ND_MUL:
		if (ca && cb && is_fp(e)) { Node *c = cplx_libcall(e->kind == TY_FLOAT ? "__mulsc3" : "__muldc3", rt, P(x, 0), P(x, 1), P(y, 0), P(y, 1)); return h ? stmtexpr_of(h, c) : c; }
		if (ca && cb) { re = binary(ND_SUB, binary(ND_MUL, P(x, 0), P(y, 0)), binary(ND_MUL, P(x, 1), P(y, 1)));
		                im = binary(ND_ADD, binary(ND_MUL, P(x, 0), P(y, 1)), binary(ND_MUL, P(x, 1), P(y, 0))); }
		else if (ca) { re = binary(ND_MUL, P(x, 0), P(y, 0)); im = binary(ND_MUL, P(x, 1), P(y, 0)); }   /* scaled by a real */
		else { re = binary(ND_MUL, P(x, 0), P(y, 0)); im = binary(ND_MUL, P(x, 0), P(y, 1)); }
		break;
	case ND_DIV:
		if (!cb) { re = binary(ND_DIV, P(x, 0), P(y, 0)); im = binary(ND_DIV, P(x, 1), P(y, 0)); break; }
		if (is_fp(e)) { Node *c = cplx_libcall(e->kind == TY_FLOAT ? "__divsc3" : "__divdc3", rt, P(x, 0), ca ? P(x, 1) : zero_of(e), P(y, 0), P(y, 1)); return h ? stmtexpr_of(h, c) : c; }
		{   /* ((ar*br + ai*bi) + (ai*br - ar*bi)i) / (br*br + bi*bi) */
			Node *ai = ca ? P(x, 1) : zero_of(e), *ai2 = ca ? P(x, 1) : zero_of(e);
			Node *den = binary(ND_ADD, binary(ND_MUL, P(y, 0), P(y, 0)), binary(ND_MUL, P(y, 1), P(y, 1)));
			Node *dv = in_func ? bind(&t, e, den) : NULL;
			re = binary(ND_DIV, binary(ND_ADD, binary(ND_MUL, P(x, 0), P(y, 0)), binary(ND_MUL, ai, P(y, 1))), dv ? ref(dv) : den);
			im = binary(ND_DIV, binary(ND_SUB, binary(ND_MUL, ai2, P(y, 0)), binary(ND_MUL, P(x, 0), P(y, 1))), dv ? ref(dv) : clone(den));
		}
		break;
	default: {   /* == / != : both parts (a missing one is 0) */
		NodeKind j = k == ND_EQ ? ND_BITAND : ND_BITOR;
		Node *c = binary(j, binary(k, P(x, 0), P(y, 0)), binary(k, ca ? P(x, 1) : zero_of(e), cb ? P(y, 1) : zero_of(e)));
		return h ? stmtexpr_of(h, c) : c;
	}
	}
	#undef P
	Node *r = cpair(rt, re, im);
	return h ? stmtexpr_of(h, r) : r;
}
static Node *cplx_unary(NodeKind k, Node *a) {   /* -z, ~z (the conjugate); !z tests both parts in codegen */
	if (k == ND_NOT) return unary(ND_NOT, a);
	Node *h = NULL, **t = &h, *x = hold(&t, a); Type *vt = x->type;
	Node *r = cpair(vt, k == ND_NEG ? unary(ND_NEG, part(x, 0)) : part(x, 0), unary(ND_NEG, part(x, 1)));
	return h ? stmtexpr_of(h, r) : r;
}
/* lv OP= e, ++/--: the lvalue's address once, the operand, then the old value; the result converts back to lv's type. */
static Node *cplx_rmw(Node *lv, NodeKind op, Node *e, int post) {
	Type *lt = lv->type; Node *h = NULL, **t = &h;
	Node *p = bind(&t, pointer_to(lt), unary(ND_ADDR, lv)), *opd = bind(&t, NULL, e), *old = bind(&t, lt, unary(ND_DEREF, ref(p)));
	*t = unary(ND_EXPRSTMT, binary(ND_ASSIGN, unary(ND_DEREF, ref(p)), cplx_convert(lt, cplx_binary(op, ref(old), ref(opd))))); t = &(*t)->next;
	return stmtexpr_of(h, post ? ref(old) : unary(ND_DEREF, ref(p)));
}
static Node *cplx_part_of(Node *e, int i) {   /* __real__ / __imag__ e: an lvalue of an lvalue; of a real e, e / 0 */
	Type *t = type_of(e);
	if (!is_cplx(t)) { arith_operand(t); return i ? binary(ND_COMMA, e, zero_of(t)) : e; }
	if (is_lval(e)) { Node *n = node(ND_MEMBER); n->lhs = e; n->offset = i * t->base->size; n->type = t->base; return n; }
	if (e->kind == ND_CPAIR) return binary(ND_COMMA, i ? e->lhs : e->rhs, i ? e->rhs : e->lhs);
	Node *h = NULL, **tl = &h, *x = bind(&tl, NULL, e);
	return stmtexpr_of(h, lane(x, i));
}
static Node *cplx_cond(Node *n) {   /* c ? a : b with a complex arm: both arms in the common complex type */
	Type *a = type_of(n->then), *b = type_of(n->els);
	arith_operand(a); arith_operand(b);
	Type *t = complex_of(cplx_elem(is_cplx(a) ? a->base : a, is_cplx(b) ? b->base : b));
	n->then = cplx_convert(t, n->then); n->els = cplx_convert(t, n->els); n->type = t;
	return n;
}
static Node *conv_args(Node *call) {   /* a prototyped call: complex arguments converted to their parameters' types */
	Type *ft = call->lhs ? call->lhs->type : NULL; if (ft && is_ptr(ft)) ft = ft->base; if (ft && !ft->fn_ret) ft = NULL;
	int i = 0;
	for (Node **ap = &call->args; *ap; ap = &(*ap)->next, i++) {
		if (type_of(*ap)->kind == TY_STRUCT && (*ap)->type->vsize_off) {   /* variable size: passed by reference (GCC's ARM ABI) */
			Node *nx = (*ap)->next; (*ap)->next = NULL; *ap = unary(ND_ADDR, *ap); (*ap)->next = nx; continue;
		}
		Type *pt = call->lhs ? (ft && i < ft->nparams ? ft->params[i] : NULL) : func_param_type(call->name, i);
		if (!pt || !(is_cplx(pt) || is_cplx(type_of(*ap))) || same_cplx(pt, (*ap)->type)) continue;
		Node *nx = (*ap)->next; (*ap)->next = NULL; *ap = cplx_convert(pt, *ap); (*ap)->next = nx;
	}
	return call;
}
static void init_leaf(InitPlace **tail, int base, Type *ty, Node *e) {   /* a whole-value initializer (a complex one converted, and
	                                                                            * split into its parts when they're separate: a constant folds) */
	if (is_cplx(ty) || is_cplx(type_of(e))) {
		e = cplx_convert(ty, e);
		if (is_cplx(ty) && e->kind == ND_CPAIR) { pi_append(tail, base, ty->base, e->lhs, 0, 0); pi_append(tail, base + ty->base->size, ty->base, e->rhs, 0, 0); return; }
	}
	pi_append(tail, base, ty, e, 0, 0);
}
static Node *un_op(NodeKind k, Node *e) { Type *t = type_of(e); return is_vec(t) ? vec_unary(k, e) : is_cplx(t) ? cplx_unary(k, e) : unary(k, e); }
static Node *arith(NodeKind k, Node *l, Node *r) {   /* a binary operator (typed now, so a chain is typed once) */
	if (is_vec(type_of(l)) || is_vec(type_of(r))) return vec_binary(k, l, r);
	if (is_cplx(l->type) || is_cplx(r->type)) return cplx_binary(k, l, r);
	Node *n = binary(k, l, r); type_node(n); return n;
}

/* __builtin_{add,sub,mul}_overflow(a, b, r): *r = a OP b wrapped to r's type T; the value is whether the EXACT
 * result doesn't fit T (any operand and result types). <=32-bit operands are exact in 64 bits. Otherwise the exact
 * result is carried as its low 64 bits W plus the part above them: for +/- the high word k of the 65-bit value
 * (S = k*2^64 + W, k from the operands' sign extensions and the carry), for * a 128-bit magnitude hi:lo (32-bit
 * halves: no 64-bit divide, the kernel provides none) and a sign. Then S is range-checked against T. */
static Node *overflow_lower(int op, Node *a, Node *b, Node *r, const char *name) {
	add_type(a); add_type(b); add_type(r);
	if (!r->type || !is_ptr(r->type) || !r->type->base) die("cc: %s: third argument must be a pointer", name);
	Type *T = r->type->base, *LL = ty_llong, *ULL = ty_ullong, *U = ty_uint, *I = ty_int;
	NodeKind k = op == '+' ? ND_ADD : op == '-' ? ND_SUB : ND_MUL;
	Node h = {0}, **t = &h.next;
	Node *x = bind(&t, NULL, a), *y = bind(&t, NULL, b), *p = bind(&t, NULL, r);
	#define STORE(v) (*t = unary(ND_EXPRSTMT, binary(ND_ASSIGN, unary(ND_DEREF, ref(p)), (v))), t = &(*t)->next)
	if (x->type->size <= 4 && y->type->size <= 4) {     /* exact in 64 bits (u32*u32 in u64) */
		Type *W = op == '*' && x->type->is_unsigned && y->type->is_unsigned ? ULL : LL;
		Node *w = bind(&t, W, binary(k, cast_to(W, ref(x)), cast_to(W, ref(y))));
		STORE(ref(w));
		Node *res = T->size <= 4 ? binary(ND_NE, cast_to(LL, unary(ND_DEREF, ref(p))), cast_to(LL, ref(w)))   /* the narrowed value differs */
		          : W == ULL ? (T->is_unsigned ? tnum(0, I) : binary(ND_GT, ref(w), tnum(0x7fffffffffffffffLL, ULL)))
		          : (T->is_unsigned ? is_zero_cmp(ND_LT, ref(w)) : tnum(0, I));
		return stmtexpr_of(h.next, res);
	}
	int bits = 8 * T->size;                              /* T's range, as unsigned 64-bit bounds */
	unsigned long long tmax = T->is_unsigned ? (bits == 64 ? ~0ULL : (1ULL << bits) - 1) : (1ULL << (bits - 1)) - 1;
	Node *fits;
	if (op != '*') {
		Node *xa = bind(&t, ULL, ref(x)), *ya = bind(&t, ULL, ref(y));   /* low 64 bits (sign- or zero-extended) */
		Node *hx = x->type->is_unsigned ? tnum(0, I) : unary(ND_NEG, is_zero_cmp(ND_LT, ref(x)));   /* 0 or -1 */
		Node *hy = y->type->is_unsigned ? tnum(0, I) : unary(ND_NEG, is_zero_cmp(ND_LT, ref(y)));
		Node *w = bind(&t, ULL, binary(k, ref(xa), ref(ya)));
		Node *carry = op == '+' ? binary(ND_LT, ref(w), ref(xa)) : binary(ND_LT, ref(xa), ref(ya));
		Node *hk = bind(&t, I, op == '+' ? binary(ND_ADD, binary(ND_ADD, hx, hy), carry) : binary(ND_SUB, binary(ND_SUB, hx, hy), carry));
		STORE(ref(w));
		if (T->is_unsigned) fits = binary(ND_AND, is_zero_cmp(ND_EQ, ref(hk)), binary(ND_LE, ref(w), tnum((long long)tmax, ULL)));
		else {                                           /* k must be W's own sign extension, and W within T */
			Node *sw = bind(&t, LL, ref(w));
			fits = binary(ND_EQ, ref(hk), unary(ND_NEG, is_zero_cmp(ND_LT, ref(sw))));
			if (bits < 64) fits = binary(ND_AND, fits, binary(ND_AND, binary(ND_GE, ref(sw), tnum(-(long long)tmax - 1, LL)),
			                                                         binary(ND_LE, ref(sw), tnum((long long)tmax, LL))));
		}
	} else {
		Node *nx = x->type->is_unsigned ? tnum(0, I) : is_zero_cmp(ND_LT, ref(x));
		Node *ny = y->type->is_unsigned ? tnum(0, I) : is_zero_cmp(ND_LT, ref(y));
		Node *neg = bind(&t, I, binary(ND_NE, nx, ny));
		Node *mx = bind(&t, ULL, x->type->is_unsigned ? ref(x) : cond_of(is_zero_cmp(ND_LT, ref(x)), unary(ND_NEG, cast_to(ULL, ref(x))), cast_to(ULL, ref(x))));
		Node *my = bind(&t, ULL, y->type->is_unsigned ? ref(y) : cond_of(is_zero_cmp(ND_LT, ref(y)), unary(ND_NEG, cast_to(ULL, ref(y))), cast_to(ULL, ref(y))));
		Node *xh = bind(&t, U, binary(ND_SHR, ref(mx), tnum(32, I))), *xl = bind(&t, U, ref(mx));
		Node *yh = bind(&t, U, binary(ND_SHR, ref(my), tnum(32, I))), *yl = bind(&t, U, ref(my));
		Node *p0 = bind(&t, ULL, binary(ND_MUL, cast_to(ULL, ref(xl)), ref(yl))), *p1 = bind(&t, ULL, binary(ND_MUL, cast_to(ULL, ref(xl)), ref(yh)));
		Node *p2 = bind(&t, ULL, binary(ND_MUL, cast_to(ULL, ref(xh)), ref(yl))), *p3 = bind(&t, ULL, binary(ND_MUL, cast_to(ULL, ref(xh)), ref(yh)));
		Node *mid = bind(&t, ULL, binary(ND_ADD, binary(ND_ADD, binary(ND_SHR, ref(p0), tnum(32, I)), cast_to(ULL, cast_to(U, ref(p1)))), cast_to(ULL, cast_to(U, ref(p2)))));
		Node *lo = bind(&t, ULL, binary(ND_ADD, binary(ND_SHL, ref(mid), tnum(32, I)), cast_to(ULL, cast_to(U, ref(p0)))));
		Node *hi = bind(&t, ULL, binary(ND_ADD, binary(ND_ADD, ref(p3), binary(ND_SHR, ref(p1), tnum(32, I))),
		                                        binary(ND_ADD, binary(ND_SHR, ref(p2), tnum(32, I)), binary(ND_SHR, ref(mid), tnum(32, I)))));
		STORE(cond_of(ref(neg), unary(ND_NEG, ref(lo)), ref(lo)));
		Node *mag = T->is_unsigned ? binary(ND_OR, unary(ND_NOT, ref(neg)), is_zero_cmp(ND_EQ, ref(lo)))   /* a negative product fits only as 0 */
		                           : tnum(1, I);
		Node *lim = T->is_unsigned ? tnum((long long)tmax, ULL) : cond_of(ref(neg), tnum((long long)(tmax + 1), ULL), tnum((long long)tmax, ULL));
		fits = binary(ND_AND, binary(ND_AND, is_zero_cmp(ND_EQ, ref(hi)), mag), binary(ND_LE, ref(lo), lim));
	}
	#undef STORE
	return stmtexpr_of(h.next, unary(ND_NOT, fits));
}
static const char *const libc_alias[] = { "memcpy", "memmove", "memset", "memcmp", "memchr", "strlen", "strnlen", "strcpy", "strncpy",
	"strcmp", "strncmp", "strchr", "strrchr", "strcat", "strstr", "abort", "puts", "printf", "sprintf", "snprintf",
	"malloc", "calloc", "realloc", "free", "exit", "strncat", "strspn", "strcspn", "strpbrk", "stpcpy", "strdup", "strndup",
	"mempcpy", "fputs", "fprintf", "putchar", "vprintf", "vsprintf", "vsnprintf", "bzero", "bcmp", "index", "rindex",
	"bcopy", "stpncpy", "strcasecmp", "strncasecmp", "fputc", "putc", "fwrite", "vfprintf", "scanf", "sscanf", "fscanf",
	"_exit", "_Exit", "tolower", "toupper", "isalnum", "isalpha", "isascii", "isblank", "iscntrl",
	"isgraph", "islower", "isprint", "ispunct", "isspace", "isupper", "isxdigit", NULL };
/* libm functions GCC knows as builtins (each also with an f / l suffix): __builtin_ceilf is a call to ceilf when not
 * expanded inline (ARMv7 has no rounding instructions), as GCC does. */
static const char *const libm_alias[] = { "acos", "acosh", "asin", "asinh", "atan", "atan2", "atanh", "cbrt", "ceil", "cos", "cosh",
	"erf", "erfc", "exp", "exp2", "expm1", "fdim", "floor", "fmax", "fmin", "fmod", "frexp", "hypot", "ilogb", "ldexp", "lgamma",
	"llrint", "llround", "log", "log10", "log1p", "log2", "logb", "lrint", "lround", "modf", "nearbyint", "nextafter", "nexttoward",
	"pow", "remainder", "remquo", "rint", "round", "scalbln", "scalbn", "sin", "sinh", "sqrt", "tan", "tanh", "tgamma", "trunc",
	"cabs", "cacos", "cacosh", "carg", "casin", "casinh", "catan", "catanh", "ccos", "ccosh", "cexp", "clog", "cpow", "cproj",
	"csin", "csinh", "csqrt", "ctan", "ctanh", NULL };
static int is_libm(const char *b) {
	for (int i = 0; libm_alias[i]; i++) {
		size_t l = strlen(libm_alias[i]);
		if (!strncmp(b, libm_alias[i], l) && (!b[l] || ((b[l] == 'f' || b[l] == 'l') && !b[l + 1]))) return 1;
	}
	return 0;
}
/* Its prototype, as GCC's builtin knows it (so a call without <math.h> still passes and returns the right types). */
static void libm_sig(const char *fn) {
	if (func_declared(fn)) return;
	size_t L = strlen(fn); char base[32]; int sfx = fn[L - 1] == 'f' ? 'f' : fn[L - 1] == 'l' ? 'l' : 0;
	snprintf(base, sizeof base, "%s", fn);
	int known = 0; for (int i = 0; libm_alias[i]; i++) if (!strcmp(fn, libm_alias[i])) known = 1;
	if (!known && sfx) base[L - 1] = 0;                  /* ceilf -> ceil (erf stays erf) */
	Type *T = sfx == 'f' && !known ? ty_float : ty_double, *C = complex_of(T), *ret = T, *pt[3] = { T, T, T }; int np = 1;
	static const char *const two[] = { "atan2", "fdim", "fmax", "fmin", "fmod", "hypot", "nextafter", "pow", "remainder", NULL };
	for (int i = 0; two[i]; i++) if (!strcmp(base, two[i])) np = 2;
	if (!strcmp(base, "frexp")) { pt[1] = pointer_to(ty_int); np = 2; }
	else if (!strcmp(base, "ldexp") || !strcmp(base, "scalbn") || !strcmp(base, "scalbln")) { pt[1] = ty_int; np = 2; }
	else if (!strcmp(base, "modf")) { pt[1] = pointer_to(T); np = 2; }
	else if (!strcmp(base, "remquo")) { pt[2] = pointer_to(ty_int); np = 3; }
	else if (!strcmp(base, "nexttoward")) { pt[1] = ty_double; np = 2; }
	else if (!strcmp(base, "ilogb")) ret = ty_int;
	else if (!strcmp(base, "lrint") || !strcmp(base, "lround")) ret = ty_int;   /* long is 32-bit */
	else if (!strcmp(base, "llrint") || !strcmp(base, "llround")) ret = ty_llong;
	else if (base[0] == 'c' && strcmp(base, "cbrt") && strcmp(base, "ceil") && strcmp(base, "cos") && strcmp(base, "cosh")) {   /* complex */
		pt[0] = pt[1] = C; ret = !strcmp(base, "cabs") || !strcmp(base, "carg") ? T : C; np = !strcmp(base, "cpow") ? 2 : 1;
	}
	record_func_sig(fn, ret, pt, np, 0);
}
/* libgcc's bit-count helpers: GCC calls them for __builtin_popcount and co. on ARMv7 (our runtime provides them). */
static const struct { const char *b, *fn; int wide; } bit_helpers[] = { {"popcount", "__popcountsi2", 0}, {"popcountl", "__popcountsi2", 0},
	{"popcountll", "__popcountdi2", 1}, {"parity", "__paritysi2", 0}, {"parityl", "__paritysi2", 0}, {"parityll", "__paritydi2", 1},
	{"clrsb", "__clrsbsi2", 0}, {"clrsbl", "__clrsbsi2", 0}, {"clrsbll", "__clrsbdi2", 1} };
/* Returns the lowered node, or NULL to fall through to the normal call path (after renaming a libc alias). */
/* The value bits of a type at byte `base` of a mask (1 = holds value): scalars whole, bit-fields their bits, unions any
 * member's — the rest is padding (__builtin_clear_padding). */
static void cover_bits(unsigned char *m, int base, Type *t) {
	if (t->kind == TY_ARRAY) { for (int i = 0; i < t->len; i++) cover_bits(m, base + i * t->base->size, t->base); return; }
	if (t->kind != TY_STRUCT) { for (int k = 0; k < t->size; k++) m[base + k] = 0xff; return; }
	for (Member *mb = t->members; mb; mb = mb->next) {
		if (mb->promoted) continue;
		if (mb->voff) die("parse: __builtin_clear_padding of a variable-size struct");
		if (!mb->is_bitfield) { cover_bits(m, base + mb->offset, mb->type); continue; }
		for (int b = 0; b < mb->bit_width; b++) { int pos = (base + mb->offset) * 8 + mb->bit_offset + b; m[pos / 8] |= 1 << (pos % 8); }
	}
}
static Node *builtin_lower(char *name) {
	const char *b = name + 10;
	for (int i = 0; libc_alias[i]; i++) if (!strcmp(b, libc_alias[i])) { memmove(name, name + 10, strlen(b) + 1); return NULL; }   /* __builtin_memcpy -> memcpy */
	if (!strcmp(b, "__clear_cache")) { strcpy(name, "__clear_cache"); return NULL; }
	if (!strcmp(b, "memcmp_eq")) { strcpy(name, "memcmp"); return NULL; }   /* GCC's memcmp whose result is only tested against 0 */
	if (is_libm(b)) { memmove(name, name + 10, strlen(b) + 1); libm_sig(name); return NULL; }
	for (unsigned i = 0; i < sizeof bit_helpers / sizeof *bit_helpers; i++) if (!strcmp(b, bit_helpers[i].b)) {
		Type *pt[1] = { bit_helpers[i].wide ? ty_ullong : ty_uint }; strcpy(name, bit_helpers[i].fn);
		if (!func_declared(name)) record_func_sig(name, ty_int, pt, 1, 0);
		return NULL;
	}
	{   /* fortify: __builtin___memcpy_chk(d, s, n, objsize) — with an unknown object size ((size_t)-1, all ours) the plain
	     * function; else the libc's __memcpy_chk. The printf ones always call their __*_chk. */
		size_t L = strlen(b);
		if (L > 6 && !strncmp(b, "__", 2) && !strcmp(b + L - 4, "_chk")) {
			static const char *const mem[] = { "memcpy", "memmove", "mempcpy", "memset", "stpcpy", "stpncpy", "strcat", "strcpy", "strncat", "strncpy", NULL };
			char base[48]; snprintf(base, sizeof base, "%.*s", (int)(L - 6), b + 2);
			expect("("); Node *args = call_args();
			Node *c = node(ND_CALL); c->args = args;
			int plain = 0;
			for (int i = 0; mem[i]; i++) if (!strcmp(base, mem[i])) {
				Node *last = args, *prev = NULL; while (last && last->next) { prev = last; last = last->next; }
				int ok = 1; long os = last ? eval_try(last, &ok) : 0;
				if (ok && (unsigned long)os == 0xffffffffUL && prev) { prev->next = NULL; plain = 1; }
			}
			snprintf(c->name, sizeof c->name, plain ? "%s" : "__%s_chk", base);
			return conv_args(c);
		}
	}
	if (!strcmp(b, "object_size") || !strcmp(b, "dynamic_object_size")) {   /* we don't track object sizes: "unknown" */
		expect("("); assign(); expect(","); long t = eval_const(assign()); expect(")");
		Node *n = num((t & 2) ? 0 : 0xffffffffL); n->type = ty_uint; return n;   /* types 0/1 -> (size_t)-1, 2/3 -> 0 (GCC semantics) */
	}
	if (!strncmp(b, "conj", 4) || !strncmp(b, "creal", 5) || !strncmp(b, "cimag", 5)) {   /* conj / creal / cimag, + f / l */
		const char *sfx = b + (b[1] == 'o' ? 4 : 5);
		if (!*sfx || !strcmp(sfx, "f") || !strcmp(sfx, "l")) {
			expect("("); Node *z = cplx_convert(complex_of(*sfx == 'f' ? ty_float : ty_double), assign()); expect(")");
			return b[1] == 'o' ? cplx_unary(ND_BITNOT, z) : cplx_part_of(z, b[1] == 'i');
		}
	}
	if (!strcmp(b, "complex")) {   /* __builtin_complex(re, im): both of one floating type */
		expect("("); Node *re = assign(); expect(","); Node *im = assign(); expect(")");
		Type *t = type_of(re); if (!is_fp(t) || type_of(im)->kind != t->kind) die("parse: __builtin_complex needs two operands of one floating type (line %d)", tk->line);
		return cpair(complex_of(t), re, im);
	}
	if (!strcmp(b, "va_arg_pack_len")) {   /* the expansion's pack size (only valid inside an always_inline expansion) */
		expect("("); expect(")");
		if (!inline_ctx) { saw_va_pack = 1; Node *n = node(ND_CALL); strcpy(n->name, name); return n; }   /* the definition's own parse */
		return num(inline_ctx->npack);
	}
	if (!strcmp(b, "return_address") || !strcmp(b, "frame_address")) {   /* the level: any integer constant expression */
		expect("("); long lv = eval_const(assign()); expect(")");
		Node *n = node(ND_CALL); strcpy(n->name, name); n->args = tnum(lv, ty_uint); n->type = pointer_to(ty_char); return n;
	}
	if (!strcmp(b, "shufflevector")) {   /* (a, b, i...): lane k = lane i_k of a then b (constants; -1: any, here 0) */
		expect("("); Node *a = assign(); expect(","); Node *bv = assign(); Type *va = type_of(a);
		if (!is_vec(va) || !is_vec(type_of(bv)) || va->base->size != bv->type->base->size || is_fp(va->base) != is_fp(bv->type->base))
			die("parse: __builtin_shufflevector needs two vectors of one element type (line %d)", tk->line);
		long idx[64]; int n = 0, na = va->len;
		while (consume(",")) { if (n == 64) die("parse: __builtin_shufflevector: too many lanes (line %d)", tk->line); idx[n++] = eval_const(assign()); }
		expect(")");
		if (!n || (n & (n - 1))) die("parse: __builtin_shufflevector: the lane count must be a power of two (line %d)", tk->line);
		Type *rt = vector_of(va->base, (long)n * va->base->size);
		Node *h = NULL, **tl = &h, *x = vec_var(&tl, a), *y = vec_var(&tl, bv), *r = vec_temp(rt);
		for (int i = 0; i < n; i++) {
			if (idx[i] < -1 || idx[i] >= na + bv->type->len) die("parse: __builtin_shufflevector: lane index %ld out of range (line %d)", idx[i], tk->line);
			Node *src = idx[i] < 0 ? zero_of(va->base) : idx[i] < na ? lane(x, (int)idx[i]) : cast_to(va->base, lane(y, (int)idx[i] - na));
			*tl = unary(ND_EXPRSTMT, binary(ND_ASSIGN, lane(r, i), src)); tl = &(*tl)->next;
		}
		return stmtexpr_of(h, ref(r));
	}
	if (!strcmp(b, "convertvector")) {   /* (v, T): each lane converted to T's element type */
		expect("("); Node *v = assign(); expect(","); char d[64]; Type *t = declarator(declspec(NULL, NULL), d); expect(")");
		Type *vt = type_of(v);
		if (!is_vec(vt) || !is_vec(t) || vt->len != t->len) die("parse: __builtin_convertvector needs vectors of one lane count (line %d)", tk->line);
		Node *h = NULL, **tl = &h, *x = vec_var(&tl, v), *r = vec_temp(t);
		for (int i = 0; i < t->len; i++) { *tl = unary(ND_EXPRSTMT, binary(ND_ASSIGN, lane(r, i), cast_to(t->base, lane(x, i)))); tl = &(*tl)->next; }
		return stmtexpr_of(h, ref(r));
	}
	if (!strcmp(b, "clear_padding")) {   /* (p): zero every padding bit of *p — between members, after them, unused bit-field bits */
		expect("("); Node *p = assign(); expect(")");
		Type *pt = type_of(p); Type *t = is_ptr_like(pt) ? pt->base : NULL;   /* (an array argument decays) */
		while (t && t->vsize_off && t->kind == TY_ARRAY) t = t->base;   /* a VLA: its elements' padding, if they have any */
		if (!t || t->vsize_off || t->size > 65536) die("parse: __builtin_clear_padding needs a pointer to a complete fixed-size object (line %d)", tk->line);
		if (t != pt->base) {
			unsigned char *m = calloc(t->size ? t->size : 1, 1); cover_bits(m, 0, t); int pad = 0;
			for (int k = 0; k < t->size; k++) pad |= m[k] != 0xff;
			free(m);
			if (pad) die("parse: __builtin_clear_padding of a variable-length array of padded elements is not supported (line %d)", tk->line);
			return binary(ND_COMMA, p, cast_to(ty_char, num(0)));   /* nothing to clear */
		}
		unsigned char *m = calloc(t->size ? t->size : 1, 1); cover_bits(m, 0, t);
		Node *h = NULL, **tl = &h, *q = bind(&tl, pointer_to(ty_char), cast_to(pointer_to(ty_char), p));
		for (int k = 0; k < t->size; k++) if (m[k] != 0xff) {
			Node *at = unary(ND_DEREF, new_add(ref(q), tnum(k, ty_int)));
			*tl = unary(ND_EXPRSTMT, m[k] ? binary(ND_ASSIGN, at, binary(ND_BITAND, unary(ND_DEREF, new_add(ref(q), tnum(k, ty_int))), tnum(m[k], ty_int)))
			                              : binary(ND_ASSIGN, at, tnum(0, ty_int))); tl = &(*tl)->next;
		}
		free(m);
		return stmtexpr_of(h, cast_to(ty_char, num(0)));   /* (void) */
	}
	if (!strcmp(b, "shuffle")) {   /* (a, mask) or (a, b, mask) */
		expect("("); Node *x = assign(), *y = NULL; expect(","); Node *m = assign();
		if (consume(",")) { y = m; m = assign(); }
		expect(")"); return vec_shuffle(x, y, m);
	}
	if (!strcmp(b, "va_copy")) { expect("("); Node *d = assign(); expect(","); Node *sv = assign(); expect(")"); return binary(ND_ASSIGN, d, sv); }
	{   /* floating constants: inf / huge_val / nan("") (payload ignored) — folded like literals */
		static const struct { const char *n; int nan; char t; } fc[] = { {"inf",0,'d'}, {"inff",0,'f'}, {"infl",0,'d'}, {"huge_val",0,'d'},
			{"huge_valf",0,'f'}, {"huge_vall",0,'d'}, {"nan",1,'d'}, {"nanf",1,'f'}, {"nanl",1,'d'} };
		for (unsigned i = 0; i < sizeof fc / sizeof *fc; i++) if (!strcmp(b, fc[i].n)) {
			expect("("); if (fc[i].nan) assign(); expect(")");
			return fnum(fc[i].nan ? strtod("nan", NULL) : strtod("inf", NULL), fc[i].t == 'f' ? ty_float : ty_double);
		}
	}
	if (!strcmp(b, "classify_type")) {   /* GCC's type classes, of the (decayed) argument's type */
		expect("("); Node *e = assign(); expect(")"); add_type(e); Type *t = e->type;
		int c = !t ? 1 : is_vec(t) ? -1 : is_cplx(t) ? 9 : t->is_bool ? 4 : is_fp(t) ? 8 : (t->kind == TY_PTR || t->kind == TY_ARRAY) ? 5
		      : t->kind == TY_STRUCT ? (t->members && t->members->next && t->members->offset == t->members->next->offset ? 13 : 12) : 1;
		return num(c);
	}
	{   /* type-generic classification / quiet comparisons over temporaries (a NaN is the one x != x) */
		static const char *const tg[] = { "isnan", "isinf", "isinff", "isinfl", "isfinite", "isgreater", "isgreaterequal",
			"isless", "islessequal", "islessgreater", "isunordered", NULL };
		int w = 0; while (tg[w] && strcmp(b, tg[w])) w++;
		if (tg[w]) {
			int two = w >= 5;
			expect("("); Node *x0 = assign(), *y0 = NULL; if (two) { expect(","); y0 = assign(); } expect(")");
			Node h = {0}, **t = &h.next, *x = bind(&t, NULL, x0), *y = two ? bind(&t, NULL, y0) : NULL;
			Node *inf = fnum(strtod("inf", NULL), ty_double), *ninf = fnum(-strtod("inf", NULL), ty_double);
			Node *xnan = binary(ND_NE, ref(x), ref(x)), *nan2 = two ? binary(ND_OR, binary(ND_NE, ref(x), ref(x)), binary(ND_NE, ref(y), ref(y))) : NULL;
			Node *v;
			if (w == 0) v = xnan;
			else if (w <= 3) v = binary(ND_OR, binary(ND_EQ, ref(x), inf), binary(ND_EQ, ref(x), ninf));
			else if (w == 4) v = binary(ND_AND, binary(ND_AND, binary(ND_EQ, ref(x), ref(x)), binary(ND_NE, ref(x), inf)), binary(ND_NE, ref(x), ninf));
			else if (w == 10) v = nan2;
			else {
				static const NodeKind rel[] = { ND_GT, ND_GE, ND_LT, ND_LE, ND_NE };   /* isgreater … islessgreater */
				v = binary(ND_AND, unary(ND_NOT, nan2), binary(rel[w - 5], ref(x), ref(y)));
			}
			return stmtexpr_of(h.next, v);
		}
	}
	if (!strcmp(b, "abs") || !strcmp(b, "labs") || !strcmp(b, "llabs") || !strcmp(b, "imaxabs")) {   /* GCC expands these inline */
		expect("("); Node *x0 = assign(); expect(")");
		Node h = {0}, **t = &h.next, *x = bind(&t, (b[0] == 'l' && b[1] == 'l') || b[0] == 'i' ? ty_llong : ty_int, x0);
		return stmtexpr_of(h.next, cond_of(is_zero_cmp(ND_LT, ref(x)), unary(ND_NEG, ref(x)), ref(x)));
	}
	if (!strcmp(b, "isdigit")) {                         /* (unsigned)c - '0' < 10u */
		expect("("); Node *c0 = assign(); expect(")");
		Node h = {0}, **t = &h.next, *c = bind(&t, NULL, c0);
		return stmtexpr_of(h.next, binary(ND_LT, binary(ND_SUB, cast_to(ty_uint, ref(c)), tnum(48, ty_uint)), tnum(10, ty_uint)));
	}
	if (!strcmp(b, "add_overflow_p") || !strcmp(b, "sub_overflow_p") || !strcmp(b, "mul_overflow_p")) {
		/* (a, b, (T)c): would a OP b overflow T? — the storing form into a T temporary (c only gives the type) */
		expect("("); Node *a = assign(); expect(","); Node *bb = assign(); expect(","); Node *c = assign(); expect(")");
		Node h = {0}, **t = &h.next, *tmp = bind(&t, NULL, c);
		return stmtexpr_of(h.next, overflow_lower(b[0] == 'a' ? '+' : b[0] == 's' ? '-' : '*', a, bb, unary(ND_ADDR, ref(tmp)), name));
	}
	int op = !strcmp(b, "add_overflow") ? '+' : !strcmp(b, "sub_overflow") ? '-' : !strcmp(b, "mul_overflow") ? '*' : 0;
	if (op) {   /* __builtin_OP_overflow(a, b, &res) */
		expect("("); Node *a = assign(); expect(","); Node *bb = assign(); expect(","); Node *r = assign(); expect(")");
		return overflow_lower(op, a, bb, r, name);
	}
	return NULL;   /* bit/frame builtins: gen_builtin expands them (it fails loud on anything else) */
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
			stmtexpr_body = 1; Node *n = node(ND_STMTEXPR); n->body = stmt()->body; expect(")"); return n;
		}
		Node *n = expr(); expect(")"); return n;
	}
	if (tk->kind == TK_NUM && tk->fp) {
		Type *t = tk->fp == 1 ? ty_float : ty_double; Node *n = fnum(tk->fval, t); int im = tk->imag; tk = tk->next;
		return im ? cpair(complex_of(t), zero_of(t), n) : n;   /* GNU 2.0i: 0 + 2.0i */
	}
	if (tk->kind == TK_NUM) {   /* C11 6.4.4.1: the type follows the SUFFIX + radix + magnitude (ARM32: long == int) */
		Node *n = num(tk->val); const char *t = tk->text;
		int u = strchr(t, 'u') || strchr(t, 'U'), ll = strstr(t, "ll") || strstr(t, "LL");
		int dec = !(t[0] == '0' && t[1]);            /* 0x.., 0.. octal -> may also go unsigned */
		unsigned long long v = (unsigned long long)tk->val;
		if (u) n->type = (!ll && v <= 0xffffffffULL) ? ty_uint : ty_ullong;
		else if (ll) n->type = (dec || v <= 0x7fffffffffffffffULL) ? ty_llong : ty_ullong;
		else if (v <= 0x7fffffffULL) n->type = ty_int;
		else if (!dec && v <= 0xffffffffULL) n->type = ty_uint;
		else if (dec || v <= 0x7fffffffffffffffULL) n->type = ty_llong;
		else n->type = ty_ullong;
		int im = tk->imag; tk = tk->next;
		return im ? cpair(complex_of(n->type), zero_of(n->type), n) : n;   /* GNU 3i: _Complex int */
	}
	if (tk->kind == TK_STR) {                                /* string literal -> anonymous .rodata array */
		Gvar *g = add_global(); g->is_str = 1; g->type = ty_char;
		snprintf(g->name, sizeof g->name, ".LSTR%d", str_id++);
		for (Token *t = tk; t->kind == TK_STR; t = t->next) if (t->wide > g->wide) g->wide = t->wide;   /* L"a" "b" is wide */
		if (g->wide) {   /* wchar_t (unsigned int, ARM EABI) / char16_t elements */
			g->wstr = strs_wide(&g->slen); g->type = array_of(g->wide == 4 ? ty_uint : ty_ushort, g->slen + 1);
		} else {
			size_t n; g->str = (char *)strs_bytes(&n); g->slen = (int)n;
			g->type = array_of(ty_char, g->slen + 1);        /* char[N]: sizeof "ab" == 3; decays to char* like any array */
		}
		Node *gv = node(ND_GVAR); strncpy(gv->name, g->name, 63); gv->type = g->type;
		return gv;
	}
	if (tk->kind == TK_IDENT) {
		char name[64]; ident(name);
		if (!strcmp(name, "__func__") || !strcmp(name, "__FUNCTION__") || !strcmp(name, "__PRETTY_FUNCTION__")) {
			/* C99 predefined identifier (+ GNU aliases): a static char[] of the current function's name.
			 * Synthesize it like a string literal so it decays to its address. */
			Gvar *g = add_global(); g->is_str = 1; g->type = ty_char;
			snprintf(g->name, sizeof g->name, ".LSTR%d", str_id++);
			if (inline_ctx) g->str = strdup(inline_ctx->name);   /* an expanded always_inline function: its own name */
			else {   /* a nested function's symbol is name.N: its name is the part before */
				g->str = strdup(cur_func_name);
				char *dot = strrchr(g->str, '.'); if (fn_depth > 1 && dot) *dot = 0;
			}
			g->slen = (int)strlen(g->str); g->type = array_of(ty_char, g->slen + 1);
			Node *gv = node(ND_GVAR); strncpy(gv->name, g->name, 63); gv->type = ty_char;
			return unary(ND_ADDR, gv);
		}
		if (!strcmp(name, "__builtin_va_start")) { expect("("); Node *n = node(ND_VA_START); n->lhs = assign(); expect(","); assign(); expect(")"); return n; }
		if (!strcmp(name, "__builtin_va_arg"))   { expect("("); Node *n = node(ND_VA_ARG); n->lhs = assign(); expect(","); char d[64]; n->type = declarator(declspec(NULL, NULL), d); expect(")");
			if (!n->type->vsize_off) return n;
			Type *t = n->type; n->type = pointer_to(t); return unary(ND_DEREF, n);   /* variable size: passed by reference (GCC's ARM ABI) */
		}
		if (!strcmp(name, "__builtin_va_end"))   { expect("("); Node *e = assign(); expect(")"); return e; }   /* no-op, but its argument's side effects happen */
		if (!strcmp(name, "__builtin_unreachable")) { expect("("); expect(")"); return num(0); }   /* no-op, not a call */
		if (!strcmp(name, "__builtin_expect"))    {   /* value is the 1st arg; the hint is still EVALUATED (side effects: expect(c, z++)) */
			expect("("); Node *e = assign(); expect(","); Node *h = assign(); expect(")");
			int ok = 1; eval_try(h, &ok); if (ok) return e;   /* constant hint (the usual case): nothing to evaluate */
			Node hd = {0}, **t = &hd.next, *v = bind(&t, NULL, e); *t = unary(ND_EXPRSTMT, h);
			return stmtexpr_of(hd.next, ref(v));
		}
		if (!strcmp(name, "__builtin_constant_p")) {   /* 1 iff the arg folds to an integer constant, or is a string literal (GCC) */
			expect("("); Node *e = assign(); expect(")");
			if (e->kind == ND_GVAR && !strncmp(e->name, ".LSTR", 5)) return num(1);
			int ok = 1; eval_try(e, &ok); return num(ok ? 1 : 0); }
		if (!strcmp(name, "__builtin_choose_expr")) {   /* compile-time ?: — pick an arm by the constant cond; the other is discarded */
			expect("("); Node *c = assign(); expect(","); Node *a = assign(); expect(","); Node *b = assign(); expect(")");
			return eval_const(c) ? a : b;
		}
		if (!strcmp(name, "__builtin_has_attribute")) {   /* (expr, attr) -> we track no decl attributes, so 0 */
			expect("("); assign(); expect(",");
			while (!is(")") && tk->kind != TK_EOF) tk = tk->next;   /* skip the attribute name */
			expect(")"); return num(0);
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
			Node *rt = NULL;   /* runtime terms: a variable index (container_of's offsetof(t, arr[i])), a variable-size layout */
			#define RT(term) (rt = rt ? binary(ND_ADD, rt, term) : (term))
			off = m->offset; if (m->voff) RT(slot_ref(m->voff, m->vdepth)); t = m->type;
			for (;;) {
				if (consume(".")) { ident(mn); for (m = t->members; m; m = m->next) if (!strcmp(m->name, mn)) break; if (!m) die("parse: __builtin_offsetof: no member '%s'", mn); off += m->offset; if (m->voff) RT(slot_ref(m->voff, m->vdepth)); t = m->type; }
				else if (consume("[")) {
					Node *ie = assign(); expect("]"); int ok = 1; long idx = eval_try(ie, &ok);
					if (t->base && t->base->vsize_off) RT(binary(ND_MUL, ie, vsize_node(t->base)));   /* a variable-size element */
					else { int elem = t->base ? t->base->size : 1;
						if (ok) off += idx * elem;                              /* constant index folds into off */
						else RT(binary(ND_MUL, ie, num(elem))); }                /* runtime index -> a term */
					if (t->base) t = t->base;
				}
				else break;
			}
			#undef RT
			expect(")"); return rt ? binary(ND_ADD, num(off), rt) : num(off);
		}
		if (!strcmp(name, "alloca") && is("(") && !local_exists(name)) strcpy(name, "__builtin_alloca");   /* GCC: always the builtin */
		if ((!strcmp(name, "fabs") || !strcmp(name, "fabsf") || !strcmp(name, "fabsl") || !strcmp(name, "copysign") || !strcmp(name, "copysignf")
		     || !strcmp(name, "copysignl")) && is("(") && !local_exists(name)) { char bn[64]; snprintf(bn, sizeof bn, "__builtin_%s", name); strcpy(name, bn); }   /* GCC: inline builtins */
		if ((!strcmp(name, "abs") || !strcmp(name, "labs") || !strcmp(name, "llabs") || !strcmp(name, "imaxabs")) && is("(") && !local_exists(name)) {
			char bn[64]; snprintf(bn, sizeof bn, "__builtin_%s", name); Node *e = builtin_lower(bn);   /* a builtin even if the TU defines its own (GCC) */
			if (!e) die("parse: internal: %s not expanded inline", bn);
			return e;
		}
		int via_builtin = 0;   /* __builtin_strncpy: the library function itself, never an inline wrapper of the name (GCC) */
		if (!strncmp(name, "__builtin_", 10) && is("(")) { Node *bn = builtin_lower(name); if (bn) return bn; via_builtin = 1; }   /* object_size/overflow/isdigit/va_copy/libc aliases */
		if (consume("(")) {                                  /* call: name(args) */
			Node *n = node(ND_CALL);
			/* Direct `bl name` if `name` is a function; INDIRECT (through the value) if it's a
			 * variable holding a function pointer — a param/local, or a global. n->lhs = the callee. */
			Ident *nf = ident_find(name);
			if (nf && nf->kind == ID_NESTFN) {               /* a nested function: + the frame it expects as its static chain */
				strncpy(n->name, nf->fname, 63); n->chain = fn_depth - nf->depth + 1;
			} else if (local_exists(name)) {                 /* a local/param fn-ptr shadows everything -> indirect */
				n->lhs = local_ref(name);
			} else if (func_declared(name)) {
				strncpy(n->name, name, 63);                 /* a known FUNCTION -> direct `bl` (wins over a same-named
				                                             * global: EXPORT_SYMBOL emits `extern typeof(fn) fn;`,
				                                             * which we'd otherwise mistake for a fn-ptr variable) */
			} else {
				Gvar *gv = global_find(name);
				if (gv) { Node *c = node(ND_GVAR); strncpy(c->name, name, 63); c->type = gv->type; n->lhs = c; }   /* a real fn-ptr global -> indirect */
				else strncpy(n->name, name, 63);            /* an as-yet-undeclared external -> direct call */
			}
			n->args = call_args();
			if (!n->lhs) n->unproto = func_unprototyped(n->name);
			if (!n->lhs && !via_builtin) { InlineDef *id = strmap_get(&inline_defs, n->name); if (id) return inline_expand(name, id, n->args); }
			return conv_args(n);
		}
		{ Ident *nf = ident_find(name); if (nf && nf->kind == ID_NESTFN) return nested_address(nf); }
		if (local_exists(name)) return local_ref(name);
		Gvar *g = global_find(name);                         /* locals shadow globals */
		if (g) { Node *n = node(ND_GVAR); strncpy(n->name, name, 63); n->type = g->type; tls_mark(n, g); return n; }
		long ev; if (enum_find(name, &ev)) return num(ev);   /* enum constant -> integer literal */
		/* Otherwise-unresolved identifier = an external symbol (usually a function). Treat it as a function
		 * designator (its address); the linker resolves it. Valid code only reaches here for externals. */
		{ Node *gv = node(ND_GVAR); strncpy(gv->name, name, 63); gv->type = ty_char; return unary(ND_ADDR, gv); }
	}
	die("parse: unexpected '%s' (line %d)", tk->text, tk->line); return NULL;
}

/* Mark an ND_MEMBER as a bitfield of declared type `ty`. Like GCC, a field promotes by its WIDTH: to int when
 * every value fits (width < 32, or signed 32), to unsigned int for unsigned :32; a long long field wider than
 * 32 bits keeps its type. */
static void bitfield_node(Node *n, Type *ty, int bw, int bo) {
	n->bit_width = bw; n->bit_offset = bo; n->bf_type = ty;
	n->type = bw > 32 ? ty : (ty->is_unsigned && bw == 32) ? ty_uint : ty_int;
	if (bw > 32 && bw < 8 * ty->size) { Type *t = calloc(1, sizeof *t); *t = *ty; t->prec = bw; n->type = t; }   /* long long:40 */
}
/* The storage a member of struct expression e is read from / written to, as GCC picks it: an lvalue (or a call's result)
 * itself; for (p = q) the assigned p; for (x, s) that of s; otherwise ((c ? p : q), ({...; s;})) a temporary copy. */
static Node *struct_place(Node *e) {
	if (is_lval(e) || e->kind == ND_CALL) return e;
	if (e->kind == ND_COMMA) return unary(ND_DEREF, binary(ND_COMMA, e->lhs, unary(ND_ADDR, struct_place(e->rhs))));
	Node *h = NULL, **t = &h;
	if (e->kind == ND_ASSIGN) {
		Node *pv = bind(&t, pointer_to(e->type), unary(ND_ADDR, e->lhs));
		*t = unary(ND_EXPRSTMT, binary(ND_ASSIGN, unary(ND_DEREF, ref(pv)), e->rhs));
		return stmtexpr_of(h, unary(ND_DEREF, ref(pv)));
	}
	Node *v = bind(&t, e->type, e); return stmtexpr_of(h, ref(v));
}
/* base.member — resolve the member's offset+type on the struct; ND_MEMBER holds the base lvalue. */
static Node *struct_member(Node *base, const char *mname) {
	add_type(base);
	if (!base->type || base->type->kind != TY_STRUCT) die("parse: '.%s' on a non-struct", mname);
	if (in_func) { base = struct_place(base); add_type(base); }
	for (Member *m = base->type->members; m; m = m->next) if (!strcmp(m->name, mname)) {
		if (m->voff) {   /* after a variable-length member: at base + the runtime offset */
			if (!is_lval(base)) die("parse: a member of a variable-size struct value (line %d)", tk->line);
			Node *at = unary(ND_DEREF, cast_to(pointer_to(m->type), new_add(cast_to(pointer_to(ty_char), unary(ND_ADDR, base)), slot_ref(m->voff, m->vdepth))));
			if (!m->is_bitfield) return at;
			Node *n = node(ND_MEMBER); n->lhs = at; n->offset = 0; bitfield_node(n, m->type, m->bit_width, m->bit_offset); return n;
		}
		Node *n = node(ND_MEMBER); n->lhs = base; n->offset = m->offset; n->type = m->type; n->sso = base->type->sso;
		if (m->is_bitfield) bitfield_node(n, m->type, m->bit_width, m->bit_offset);
		return n;
	}
	die("parse: struct has no member '%s'", mname); return NULL;
}

/* postfix := primary ( "[" expr "]" | "." ident | "->" ident )* ; a[i] = *(a+i), p->m = (*p).m. */
static Node *postfix_ops(Node *n);
static Node *postfix(void) {
	return postfix_ops(primary());
}
/* The postfix operators applied to an already-parsed operand (also a compound literal: (int[]){1,2}[i]). */
static Node *postfix_ops(Node *n) {
	for (;;) {
		if (consume("[")) {
			Node *idx = expr(); expect("]");
			if (is_vec(type_of(n))) { n = vec_index(n, idx); continue; }
			int rev = (n->kind == ND_MEMBER && n->sso) || (idx->kind == ND_MEMBER && idx->sso);   /* an element of a reversed array */
			n = unary(ND_DEREF, new_add(n, idx)); n->sso = rev;
		}
		else if (consume(".")) { char m[64]; ident(m); n = struct_member(n, m); }
		else if (consume("->")) { char m[64]; ident(m); n = struct_member(unary(ND_DEREF, n), m); }
		else if (consume("++")) n = rmw(n, ND_ADD, num(1), 1);   /* x++ */
		else if (consume("--")) n = rmw(n, ND_SUB, num(1), 1);   /* x-- */
		else if (consume("(")) {   /* call on an arbitrary expr: _Generic(...)(args), (fp)(args), f(x)(y) — indirect via lhs */
			Node *c = node(ND_CALL); c->lhs = n;
			c->args = call_args(); n = conv_args(c);
		}
		else return n;
	}
}

/* Is the cursor at a cast `(type)`? Peek past "(" without consuming. */
static int cast_ahead(void) {
	if (!is("(")) return 0;
	Token *save = tk; tk = tk->next; while (is("__extension__")) tk = tk->next;   /* `(__extension__ (...))` is an expression */
	int r = is_typename() && !is("__extension__"); tk = save; return r;
}

static Node *unary_expr(void) {
	if (consume("__extension__")) return unary_expr();       /* GNU: a no-op prefix on any unary expression */
	if (cast_ahead()) {                                      /* (type) expr — re-types the operand */
		char d[64]; expect("(");
		int to_void = is("void") && !strcmp(tk->next->text, ")");   /* (void) — `void` is char in our types */
		Type *t = declarator(declspec(NULL, NULL), d); expect(")");
		if (is("{")) {   /* compound literal (type){init}: an anonymous initialized object, yields its lvalue */
			static int cl_seq;
			char nm[32]; snprintf(nm, sizeof nm, ".Lcl%d", cl_seq++);
			InitPlace *pl = init_places(t);                  /* first: `(T[]){...}` takes its size from the braces */
			if (!in_func && unevaluated) { Node *gv = node(ND_GVAR); strcpy(gv->name, ".Lunevaluated"); gv->type = t; return postfix_ops(gv); }   /* sizeof: no object */
			if (!in_func) {   /* file scope (a static initializer's `&(struct B){...}`): an anonymous static object */
				char cl[32]; snprintf(cl, sizeof cl, ".Lcl%d", cl_seq++); Gvar *g = new_global(cl); g->type = t; g->is_static = 1;
				g->init = lower_global(pl, t->size);
				Node *gv = node(ND_GVAR); strncpy(gv->name, g->name, 63); gv->type = t; return postfix_ops(gv);
			}
			int off = add_local(nm, t);
			Node *v = node(ND_VAR); strncpy(v->name, nm, 63); v->offset = off; v->type = t;
			Node *initb = lower_local(v, pl, t->size);       /* block of member/element assignments to v */
			Node *y = node(ND_VAR); strncpy(y->name, nm, 63); y->offset = off; y->type = t;
			initb->next = unary(ND_EXPRSTMT, y);             /* ...then the statement-expression yields v */
			Node *se = node(ND_STMTEXPR); se->body = initb; se->type = t; return postfix_ops(se);
		}
		Node *e = unary_expr(); add_type(e);
		if (t->kind == TY_STRUCT && e->type && e->type->kind != TY_STRUCT) {   /* GNU cast to union: (U)x == a temp U with its x-typed member set */
			Member *m = t->members; while (m && !(m->type->kind == e->type->kind && m->type->size == e->type->size)) m = m->next;
			if (!m || m->offset) die("parse: cast to union: no member of the operand's type (line %d)", tk->line);
			static int cu_seq; char nm[32]; snprintf(nm, sizeof nm, ".Lcu%d", cu_seq++);
			int off = add_local(nm, t);
			Node *v = node(ND_VAR); strncpy(v->name, nm, 63); v->offset = off; v->type = t;
			Node *mv = node(ND_MEMBER); mv->lhs = v; mv->offset = 0; mv->type = m->type;
			Node *y = node(ND_VAR); strncpy(y->name, nm, 63); y->offset = off; y->type = t;
			Node *st = unary(ND_EXPRSTMT, binary(ND_ASSIGN, mv, e)); st->next = unary(ND_EXPRSTMT, y);
			Node *se = node(ND_STMTEXPR); se->body = st; se->type = t; return se;
		}
		if (is_vec(t) || is_vec(e->type)) return to_void ? binary(ND_COMMA, e, num(0)) : vec_cast(t, e);   /* (void)v: evaluated, discarded */
		if (is_cplx(t) || is_cplx(e->type)) return to_void ? binary(ND_COMMA, e, num(0)) : cplx_convert(t, e);
		Node *n = node(ND_CAST); n->lhs = e; n->type = t; return with_vla_pending(n);
	}
	if (consume("sizeof")) {                                 /* sizeof(type) or sizeof expr -> a constant */
		if (cast_ahead()) {
			Token *at = tk; char d[64]; expect("("); Type *t = declarator(declspec(NULL, NULL), d); expect(")");
			if (!is("{")) return with_vla_pending(vsize_node(t));
			tk = at;   /* `sizeof (T){...}`: a compound literal — unevaluated, its type only */
		}
		int ue = unevaluated; unevaluated = 1; Node *e = unary_expr(); unevaluated = ue;
		add_type(e); return e->type ? vsize_node(e->type) : num(4);
	}
	if (consume("_Alignof")) {   /* __alignof__(type|expr) -> a constant */
		if (cast_ahead()) { char d[64]; expect("("); Type *t = declarator(declspec(NULL, NULL), d); expect(")"); return num(align_of(t)); }
		Node *e = unary_expr(); add_type(e); return num(decl_align(e));
	}
	if (consume("++")) return rmw(unary_expr(), ND_ADD, num(1), 0);   /* ++x */
	if (consume("--")) return rmw(unary_expr(), ND_SUB, num(1), 0);   /* --x */
	if (consume("&&")) {   /* &&label : GNU address-of-label (maybe an enclosing function's __label__) */
		Node *n = node(ND_LABELADDR); ident(n->name); int li = map_label(n->name);
		if (li >= 0 && lscope[li].depth < fn_depth) n->owner = strdup(lscope[li].owner);
		return n;
	}
	if (consume("&")) {                                      /* address-of */
		Node *e = unary_expr();
		add_type(e);
		if (e->sso && e->type->kind != TY_ARRAY) die("parse: address of a scalar with reverse storage order (line %d)", tk->line);   /* GCC: an error */
		return unary(ND_ADDR, e);
	}
	if (consume("*")) return unary(ND_DEREF, unary_expr());  /* dereference */
	if (consume("__real__")) return cplx_part_of(unary_expr(), 0);
	if (consume("__imag__")) return cplx_part_of(unary_expr(), 1);
	if (consume("-")) return un_op(ND_NEG, unary_expr());
	if (consume("!")) return un_op(ND_NOT, unary_expr());
	if (consume("~")) return un_op(ND_BITNOT, unary_expr());
	if (consume("+")) return unary_expr();                   /* unary plus is a no-op */
	return postfix();
}

/* +/- with C pointer semantics: `ptr + int` scales the int by the pointee size; `int + ptr` is
 * commuted to `ptr + int`; `ptr - ptr` is the element distance (difference / pointee size). */
static Node *new_add(Node *l, Node *r) {
	add_type(l); add_type(r);
	if (is_vec(l->type) || is_vec(r->type)) return vec_binary(ND_ADD, l, r);
	if (is_cplx(l->type) || is_cplx(r->type)) return cplx_binary(ND_ADD, l, r);
	if (is_ptr_like(l->type) && is_ptr_like(r->type)) die("parse: cannot add two pointers");
	if (!is_ptr_like(l->type) && is_ptr_like(r->type)) { Node *t = l; l = r; r = t; }
	if (is_ptr_like(l->type)) r = binary(ND_MUL, r, vsize_node(l->type->base));   /* scale by element size (a VLA row: runtime) */
	return binary(ND_ADD, l, r);
}
static Node *new_sub(Node *l, Node *r) {
	add_type(l); add_type(r);
	if (is_vec(l->type) || is_vec(r->type)) return vec_binary(ND_SUB, l, r);
	if (is_cplx(l->type) || is_cplx(r->type)) return cplx_binary(ND_SUB, l, r);
	if (is_ptr_like(l->type) && is_ptr_like(r->type)) return binary(ND_DIV, binary(ND_SUB, l, r), vsize_node(l->type->base));
	if (is_ptr_like(l->type)) r = binary(ND_MUL, r, vsize_node(l->type->base));
	return binary(ND_SUB, l, r);
}
static Node *mul(void)   { Node *n = unary_expr(); for (;;) { if (consume("*")) n = arith(ND_MUL, n, unary_expr()); else if (consume("/")) n = arith(ND_DIV, n, unary_expr()); else if (consume("%")) n = arith(ND_MOD, n, unary_expr()); else return n; } }
static Node *add(void)   { Node *n = mul();         for (;;) { if (consume("+")) n = new_add(n, mul()); else if (consume("-")) n = new_sub(n, mul()); else return n; } }
static Node *shift(void) { Node *n = add();         for (;;) { if (consume("<<")) n = arith(ND_SHL, n, add()); else if (consume(">>")) n = arith(ND_SHR, n, add()); else return n; } }
static Node *rel(void)   { Node *n = shift();       for (;;) { if (consume("<")) n = arith(ND_LT, n, shift()); else if (consume("<=")) n = arith(ND_LE, n, shift()); else if (consume(">")) n = arith(ND_GT, n, shift()); else if (consume(">=")) n = arith(ND_GE, n, shift()); else return n; } }
static Node *eq(void)    { Node *n = rel();         for (;;) { if (consume("==")) n = arith(ND_EQ, n, rel()); else if (consume("!=")) n = arith(ND_NE, n, rel()); else return n; } }
static Node *bitand(void){ Node *n = eq();          while (consume("&")) n = arith(ND_BITAND, n, eq()); return n; }
static Node *bitxor(void){ Node *n = bitand();      while (consume("^")) n = arith(ND_BITXOR, n, bitand()); return n; }
static Node *bitor(void) { Node *n = bitxor();      while (consume("|")) n = arith(ND_BITOR, n, bitxor()); return n; }
static Node *logand(void){ Node *n = bitor();       while (consume("&&")) n = binary(ND_AND, n, bitor()); return n; }
static Node *logor(void) { Node *n = logand();      while (consume("||")) n = binary(ND_OR, n, logand()); return n; }
static Node *conditional(void){ Node *c = logor(); if (!consume("?")) return c;   /* c ? then : els */
	Node *n = node(ND_COND); n->cond = c;
	if (is(":")) n->then = c;                    /* GNU `a ?: b` == `a ? a : b` (a re-evaluated; fine for side-effect-free) */
	else n->then = expr();
	expect(":"); n->els = conditional();
	if (is_cplx(type_of(n->then)) || is_cplx(type_of(n->els))) return cplx_cond(n);
	return n; }
/* Read-modify-write `lv OP= e` (and ++/--, post=1 yields the old value). ND_RMW evaluates lv's address ONCE
 * (a[i++] += 1 bumps i once), then the operand e (init) — BEFORE reading lv, GCC's order (x |= f() sees f's
 * store to x) — then the old value. rhs = OP(ND_CUR old, ND_CUR operand), so the usual typing and pointer
 * scaling (new_add/new_sub) apply unchanged. */
static Node *rmw(Node *lv, NodeKind op, Node *e, int post) {
	add_type(lv); add_type(e);
	if (!is_lval(lv)) die("parse: assignment to non-lvalue (line %d)", tk->line);
	if (is_vec(lv->type)) return vec_rmw(lv, op, e, post);
	if (is_vec(e->type)) die("parse: a vector operand updating a scalar (line %d)", tk->line);
	if (is_cplx(lv->type) || is_cplx(e->type)) return cplx_rmw(lv, op, e, post);
	Node *r = node(ND_RMW), *cur = node(ND_CUR), *opd = node(ND_CUR);
	cur->type = lv->type; cur->target = r; opd->type = e->type; opd->target = r; opd->val = 1;
	r->lhs = lv; r->init = e; r->is_post = post;
	r->rhs = op == ND_ADD ? new_add(cur, opd) : op == ND_SUB ? new_sub(cur, opd) : binary(op, cur, opd);
	return r;
}
static Node *assign(void) {
	Node *n = conditional();
	static const struct { const char *s; NodeKind k; } ops[] = { {"+=", ND_ADD}, {"-=", ND_SUB}, {"*=", ND_MUL}, {"/=", ND_DIV}, {"%=", ND_MOD},
		{"&=", ND_BITAND}, {"|=", ND_BITOR}, {"^=", ND_BITXOR}, {"<<=", ND_SHL}, {">>=", ND_SHR} };
	for (unsigned i = 0; i < sizeof ops / sizeof *ops; i++) if (consume(ops[i].s)) return rmw(n, ops[i].k, assign(), 0);
	if (!consume("=")) return n;
	if (!is_lval(n)) die("parse: assignment to non-lvalue (line %d)", tk->line);
	Node *r = assign();
	if (type_of(n)->kind == TY_STRUCT && n->type->vsize_off) {   /* a variable-size struct: memcpy its run-time size */
		Node *h = NULL, **t = &h, *p = bind(&t, pointer_to(n->type), unary(ND_ADDR, n));
		Node *c = node(ND_CALL); strcpy(c->name, "memcpy"); c->args = ref(p); c->args->next = unary(ND_ADDR, r); c->args->next->next = vsize_node(n->type);
		*t = unary(ND_EXPRSTMT, c);
		return stmtexpr_of(h, unary(ND_DEREF, ref(p)));
	}
	if (is_vec(type_of(n))) vec_assign_ok(n->type, type_of(r), "an assignment");   /* (a vector into a scalar: codegen's check) */
	if (is_cplx(n->type) || is_cplx(type_of(r))) r = cplx_convert(n->type, r);
	return binary(ND_ASSIGN, n, r);
}
static Node *expr(void)  { Node *n = assign(); while (consume(",")) n = binary(ND_COMMA, n, assign()); return n; }   /* comma operator */

/* ---- statements ---------------------------------------------------------------------------------- */
/* Aggregate/brace initializer for a local: `{ e0, e1, ... }` -> a block of member/element assignments to
 * `dest` (an lvalue). Recurses for nested braces; a scalar with braces takes the first element. Partial
 * initializers just stop (the rest is left as-is — no zero-fill, an M1 simplification). */
/* Aggregate/brace initializer for a local (and for compound literals): parse into placements via the shared
 * traversal, then lower to a block of stores against `dest`. */
static InitPlace *init_places(Type *ty) {   /* parse only; sizes an unsized array `T x[]` in place */
	InitPlace head = {0}, *tail = &head;
	parse_init(ty, 0, &tail);
	return head.next;
}
static Node *init_of(Node *dest, Type *ty) { InitPlace *pl = init_places(ty); return lower_local(dest, pl, ty->size); }

static int has_jump_target(Node *n);
/* Same, for ONE node's children (not its ->next siblings). */
static int node_has_jump_target(Node *n) {
	if (!n) return 0;
	if (n->kind == ND_LABEL || n->kind == ND_CASE) return 1;
	Node *kids[] = { n->lhs, n->rhs, n->cond, n->then, n->els, n->init, n->inc, n->body };
	for (unsigned i = 0; i < sizeof kids / sizeof *kids; i++) if (has_jump_target(kids[i])) return 1;
	if (n->kind == ND_CALL) for (Node *a = n->args; a; a = a->next) if (has_jump_target(a)) return 1;
	return 0;
}
/* `switch (CONST) { case A: ...; break; case B: ...; }` — keep only the selected case, as GCC's DCE does. The kernel
 * relies on this: `switch (sizeof(x)) { case 1: ... case 8: <asm using %R on a 64-bit value> }` must not emit the
 * dead cases (their asm is only valid for that size). The SWITCH node stays, so `break` still means "leave it".
 * Kept: the target case through the first top-level `break` (fallthrough included). Bails (no fold) if a dropped
 * statement holds a label/case something could still jump to. */
static void fold_const_switch(Node *sw) {
	int ok = 1; long v = eval_try(sw->cond, &ok);
	if (!ok || !sw->then || sw->then->kind != ND_BLOCK) return;
	Node *target = NULL, *deflt = NULL;
	for (Node *s = sw->then->body; s; s = s->next) if (s->kind == ND_CASE) {
		if (s->is_default) { if (!deflt) deflt = s; }
		else if (s->is_range ? (v >= s->val && v <= s->val2) : v == s->val) { if (!target) target = s; }
	}
	if (!target) target = deflt;
	/* phase 0 = before target (dropped), 1 = kept run, 2 = after the run's `break` (dropped) */
	Node *keep_end = NULL; int phase = 0;
	for (Node *s = sw->then->body; s; s = s->next) {
		if (phase == 0 && s == target) phase = 1;
		if (phase == 1) { keep_end = s; if (s->kind == ND_BREAK) phase = 2; continue; }
		if (s->kind != ND_CASE && node_has_jump_target(s)) return;   /* a goto could still enter it: don't fold */
	}
	if (!target) { sw->then->body = NULL; sw->case_list = NULL; return; }   /* no case matches, no default */
	keep_end->next = NULL;
	sw->then->body = target;
	Node ch = {0}, *cc = &ch;   /* case_list = every case marker in the kept run (fallthrough ones need labels too) */
	for (Node *s = target; s; s = s->next) if (s->kind == ND_CASE) { cc->case_next = s; cc = s; }
	cc->case_next = NULL; sw->case_list = ch.case_next;
}
/* Does a statement subtree contain a label or case (a place control can enter from outside)? */
static int has_jump_target(Node *n) {
	for (; n; n = n->next) {
		if (n->kind == ND_LABEL || n->kind == ND_CASE) return 1;
		if (has_jump_target(n->lhs) || has_jump_target(n->rhs) || has_jump_target(n->cond) || has_jump_target(n->then) ||
		    has_jump_target(n->els) || has_jump_target(n->init) || has_jump_target(n->inc) || has_jump_target(n->body)) return 1;
		if (n->kind == ND_CALL) for (Node *a = n->args; a; a = a->next) if (has_jump_target(a)) return 1;
	}
	return 0;
}
/* __attribute__((cleanup(fn))) locals (the kernel's guard() / __free()): fn(&var) runs whenever var's scope is left —
 * the block's end, a return (after its value is computed), break/continue out of it, a goto to a label outside it —
 * innermost first, as in GCC. The active ones form a stack (an entry's `up` is the one declared before it); a jump
 * runs the entries between its own point and its target's. A jump INTO such a scope (goto, case) is an error. */
typedef struct Cleanup { Node *var; char fn[64]; struct Cleanup *up; } Cleanup;
static Cleanup *cleanups;                                 /* the innermost active one (NULL: none) */
static Cleanup *brk_cleanups, *cont_cleanups, *case_cleanups;   /* ...at the start of the innermost break / continue / case target */
typedef struct { Node *n; Cleanup *at; } JumpAt;          /* a goto / label and the cleanups active there */
static JumpAt *fn_gotos, *fn_labels; static int nfn_gotos, nfn_labels, fn_gotos_cap, fn_labels_cap;
static void jump_note(JumpAt **a, int *n, int *cap, Node *x) {
	if (*n == *cap) { *cap = *cap ? 2 * *cap : 64; *a = realloc(*a, *cap * sizeof **a); }
	(*a)[*n].n = x; (*a)[*n].at = cleanups; (*n)++;
}
static Node *cleanup_calls(Cleanup *from, Cleanup *to) {   /* fn(&var) for the entries from `from` down to (not incl.) `to` */
	Node h = {0}, *c = &h;
	for (Cleanup *e = from; e != to; e = e->up) {
		if (!e) die("parse: internal: a jump target outside the cleanup chain");
		Node *call = node(ND_CALL); strcpy(call->name, e->fn); call->args = unary(ND_ADDR, ref(e->var));
		c = c->next = unary(ND_EXPRSTMT, call);
	}
	return h.next;
}
static Node *then_stmt(Node *list, Node *last) {   /* { list...; last } — or last alone */
	if (!list) return last;
	Node *b = node(ND_BLOCK), *c = list; while (c->next) c = c->next; c->next = last; b->body = list; return b;
}
static void push_cleanup(Node *var) {   /* a local just declared (and initialized) with cleanup(fn): its scope begins */
	if (!decl_attr.cleanup[0]) return;
	Cleanup *e = calloc(1, sizeof *e); e->var = var; strcpy(e->fn, decl_attr.cleanup); e->up = cleanups; cleanups = e;
}
static void no_cleanup(const char *what) { if (decl_attr.cleanup[0]) die("parse: cleanup on %s (line %d): only a local variable has a scope to leave", what, tk->line); }
static Node *loop_body(void) {   /* break and continue in it leave the cleanup scopes begun inside it */
	Cleanup *sb = brk_cleanups, *sc = cont_cleanups; brk_cleanups = cont_cleanups = cleanups;
	Node *b = stmt(); brk_cleanups = sb; cont_cleanups = sc; return b;
}
/* ---- always_inline expansion ---------------------------------------------------------------------------
 * We don't inline. But an always_inline function using __builtin_va_arg_pack() (it forwards its caller's variadic
 * arguments, whose register/stack layout depends on where they end up) or one with no out-of-line definition
 * (GNU `extern inline` + gnu_inline) must be inlined at every call, as GCC does even at -O0. Its tokens (from the
 * parameter list) are recorded, and each call re-parses them into the caller: the arguments bound to fresh locals
 * named as the parameters, the variadic ones to temporaries (the pack), `return e` a store + a jump to the end, the
 * labels renamed apart, the names resolved in the function's own (file) scope. */
static Node *inline_expand(const char *name, InlineDef *def, Node *args) {
	static int seq;
	if (!in_func) die("parse: a call to always_inline '%s' outside a function (line %d)", name, tk->line);
	for (InlineCtx *u = inline_ctx; u; u = u->up) if (!strcmp(u->name, name)) die("parse: recursive always_inline '%s' can't be inlined (line %d)", name, tk->line);
	InlineCtx cx = { .name = name, .ret = def->ret, .id = seq++, .up = inline_ctx };
	Token *resume = tk; Scope *outer = scope; int nl = nlocals; Type *fret = cur_fn_ret;
	scope = &file_scope; scope_push();                  /* its names are the file's + its own, not the caller's */
	Node h = {0}, *c = &h, *a = args;
	tk = def->params; expect("(");
	if (is("void") && !strcmp(tk->next->text, ")")) tk = tk->next;
	else if (!is(")")) do {
		if (consume("...")) {   /* the rest: the pack, each argument in a temporary of its promoted type */
			Node **tail = &c->next;
			for (; a; a = a->next) {
				Node *e = a; Type *t = type_of(e);
				t = t->kind == TY_ARRAY ? pointer_to(t->base) : t->kind == TY_FLOAT ? ty_double : t->kind < TY_LLONG && !is_fp(t) && t->size < 4 ? ty_int : t;
				cx.pack = realloc(cx.pack, (cx.npack + 1) * sizeof *cx.pack); cx.pack[cx.npack++] = bind(&tail, t, e);
			}
			while (c->next) c = c->next;
			break;
		}
		char p[64]; Type *ty = declarator(declspec(NULL, NULL), p);
		if (ty->kind == TY_ARRAY) ty = pointer_to(ty->base);
		if (!a) die("parse: too few arguments to always_inline '%s' (line %d)", name, resume->line);
		Node *e = a; a = a->next; e->next = NULL;
		add_local(p, ty); c = c->next = unary(ND_EXPRSTMT, binary(ND_ASSIGN, local_ref(p), e));
	} while (consume(","));
	expect(")");
	if (a && !cx.pack) die("parse: too many arguments to always_inline '%s' (line %d)", name, resume->line);
	cx.retvar = node(ND_VAR);   /* (void is char here: a void function's "result" is just unused) */ cx.retvar->offset = add_local("", def->ret); cx.retvar->type = def->ret;
	snprintf(cx.end, sizeof cx.end, ".inl%d.end", cx.id);
	int lid = inline_label_id; cur_fn_ret = def->ret; inline_ctx = &cx; inline_label_id = cx.id;
	if (!is("{")) die("parse: always_inline '%s' with K&R parameter declarations is not supported", name);
	c = c->next = stmt();                               /* the body, `{...}` */
	Node *end = node(ND_LABEL); strcpy(end->name, cx.end); jump_note(&fn_labels, &nfn_labels, &fn_labels_cap, end); c = c->next = end;
	inline_ctx = cx.up; cur_fn_ret = fret; inline_label_id = lid;
	scope_pop(); scope = outer; nlocals = nl; tk = resume; free(cx.pack);
	return stmtexpr_of(h.next, ref(cx.retvar));
}
static Node *va_pack_args(void) {   /* `__builtin_va_arg_pack ()` as a call's last arguments: the expansion's pack */
	tk = tk->next; expect("("); expect(")");
	if (!inline_ctx) { saw_va_pack = 1; Node *n = node(ND_CALL); strcpy(n->name, "__builtin_va_arg_pack"); return n; }   /* the definition's own
	                                                                                                   * parse: marks it, never emitted */
	Node h = {0}, *c = &h;
	for (int i = 0; i < inline_ctx->npack; i++) c = c->next = ref(inline_ctx->pack[i]);
	return h.next;
}
static int va_pack_ahead(void) { return tk->kind == TK_IDENT && !strcmp(tk->text, "__builtin_va_arg_pack") && tk->next && !strcmp(tk->next->text, "("); }
static Node *call_args(void) {   /* `(args)` after a callee (the "(" consumed): assign()s; a pack splices in */
	Node argh = {0}, *ac = &argh;
	if (!is(")")) do {
		if (va_pack_ahead()) { ac->next = va_pack_args(); while (ac->next) ac = ac->next; if (!is(")")) die("parse: __builtin_va_arg_pack () must be the last arguments (line %d)", tk->line); break; }
		ac = ac->next = assign();
	} while (consume(","));
	expect(")");
	return argh.next;
}
/* ---- GNU nested functions ----------------------------------------------------------------------------
 * A function defined inside another sees the enclosing functions' locals (and __label__ labels): it is compiled as a
 * file-local function `name.N` that receives the frame of the function it's defined in — its STATIC CHAIN — in ip
 * (as GCC does on ARM), kept in its first frame slot [r11, #-4]. An enclosing local is reached by walking the chain
 * (ND_VAR.chain hops); a call passes the frame the callee expects; a goto to an enclosing function's label restores
 * that function's frame and jumps (a non-local goto).
 *
 * Its ADDRESS is a TRAMPOLINE, as GCC makes on ARM: 16 bytes in the definer's frame, written at the definer's entry
 *     ldr ip, [pc, #0]    @ e59fc000: ip = the chain (pc reads 8 ahead)
 *     ldr pc, [pc, #0]    @ e59ff000: jump to the function
 *     .word <the definer's frame>, <name.N>
 * then __clear_cache over it (the runtime's: the cacheflush syscall), so a call through the pointer arrives with
 * the chain in ip like a direct call. Code on the stack: the object's .note.GNU-stack asks for an executable one. */
typedef struct Tramp { char *fname; int depth; Node **refs; int nrefs, cap; struct Tramp *next; } Tramp;
static Tramp *tramps;   /* taken addresses whose definer is still being parsed (innermost first) */
int exec_stack;         /* a trampoline was made: the stack must be executable */
static Node *nested_address(Ident *nf) {   /* `f` / `&f` for a nested function f: its trampoline in the definer's frame */
	if (!nf->tramp) { Tramp *t = calloc(1, sizeof *t); t->fname = nf->fname; t->depth = nf->depth; t->next = tramps; tramps = t; nf->tramp = t; }
	Tramp *t = nf->tramp;
	Node *v = node(ND_VAR); strcpy(v->name, "__trampoline"); v->type = ty_char; v->chain = fn_depth - t->depth;   /* offset: set once
	                                                                                * the definer's frame is laid out (tramps_emit) */
	if (t->nrefs == t->cap) t->refs = realloc(t->refs, (t->cap = t->cap ? 2 * t->cap : 4) * sizeof *t->refs);
	t->refs[t->nrefs++] = v;
	return unary(ND_ADDR, v);
}
/* End of a function body (fn_depth still its own): lay out the trampolines of the nested functions it defined whose
 * address was taken, and the statements that write them — to run first, at its entry. */
static Node *tramps_emit(void) {
	Node h = {0}, *c = &h;
	while (tramps && tramps->depth == fn_depth) {
		Tramp *t = tramps; tramps = t->next;
		int off = add_local_aligned("", array_of(ty_char, 16), 4);
		for (int i = 0; i < t->nrefs; i++) t->refs[i]->offset = off;
		#define WORD(k, e) do { Node *w = node(ND_VAR); strcpy(w->name, "__trampoline"); w->type = ty_uint; w->offset = off + 4 * (k); \
		                        c = c->next = unary(ND_EXPRSTMT, binary(ND_ASSIGN, w, cast_to(ty_uint, e))); } while (0)
		Node *fp = node(ND_CALL); strcpy(fp->name, "__builtin_frame_address"); fp->args = tnum(0, ty_uint); fp->type = pointer_to(ty_char);
		Node *fn = node(ND_GVAR); strcpy(fn->name, t->fname); fn->type = ty_char;
		WORD(0, tnum(0xe59fc000, ty_uint)); WORD(1, tnum(0xe59ff000, ty_uint)); WORD(2, fp); WORD(3, unary(ND_ADDR, fn));
		#undef WORD
		Node *lo = node(ND_VAR), *hi = node(ND_VAR); strcpy(lo->name, "__trampoline"); strcpy(hi->name, "__trampoline");
		lo->type = hi->type = ty_char; lo->offset = off; hi->offset = off + 16;
		Node *cl = node(ND_CALL); strcpy(cl->name, "__clear_cache"); cl->args = unary(ND_ADDR, lo); cl->args->next = unary(ND_ADDR, hi);
		if (!func_declared("__clear_cache")) { Type *vp[2] = { pointer_to(ty_char), pointer_to(ty_char) }; record_func_sig("__clear_cache", ty_char, vp, 2, 0); }   /* (void: no value used) */
		c = c->next = unary(ND_EXPRSTMT, cl);
		free(t->refs); free(t);
		exec_stack = 1;
	}
	return h.next;
}
static Func *function_tail(const char *name, Type *ret, Token *params);
static Func **nested_pend; static int nnested_pend, nested_pend_cap;   /* finished nested functions, outermost first */
static int nested_def_ahead(void) {   /* cursor at the parameter list's "(": is this a definition (a body or K&R decls follow)? */
	Token *save = tk; int d = 0;
	do { if (is("(")) d++; else if (is(")")) d--; tk = tk->next; } while (d && tk->kind != TK_EOF);
	while (is("__attribute__")) { tk = tk->next; if (is("(")) { int e = 0; do { if (is("(")) e++; else if (is(")")) e--; tk = tk->next; } while (e && tk->kind != TK_EOF); } }
	int def = is("{") || (is_typename() && !is("__attribute__"));
	tk = save; return def;
}
static int nested_seq;
static Ident *nested_forward(const char *name) {   /* a nested function's name in this scope (`auto` declared it ahead: the same one) */
	Ident *d = strmap_get(&scope->ids, name);
	if (d && d->kind == ID_NESTFN) return d;
	char sym[64]; snprintf(sym, sizeof sym, "%.50s.%d", name, nested_seq++);
	d = ident_add(name, ID_NESTFN); d->depth = fn_depth; d->fname = strdup(sym);
	return d;
}
static void nested_function(const char *name, Type *ret, Token *params) {
	char sym[64]; snprintf(sym, sizeof sym, "%s", nested_forward(name)->fname);   /* in scope already: it may recurse */
	/* the enclosing function's parse state, restored after */
	char fname[64]; strcpy(fname, cur_func_name); Type *fret = cur_fn_ret; int lb = local_bytes, nl = nlocals, sb = stmtexpr_body;
	Cleanup *c = cleanups, *cb = brk_cleanups, *cc = cont_cleanups, *ck = case_cleanups; Node *sw = cur_switch; Attr da = decl_attr;
	int at = nnested_pend;
	cur_switch = NULL; stmtexpr_body = 0;
	Func *f = function_tail(sym, ret, params);
	if (!f) die("parse: internal: nested function '%s' without a body", name);
	f->nested = 1; f->is_static = 1;
	strcpy(cur_func_name, fname); cur_fn_ret = fret; local_bytes = lb; nlocals = nl; stmtexpr_body = sb; in_func = 1;
	cleanups = c; brk_cleanups = cb; cont_cleanups = cc; case_cleanups = ck; cur_switch = sw; decl_attr = da;
	if (nnested_pend == nested_pend_cap) { nested_pend_cap = nested_pend_cap ? 2 * nested_pend_cap : 16; nested_pend = realloc(nested_pend, nested_pend_cap * sizeof *nested_pend); }
	memmove(nested_pend + at + 1, nested_pend + at, (nnested_pend - at) * sizeof *nested_pend);   /* before its own nested ones */
	nested_pend[at] = f; nnested_pend++;
}
/* After a function body: a goto out of cleanup scopes runs them first; into one is an error. */
static void resolve_goto_cleanups(int g0, int l0) {   /* this function's: gotos [g0, n), labels [l0, n) (an enclosing one's are below) */
	for (int i = g0; i < nfn_gotos; i++) {
		Node *g = fn_gotos[i].n; Cleanup *from = fn_gotos[i].at, *to = NULL; int found = 0;
		for (int k = l0; k < nfn_labels && !found; k++) if (!strcmp(fn_labels[k].n->name, g->name)) { to = fn_labels[k].at; found = 1; }
		if (!found || from == to) continue;
		Cleanup *e = from; while (e && e != to) e = e->up;
		if (e != to) die("parse: goto %s jumps into the scope of a cleanup variable", g->name);
		Node *jump = node(ND_GOTO); *jump = *g; jump->next = NULL;
		Node *nx = g->next; *g = *then_stmt(cleanup_calls(from, to), jump); g->next = nx;
	}
	nfn_gotos = g0; nfn_labels = l0;
}
/* A label / case / default and the statement it labels (none at a block's `}`: GCC) — a block marked LABELED, which a
 * statement list splices in flat (a switch's cases stay top-level markers); it stays a block where there's no list
 * (`do L: ; while (0)`, `switch (x) case 1: f();`). */
#define LABELED 1   /* ND_BLOCK.val */
static Node *append_stmt(Node *c, Node *s) {   /* link s after c (a LABELED block's statements join the list); the new tail */
	if (s->kind == ND_BLOCK && s->val == LABELED) { c->next = s->body; while (c->next) c = c->next; return c; }
	return c->next = s;
}
/* Multi-alternative constraints (`"=r,m"`, `"%0,0"`): GCC picks the cheapest alternative every operand can satisfy;
 * we take the first one that fits — a memory alternative needs an lvalue, an immediate-only one a constant. Each
 * operand's constraint becomes its modifiers ('=', '+', '&') + that alternative; '%' (commutative) is dropped. */
static void asm_pick_alternative(Node *n) {
	int nalt = 1;
	for (Node *o = n->args; o; o = o->next) { int k = 1; for (const char *c = o->cons; *c; c++) k += *c == ','; if (k > nalt) nalt = k; }
	if (nalt == 1) return;
	for (int a = 0; a < nalt; a++) {
		int ok = 1;
		for (Node *o = n->args; o && ok; o = o->next) {
			const char *c = o->cons; for (int k = 0; k < a && c; k++) { c = strchr(c, ','); if (c) c++; }
			if (!c) { ok = 0; break; }
			char alt[32]; size_t l = strcspn(c, ","); snprintf(alt, sizeof alt, "%.*s", (int)l, c);
			int mem = strpbrk(alt, "mQoV") != NULL, reg = strpbrk(alt, "rlg0123456789") != NULL, imm = strpbrk(alt, "inI") != NULL, cst = 1;
			if (imm) eval_try(o, &cst);
			if (!(reg || (mem && is_lval(o)) || (imm && cst))) ok = 0;
		}
		if (!ok) continue;
		for (Node *o = n->args; o; o = o->next) {
			char mods[8] = ""; size_t m = 0; const char *c = o->cons;
			while (*c == '=' || *c == '+' || *c == '&' || *c == '%') { if (*c != '%' && m < 6) mods[m++] = *c; c++; }
			mods[m] = 0;
			for (int k = 0; k < a; k++) { c = strchr(c, ','); c++; }
			char alt[32]; size_t l = strcspn(c, ","); snprintf(alt, sizeof alt, "%.*s", (int)l, c);
			char *pct; while ((pct = strchr(alt, '%'))) memmove(pct, pct + 1, strlen(pct));
			snprintf(o->cons, sizeof o->cons, "%s%s", mods, alt);
		}
		return;
	}
	die("parse: no alternative of an asm's constraints fits its operands (line %d)", tk->line);
}
static int label_chain;   /* stmt() is reading the statement after a label: another label is returned bare (no recursion
                          * per label — `a: b: c: ...` chains can be 100000 long) */
/* An immediate-only constraint: i (any constant: an integer or an address), n (an integer), s (an address), ARM's
 * I-N (integers in some range) — no register or memory alternative. */
static int asm_imm_constraint(const char *c) { return strpbrk(c, "insIJKLMN") && !strpbrk(c, "rlgmQoV0123456789"); }
static int as_addr_const(Node *e, char *sym, long *ad);
static void asm_immediate(Node *op) {
	int ok = 1; long v = eval_try(op, &ok);
	if (ok && !strchr(op->cons, 's')) { op->val = v; return; }
	char sym[SYMEXPR_MAX] = ""; long ad = 0;
	if (!strchr(op->cons, 'n') && as_addr_const(op, sym, &ad)) {
		char buf[SYMEXPR_MAX + 24]; if (ad) snprintf(buf, sizeof buf, "%s%+ld", sym, ad); else snprintf(buf, sizeof buf, "%s", sym);
		op->imm_sym = strdup(buf); return;
	}
	op->imm_bad = 1;
}
static Node *labeled(Node *lab) {
	if (label_chain) return lab;
	Node *b = node(ND_BLOCK); b->val = LABELED; b->body = lab;
	for (Node *c = lab; !is("}"); ) {
		label_chain = 1; Node *st = stmt(); label_chain = 0;
		c = append_stmt(c, st);
		if (st->kind != ND_LABEL && st->kind != ND_CASE) break;   /* the labeled statement itself */
	}
	return b;
}
static Node *stmt(void) {
	int chain = label_chain; label_chain = 0;               /* (only this call: nested statements are ordinary) */
	if (chain && !(is("case") || is("default") || (tk->kind == TK_IDENT && tk->next && !strcmp(tk->next->text, ":")))) chain = 0;
	label_chain = chain;
	if (consume(";")) { label_chain = 0; return node(ND_BLOCK); }   /* empty statement (e.g. `while (...) ;`) */
	if (!chain) label_chain = 0;
	if (consume("switch")) {                                 /* switch (e) body ; cases attach to it */
		Node *n = node(ND_SWITCH); expect("("); n->cond = expr(); expect(")");
		Node *save = cur_switch; Cleanup *sb = brk_cleanups, *sk = case_cleanups; cur_switch = n; brk_cleanups = case_cleanups = cleanups;
		n->then = stmt(); cur_switch = save; brk_cleanups = sb; case_cleanups = sk;
		fold_const_switch(n);
		return n;
	}
	if (consume("case")) {                                   /* case CONST: */
		if (!cur_switch) die("parse: 'case' outside switch");
		if (cleanups != case_cleanups) die("parse: a case label in the scope of a cleanup variable (line %d)", tk->line);
		Node *n = node(ND_CASE); n->val = eval_const(conditional());   /* folds casts etc: `case (blk_status_t)1:` */
		if (consume("...")) { n->val2 = eval_const(conditional()); n->is_range = 1; }   /* GCC `case lo ... hi:` */
		expect(":");
		n->case_next = cur_switch->case_list; cur_switch->case_list = n;
		return labeled(n);
	}
	if (consume("default")) { if (!cur_switch) die("parse: 'default' outside switch"); expect(":");
		if (cleanups != case_cleanups) die("parse: a default label in the scope of a cleanup variable (line %d)", tk->line);
		Node *n = node(ND_CASE); n->is_default = 1; n->case_next = cur_switch->case_list; cur_switch->case_list = n; return labeled(n); }
	if (consume("break"))    { expect(";"); return then_stmt(cleanup_calls(cleanups, brk_cleanups), node(ND_BREAK)); }
	if (consume("continue")) { expect(";"); return then_stmt(cleanup_calls(cleanups, cont_cleanups), node(ND_CONTINUE)); }
	if (is("asm")) {                        /* __asm__ volatile("tmpl" : outs : ins : clobbers); */
		tk = tk->next; consume("volatile"); int is_goto = consume("goto");
		expect("("); Node *n = node(ND_ASM);
		char buf[4096]; size_t bl = 0; buf[0] = 0;           /* template: concatenate adjacent string literals */
		while (tk->kind == TK_STR) { size_t l = strlen(tk->sval); if (bl + l >= sizeof buf) die("parse: asm template too long (>%d)", (int)sizeof buf); memcpy(buf + bl, tk->sval, l); bl += l; buf[bl] = 0; tk = tk->next; }
		n->asm_tmpl = malloc(bl + 1); memcpy(n->asm_tmpl, buf, bl + 1);
		Node oh = {0}, *oc = &oh; int nouts = 0;
		n->asm_basic = !is(":");   /* GCC: only EXTENDED asm (has `:`) interprets %; basic asm is verbatim */
		char opn[16][32]; int nn = 0;                        /* per-operand [name] (by position: outputs then inputs) */
		for (int i = 0; i < 16; i++) opn[i][0] = 0;
		if (consume(":")) while (tk->kind == TK_STR || is("[")) {   /* outputs: [name] "constraint"(lvalue) */
			if (consume("[")) { if (nn < 16) strncpy(opn[nn], tk->text, 31); tk = tk->next; expect("]"); }
			char c[32]; if (tk->kind != TK_STR || strlen(tk->sval) >= sizeof c) die("parse: an asm constraint must be a string under %d chars (line %d)", (int)sizeof c, tk->line);
			strcpy(c, tk->sval); tk = tk->next;
			expect("("); Node *op = assign(); expect(")"); strcpy(op->cons, c);
			oc = oc->next = op; nouts++; nn++; if (!consume(",")) break;
		}
		if (consume(":")) while (tk->kind == TK_STR || is("[")) {   /* inputs: [name] "constraint"(expr) */
			if (consume("[")) { if (nn < 16) strncpy(opn[nn], tk->text, 31); tk = tk->next; expect("]"); }
			char c[32]; if (tk->kind != TK_STR || strlen(tk->sval) >= sizeof c) die("parse: an asm constraint must be a string under %d chars (line %d)", (int)sizeof c, tk->line);
			strcpy(c, tk->sval); tk = tk->next;
			expect("("); Node *op = assign(); expect(")"); strcpy(op->cons, c);
			if (asm_imm_constraint(op->cons)) asm_immediate(op);   /* immediate: fold now, substitute the constant */
			oc = oc->next = op; nn++; if (!consume(",")) break;
		}
		if (consume(":")) while (tk->kind == TK_STR) {   /* clobbers: operand registers must avoid them; callee-saved ones get preserved */
			const char *c = tk->sval; int r = -1;
			if ((c[0] == 'r' || c[0] == 'R') && isdigit((unsigned char)c[1])) { r = atoi(c + 1); if (r > 15) r = -1; }
			else if (!strcmp(c, "ip")) r = 12; else if (!strcmp(c, "lr")) r = 14; else if (!strcmp(c, "fp")) r = 11;
			else if (!strcmp(c, "sl")) r = 10; else if (!strcmp(c, "sb")) r = 9; else if (!strcmp(c, "sp")) r = 13; else if (!strcmp(c, "pc")) r = 15;
			else if (strcmp(c, "cc") && strcmp(c, "memory")) die("parse: unknown asm clobber \"%s\" (line %d)", c, tk->line);
			if (r == 11 || r == 13 || r == 15) die("parse: asm clobbers %s, which this compiler can't preserve (line %d)", c, tk->line);
			if (r >= 0) n->asm_clobber |= 1 << r;
			tk = tk->next; if (!consume(",")) break;
		}
		/* asm goto: the labels it may jump to (numbered after the operands: %l<nn+k>, or %l[name]) — ND_LABELADDRs in body */
		char lab[16][64]; int nlab = 0; Node lh = {0}, *lc = &lh;
		if (is_goto && consume(":")) do {
			if (nlab == 16) die("parse: too many asm goto labels (line %d)", tk->line);
			Node *l = node(ND_LABELADDR); ident(l->name); strcpy(lab[nlab++], l->name); int li = map_label(l->name);
			if (li >= 0 && lscope[li].depth < fn_depth) die("parse: asm goto to an enclosing function's label (line %d)", tk->line);
			lc = lc->next = l;
		} while (consume(","));
		if (nlab && cleanups) die("parse: asm goto in the scope of a cleanup variable (line %d)", tk->line);
		n->body = lh.next;
		expect(")"); expect(";");
		{   /* rewrite %[name] -> %N (operand position) and %l[label] -> %l<N> so gen's %N substitution handles names */
			size_t cap = 2 * bl + 64, k = 0; char *rw = malloc(cap); const char *s = n->asm_tmpl;
			while (*s) {
				int isl = s[0] == '%' && s[1] == 'l' && s[2] == '[';
				if (s[0] == '%' && (s[1] == '[' || isl)) {
					const char *b0 = s + (isl ? 3 : 2), *e = strchr(b0, ']');
					if (e) { char nm[64]; size_t l = (size_t)(e - b0); if (l > 63) l = 63; memcpy(nm, b0, l); nm[l] = 0;
						int idx = -1;
						if (isl) { for (int i = 0; i < nlab; i++) if (!strcmp(lab[i], nm)) { idx = nn + i; break; } }
						else for (int i = 0; i < nn; i++) if (opn[i][0] && !strcmp(opn[i], nm)) { idx = i; break; }
						if (idx < 0) die("parse: asm operand %%%s[%s] names nothing (line %d)", isl ? "l" : "", nm, tk->line);
						k += (size_t)snprintf(rw + k, cap - k, isl ? "%%l%d" : "%%%d", idx); s = e + 1; continue; }
				}
				rw[k++] = *s++;
			}
			rw[k] = 0; n->asm_tmpl = rw;
		}
		n->args = oh.next; n->val = nouts;                   /* operands: outputs first, then inputs; val = #outputs */
		asm_pick_alternative(n);
		return n;
	}
	if (consume("__label__")) {   /* GNU local label: `__label__ a, b;` scopes a/b to the enclosing block (each
		                              * expansion of a macro like wait_event gets its OWN `__out:`) — rename uniquely */
		do { if (tk->kind == TK_IDENT) {
			if (nlscope >= 512) die("parse: too many __label__ declarations in scope (>512)");
			strncpy(lscope[nlscope].from, tk->text, 63); snprintf(lscope[nlscope].to, 64, "%.40s.%d", tk->text, ++lscope_seq);
			strcpy(lscope[nlscope].owner, cur_func_name); lscope[nlscope].depth = fn_depth; nlscope++;
			tk = tk->next; } } while (consume(","));
		expect(";"); return node(ND_BLOCK);
	}
	if (tk->kind == TK_IDENT && !strcmp(tk->text, "_Static_assert")) {   /* block-scope _Static_assert (e.g. in container_of's stmt-expr) — skip */
		tk = tk->next; skip_parens(); consume(";"); return node(ND_BLOCK);
	}
	if (consume("goto"))     { Node *n = node(ND_GOTO);
		if (consume("*")) {                                 /* GNU computed goto: `goto *p;` (p from &&label) */
			n->lhs = expr();
			if (cleanups) die("parse: a computed goto in the scope of a cleanup variable (line %d)", tk->line);
		}
		else {
			ident(n->name); int li = map_label(n->name);
			if (li >= 0 && lscope[li].depth < fn_depth) {   /* a non-local goto: out of this nested function into the one whose label it is */
				if (cleanups) die("parse: a non-local goto out of the scope of a cleanup variable (line %d)", tk->line);
				n->owner = strdup(lscope[li].owner); n->chain = fn_depth - lscope[li].depth;
			} else jump_note(&fn_gotos, &nfn_gotos, &fn_gotos_cap, n);
		}
		expect(";"); return n; }
	if (tk->kind == TK_IDENT && tk->next && tk->next->kind == TK_PUNCT && !strcmp(tk->next->text, ":")) {   /* label: */
		Node *n = node(ND_LABEL); ident(n->name); int li = map_label(n->name); expect(":");
		if (li >= 0 && lscope[li].depth < fn_depth) die("parse: label '%s' is an enclosing function's __label__ (line %d)", lscope[li].from, tk->line);
		jump_note(&fn_labels, &nfn_labels, &fn_labels_cap, n); return labeled(n);
	}
	if (consume("return")) {   /* `return;` allowed */
		Node *n = node(ND_RETURN); if (!is(";")) n->lhs = expr(); expect(";");
		if (inline_ctx) {   /* in an always_inline expansion: the result, then to its end */
			Node *g = node(ND_GOTO); strcpy(g->name, inline_ctx->end); jump_note(&fn_gotos, &nfn_gotos, &fn_gotos_cap, g);
			if (!n->lhs) return g;
			Node *v = is_cplx(cur_fn_ret) || is_cplx(type_of(n->lhs)) ? cplx_convert(cur_fn_ret, n->lhs) : n->lhs;
			return then_stmt(unary(ND_EXPRSTMT, binary(ND_ASSIGN, ref(inline_ctx->retvar), v)), g);
		}
		if (n->lhs && is_vec(cur_fn_ret)) vec_assign_ok(cur_fn_ret, type_of(n->lhs), "a return");
		if (n->lhs && (is_cplx(cur_fn_ret) || is_cplx(type_of(n->lhs)))) n->lhs = cplx_convert(cur_fn_ret, n->lhs);
		if (!cleanups) return n;
		Node *h = NULL, **t = &h;                            /* the value first, then the cleanups, then the return */
		if (n->lhs) n->lhs = ref(bind(&t, cur_fn_ret, n->lhs));
		*t = cleanup_calls(cleanups, NULL);
		return then_stmt(h, n);
	}
	if (consume("if")) {
		Node *n = node(ND_IF); expect("("); n->cond = expr(); expect(")"); n->then = stmt(); if (consume("else")) n->els = stmt();
		/* Constant condition: keep only the taken branch (what GCC's DCE does). The kernel's BUILD_BUG_ON is
		 * `if (!(const)) __compiletime_assert_N();` — the call must vanish, else it's an undefined symbol at
		 * link. Only fold when the dead branch has no label/case a goto or switch could still jump into. */
		int ok = 1; long c = eval_try(n->cond, &ok);
		if (ok) { Node *dead = c ? n->els : n->then, *live = c ? n->then : n->els;
			if (!has_jump_target(dead)) return live ? live : node(ND_BLOCK);
			/* A label/case inside keeps the dead branch, but the statements BEFORE the first one are unreachable:
			 * drop them, as GCC does (`if (0) { link_error(); case 1: ... }` must not reference link_error). */
			if (dead->kind == ND_BLOCK) while (dead->body && !node_has_jump_target(dead->body)) dead->body = dead->body->next; }
		return n;
	}
	if (consume("while")) { Node *n = node(ND_WHILE); expect("("); n->cond = expr(); expect(")"); n->body = loop_body(); return n; }
	if (consume("do")) { Node *n = node(ND_DOWHILE); n->body = loop_body(); expect("while"); expect("("); n->cond = expr(); expect(")"); expect(";"); return n; }
	if (consume("for")) {                                    /* for (init; cond; inc) body — any part may be empty */
		Node *n = node(ND_FOR); expect("("); int saved_nl = nlocals; scope_push();   /* the init declaration's scope is the for statement */
		Cleanup *c0 = cleanups;
		if (is_typename()) n->init = stmt();       /* declaration eats its own ; */
		else if (!consume(";")) { n->init = unary(ND_EXPRSTMT, expr()); expect(";"); }
		if (!consume(";")) { n->cond = expr(); expect(";"); }
		if (!is(")")) n->inc = expr();
		expect(")"); n->body = loop_body(); nlocals = saved_nl; scope_pop();
		if (cleanups == c0) return n;
		Node *calls = cleanup_calls(cleanups, c0); cleanups = c0;   /* the init's cleanup variables: their scope is the loop */
		Node *b = node(ND_BLOCK); b->body = n; n->next = calls; return b;
	}
	if (consume("{")) { int saved_ls = nlscope, saved_nl = nlocals; scope_push();   /* a block is a scope: its declarations end at `}` */
		int value = stmtexpr_body; stmtexpr_body = 0; Cleanup *c0 = cleanups;
		Node *n = node(ND_BLOCK); Node h = {0}, *c = &h, *last = NULL; while (!consume("}")) c = append_stmt(c, stmt());
		if (cleanups != c0) {   /* leaving the block: its cleanups — after a ({...})'s value is computed */
			if (value && c->kind == ND_EXPRSTMT && c->lhs) {
				for (last = &h; last->next != c; last = last->next) ;
				Node *h2 = NULL, **t2 = &h2, *v = bind(&t2, NULL, c->lhs);
				last->next = h2; c = h2;
				c->next = cleanup_calls(cleanups, c0); while (c->next) c = c->next;
				c = c->next = unary(ND_EXPRSTMT, ref(v));
			} else { c->next = cleanup_calls(cleanups, c0); }
			cleanups = c0;
		}
		n->body = h.next;
		nlscope = saved_ls; nlocals = saved_nl; scope_pop(); return n; }
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
		decl_attr = (Attr){0};
		int td, sc = 0; Type *base = declspec(&td, &sc);
		Attr battr = decl_attr;
		if (td) { typedef_decl(base, battr); Node *b = node(ND_BLOCK); b->body = take_vla_pending(); return b; }   /* a VLA typedef's size is computed HERE */
		if (consume(";")) { Node *b = node(ND_BLOCK); b->body = take_vla_pending(); return b; }   /* type-only (e.g. a struct definition; a
		                                                                                         * variable-size one's layout runs here) */
		Node blk = {0}, *bc = &blk;                          /* each initializer becomes a statement in a block */
		do {
			decl_attr = battr;
			char nm[64]; Type *ty = declarator(base, nm);
			if (is("(") && nested_def_ahead()) {   /* a GNU nested function: a definition ends the declaration */
				nested_function(nm, ty, NULL);
				Node *n = node(ND_BLOCK); n->body = blk.next; return n;
			}
			if (ty->fn_ret && decl_fparams && is("{")) {   /* ...declared through a group: `int (*f(void))(int) {` */
				nested_function(nm, ty->fn_ret, decl_fparams);
				Node *n = node(ND_BLOCK); n->body = blk.next; return n;
			}
			if (ty->fn_ret) {   /* a local prototype through a group, or a function typedef: `fn_t f;` */
				record_func_sig(nm, ty->fn_ret, ty->params, ty->nparams, ty->variadic);
				while (consume("__attribute__")) attribute();
				no_type_attrs(); no_cleanup("a function"); sig_attrs(nm, &decl_attr);
				continue;
			}
			if (is("(") && (sc & SC_AUTO)) {   /* `auto T f(params);`: a nested function declared ahead of its definition */
				Type **pts; int np = 0; proto_params(&pts, &np); free(pts);
				while (consume("__attribute__")) attribute();
				no_type_attrs(); nested_forward(nm);
				continue;
			}
			if (is("(")) {   /* local function prototype `T name(params);` — record it, no local variable */
				Type **pts; int np = 0, va = proto_params(&pts, &np);   /* the parameter TYPES decide how calls pass FP args */
				record_func_sig(nm, ty, pts, np, va);
				while (consume("__attribute__")) attribute();   /* trailing: `void h(void) __attribute__((error("...")))` */
				no_type_attrs(); no_cleanup("a function");
				sig_attrs(nm, &decl_attr);
				continue;
			}
			if ((sc & SC_TLS) && !(sc & (SC_STATIC | SC_EXTERN))) die("parse: block-scope _Thread_local '%s' must be static or extern (line %d)", nm, tk->line);
			if (sc & (SC_STATIC | SC_EXTERN)) {   /* block-scope static/extern: a GLOBAL object, only the NAME is block-scoped */
				Gvar *g;
				if ((sc & SC_STATIC) && inline_ctx) die("parse: a static local in always_inline '%s', expanded at each call, is not supported (line %d)", inline_ctx->name, tk->line);
				if (sc & SC_STATIC) {             /* own file-local object `nm.N` (GCC's naming); initialized once, statically */
					static int sseq; char sn[80]; snprintf(sn, sizeof sn, "%.60s.%d", nm, sseq++); g = new_global(sn); g->is_static = 1; g->type = ty;
				} else if (!(g = global_find(nm))) { g = new_global(nm); g->type = ty; g->is_extern = 1; }
				if (consume("asm")) asm_label_for(g->name);   /* its symbol (a static's own nm.N, an extern's nm) */
				while (consume("__attribute__")) attribute();
				no_type_attrs(); no_cleanup("a static or extern object");
				attr_merge(&g->attr, &decl_attr);
				if (sc & SC_TLS) g->is_tls = 1;
				add_local_at(nm, ty, 0); strncpy(locals[nlocals - 1].gname, g->name, 63);   /* bound BEFORE the initializer: it may name itself (&x.head) */
				if ((sc & SC_STATIC) && consume("=")) g->init = global_init(ty);   /* sizes an unsized array in place */
				continue;
			}
			for (Node *v = take_vla_pending(); v; ) { Node *nx = v->next; v->next = NULL; bc = bc->next = v; v = nx; }   /* this declarator's VLA sizes */
			if (ty->vsize_off) {   /* a VLA object: its slot holds an alloca'd block, behind a mark (re-running this frees the last one) */
				if (is("=")) die("parse: a variable-length array cannot be initialized (line %d)", tk->line);
				no_cleanup("a variable-length array");
				Type *pt = pointer_to(ty->kind == TY_ARRAY ? ty->base : ty);   /* an array's decays; a variable-size struct's address */
				int off = add_local(nm, pt); locals[nlocals - 1].type = ty; locals[nlocals - 1].vla = 1;
				bc = bc->next = node(ND_VLAMARK);
				Node *pv = node(ND_VAR); strncpy(pv->name, nm, 63); pv->offset = off; pv->type = pt;
				int al = align_of(ty) > decl_attr.align ? align_of(ty) : decl_attr.align;   /* alloca gives 8: more is realigned */
				Node *a = node(ND_CALL); strcpy(a->name, "__builtin_alloca"); a->args = al > 8 ? binary(ND_ADD, vsize_node(ty), tnum(al, ty_uint)) : vsize_node(ty);
				Node *blk = al > 8 ? cast_to(pt, align_up_node(cast_to(ty_uint, a), al)) : a;
				bc = bc->next = unary(ND_EXPRSTMT, binary(ND_ASSIGN, pv, blk));
				continue;
			}
			if (ty->kind == TY_ARRAY && ty->len == 0 && consume("=")) {   /* unsized `T x[] = ...`: the initializer sizes it, THEN allocate */
				InitPlace *pl = init_places(ty);
				int off = add_local(nm, ty);
				Node *v = node(ND_VAR); strncpy(v->name, nm, 63); v->offset = off; v->type = ty;
				bc = bc->next = lower_local(v, pl, ty->size);
				push_cleanup(v);
				continue;
			}
			int al = align_of(ty) > decl_attr.align ? align_of(ty) : decl_attr.align;
			if (al > 8) {   /* over-aligned past the frame's 8: a pointer slot, into an area realigned at run time */
				Type *at = array_of(ty_char, ty->size + al);
				Node *area = node(ND_VAR); area->offset = add_local("", at); area->type = at;
				Node *pv = node(ND_VAR); pv->offset = add_local(nm, pointer_to(ty)); pv->type = pointer_to(ty);
				locals[nlocals - 1].type = ty; locals[nlocals - 1].vla = 1;
				Node *a = binary(ND_BITAND, binary(ND_ADD, cast_to(ty_uint, area), tnum(al - 1, ty_uint)), tnum(-al, ty_uint));
				bc = bc->next = unary(ND_EXPRSTMT, binary(ND_ASSIGN, pv, cast_to(pointer_to(ty), a)));
			} else add_local_aligned(nm, ty, al);
			if (consume("asm")) { expect("("); strncpy(locals[nlocals - 1].reg, tk->text, 7); tk = tk->next; expect(")"); }   /* register var (both spellings) */
			if (consume("=")) { Node *v = local_ref(nm);
				if (is("{") || ty->kind == TY_ARRAY) bc = bc->next = init_of(v, ty);   /* aggregate initializer (incl. char a[N] = "str") */
				else { Node *r = assign(); if (is_vec(ty)) vec_assign_ok(ty, type_of(r), "an initializer"); if (is_cplx(ty) || is_cplx(type_of(r))) r = cplx_convert(ty, r);
					bc = bc->next = unary(ND_EXPRSTMT, binary(ND_ASSIGN, v, r)); } }
			if (decl_attr.cleanup[0]) push_cleanup(local_ref(nm));   /* its scope begins, initialized */
		} while (consume(","));
		expect(";");
		Node *n = node(ND_BLOCK); n->body = blk.next; return n;
	}
	Node *n = unary(ND_EXPRSTMT, expr()); expect(";"); return n;
}

/* `typedef T a, *b, c[N], d(params);` (declspec already read): each declarator names a type; `name(params)` is
 * a FUNCTION type; aligned(N) makes an over-aligned copy. */
static void typedef_decl(Type *base, Attr battr) {
	do {
		decl_attr = battr;
		char nm[64]; Type *ty = declarator(base, nm);
		if (is("(")) ty = func_proto(ty);
		while (consume("__attribute__")) attribute();
		no_type_attrs(); no_cleanup("a typedef");
		add_typedef(nm, aligned_type(ty, decl_attr.align));
	} while (consume(","));
	expect(";");
}

/* A prototype's parameter list `( ... )` (cursor at "("), types only: returns 1 for `...`, 2 for `()` (unknown
 * parameters — callers apply the default promotions), else 0. Arrays/functions adjust to pointers. */
static int proto_params(Type ***ptsp, int *np) {   /* *ptsp: a fresh array of the types (the caller's) */
	Node *vla_mark = vla_pt; int cap = 8; Type **pts = *ptsp = malloc(cap * sizeof *pts);
	expect("("); *np = 0;
	if (consume(")")) return 2;
	if (is("void") && !strcmp(tk->next->text, ")")) { tk = tk->next->next; return 0; }
	int va = 0; in_proto++; in_params++;
	do {
		if (consume("...")) { va = 1; break; }
		char p[64]; Type *ty = declarator(declspec(NULL, NULL), p);
		if (is("(")) ty = pointer_to(func_proto(ty));
		else if (ty->fn_ret) ty = pointer_to(ty);
		if (ty->kind == TY_ARRAY) ty = pointer_to(ty->base);
		if (*np == cap) pts = *ptsp = realloc(pts, (cap *= 2) * sizeof *pts);
		pts[(*np)++] = ty;
	} while (consume(","));
	expect(")"); in_proto--; in_params--;
	vla_mark->next = NULL; vla_pt = vla_mark;   /* its own parameters' VLA bounds are never evaluated (outer ones stay queued) */
	return va;
}

/* ---- functions ----------------------------------------------------------------------------------- */
/* The name + return type have already been read; the cursor is at "(". Parse params + body. */
/* `register T x asm("rN")` at file scope (the `asm` consumed): a global register variable. */
static void global_register(const char *name) {
	expect("("); char s[8]; strncpy(s, tk->text, 7); s[7] = 0; if (tk->kind != TK_STR) die("parse: a register variable needs a register name (line %d)", tk->line);
	tk = tk->next; expect(")");
	if (ngregs == 16) die("parse: too many global register variables (line %d)", tk->line);
	strncpy(gregs[ngregs].name, name, 63); strncpy(gregs[ngregs].reg, s, 7); ngregs++;
}
/* The name + return type have been read; `params` is the parameter list — NULL: at the cursor; else one a grouped
 * declarator already passed (`void (*f(void))(float) {`: the cursor is at the body). */
static Func *function_tail(const char *name, Type *ret, Token *params) {
	Token *resume = params ? tk : NULL; if (params) tk = params;
	Token *params_tk = tk; saw_va_pack = 0;                 /* (an always_inline one is re-parsed from here at each call) */
	strncpy(cur_func_name, name, sizeof cur_func_name - 1);   /* for `__func__` inside the body */
	Func *f = calloc(1, sizeof *f); strncpy(f->name, name, 63); f->ret_type = ret; cur_fn_ret = ret; f->final_frame = -1;
	int nested = fn_depth > 0;                               /* a GNU nested function (see nested_function) */
	in_func = 1; fn_depth++;
	if (!nested) nlocals = 0;                                /* a nested one's locals follow its definer's (both visible) */
	local_bytes = 0;
	if (nested) add_local("", ty_uint);                     /* [r11, #-4]: the static chain */
	scope_push();                                            /* the function's scope: its parameters and body */
	expect("(");
	struct { char name[64]; Type *ty; } *prm = NULL; int np = 0, pcap = 0;   /* collect params, then assign offsets by kind */
	Aapcs early; int early_sv = 0;                           /* their provisional placement, one at a time */
	Attr fattr = decl_attr;   /* parameter attributes are the params' own */
	int unproto = is(")");   /* `f()`: parameters unknown to callers (default promotions) */
	if (is("void") && !strcmp(tk->next->text, ")")) tk = tk->next;   /* (void) = no params */
	else if (!is(")")) {
		in_params++;
		do {
			if (consume("...")) { f->variadic = 1; break; }   /* `...` */
			char p[64]; Type *ty = declarator(declspec(NULL, NULL), p);
			no_cleanup("a parameter");
			if (is("(")) ty = pointer_to(func_proto(ty));   /* function-typed param `R name(args)` -> function pointer */
			else if (ty->fn_ret) ty = pointer_to(ty);                  /* param typed with a function typedef -> function pointer */
			if (ty->kind == TY_ARRAY) ty = pointer_to(ty->base);   /* array param decays to pointer */
			if (np == pcap) prm = realloc(prm, (pcap = pcap ? 2 * pcap : 8) * sizeof *prm);
			strncpy(prm[np].name, p, 63); prm[np].name[63] = 0; prm[np].ty = ty; np++;
			{   /* visible at once — a later parameter's VLA bound may use it (`int n, int a[n]`); a param's AAPCS
			     * position depends only on the ones before it. (K&R retyping rebinds all of them below.) */
				if (np == 1) { early = aapcs_start(is_sret(ret, !soft_float), !soft_float); early_sv = 0; }
				int pos, vr; aapcs_place(&early, ty, &pos, &vr); early_sv |= vr >= 0;
				if (p[0]) add_local_at(p, ty, vr >= 0 ? 8 + 4 * vr : 8 + 4 * pos + (early_sv ? 64 : 0));
			}
		} while (consume(","));
		in_params--;
	}
	expect(")");
	if (resume) tk = resume;
	/* K&R definition `f(a, b) int a; char *b; { ... }`: the identifier list parsed as int params; the
	 * declarations before the body give their real types (array/function params decay to pointers). */
	while (is_typename() && !is("__attribute__")) {
		Type *kb = declspec(NULL, NULL);
		do {
			char kn[64]; Type *kt = declarator(kb, kn);
			if (is("(")) kt = pointer_to(func_proto(kt));
			if (kt->kind == TY_ARRAY) kt = pointer_to(kt->base);
			int k = 0; while (k < np && strcmp(prm[k].name, kn)) k++;
			if (k == np) die("parse: K&R declaration of '%s', which is not a parameter of %s", kn, name);
			prm[k].ty = kt;
		} while (consume(","));
		expect(";");
	}
	decl_attr = fattr;
	if (consume("asm")) { if (nested) die("parse: an asm label on a nested function (line %d)", tk->line); asm_label_for(name); }   /* `int f(void) __asm__("sym");` */
	while (consume("__attribute__")) attribute();       /* trailing: int f(void) __attribute__((noreturn)) { … } — before the binding (pcs) */
	no_type_attrs(); no_cleanup("a function");
	Node *vla_prologue = take_vla_pending();   /* VLA parameter bounds (`int a[n][m]`, `x[i++]`): evaluated at entry */
	f->nparams = np;
	{ Type **pts = malloc((np ? np : 1) * sizeof *pts); for (int i = 0; i < np; i++) pts[i] = prm[i].ty; record_func_sig(name, ret, pts, np, f->variadic ? 1 : unproto ? 2 : 0); free(pts); }   /* publish the signature for callers */
	sig_attrs(name, &decl_attr);
	sig_align(name, decl_attr.align);                     /* aligned(N) on any declaration applies to the definition */
	/* Bind params per AAPCS (aapcs_layout, the same placement callers use; an sret function's hidden buffer
	 * pointer takes word 0). gen_func homes r0..r3 right above the frame record, contiguous with the caller's
	 * stack args, so a core/stack param lives at [r11, #8 + 4*word]; varargs begin after the last fixed word.
	 * Under AAPCS-VFP, params arriving in s0-s15 are saved (vpush d0-d7) just BELOW the homed r0-r3 (which stay
	 * contiguous with the stack args — a struct may be split across r3 and the stack): an s-register k param
	 * is at [r11, #8 + 4k] and every core/stack word moves up by the 64 bytes. */
	{
		Type **pts = malloc((np ? np : 1) * sizeof *pts); int *pos = malloc((np ? np : 1) * sizeof *pos), *vr = malloc((np ? np : 1) * sizeof *vr);
		for (int i = 0; i < np; i++) pts[i] = prm[i].ty;
		f->vfp = !soft_float && !func_base_pcs(name);   /* base PCS: variadic, or pcs("aapcs") on this or an earlier declaration */
		f->nfixed_words = aapcs_layout(pts, np, is_sret(ret, f->vfp), f->vfp, pos, vr);
		for (int i = 0; i < np; i++) f->vfp_save |= vr[i] >= 0;
		for (int i = 0; i < np; i++) if (prm[i].name[0])
			add_local_at(prm[i].name, prm[i].ty, vr[i] >= 0 ? 8 + 4 * vr[i] : 8 + 4 * pos[i] + (f->vfp_save ? 64 : 0));
		free(pts); free(pos); free(vr);
	}
	free(prm);
	f->attr = decl_attr;                                     /* the function's own; the body's declarations reset decl_attr */
	f->attr.align = sig_align(name, 0);                      /* ...its alignment: the largest any declaration asked for */
	if (is(";") || is(",")) { in_func = 0; fn_depth--; scope_pop(); return NULL; }   /* a prototype (the declaration may go on) — no body to compile */
	expect("{");
	Node h = {0}, *c = &h;
	for (Node *v = vla_prologue; v; ) { Node *nx = v->next; v->next = NULL; c = c->next = v; v = nx; }
	cleanups = brk_cleanups = cont_cleanups = case_cleanups = NULL; int g0 = nfn_gotos, l0 = nfn_labels;
	while (!consume("}")) c = append_stmt(c, stmt());
	if (cleanups) { c->next = cleanup_calls(cleanups, NULL); cleanups = NULL; }   /* falling off the end */
	resolve_goto_cleanups(g0, l0);
	Node *tr = tramps_emit();                                /* its nested functions' trampolines, written first */
	if (tr) { Node *e = tr; while (e->next) e = e->next; e->next = h.next; h.next = tr; }
	f->body = h.next;
	in_func = 0; fn_depth--; scope_pop();
	if (saw_va_pack && !f->attr.always_inline) die("parse: %s uses __builtin_va_arg_pack but isn't always_inline", name);
	if (f->attr.always_inline && (saw_va_pack || (f->attr.gnu_inline && decl_sc_inline_extern))) {   /* inlined at every call */
		InlineDef *d = calloc(1, sizeof *d); d->params = params_tk; d->ret = ret; strmap_put(&inline_defs, strdup(name), d);
		f->no_emit = 1;
	}
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
/* Reduce a folded value to what an object of type `t` holds (C: arithmetic happens IN the type — (u8)0x1ff == 0xff,
 * 0xFFFFFFFFu + 1 == 0, (int)0x80000000 < 0). Pointers are 32-bit unsigned on ARM32. */
static long fold_to(long v, Type *t) {
	if (!t) return v;
	int sz = (t->kind == TY_PTR || t->kind == TY_ARRAY) ? 4 : t->size, un = t->is_unsigned || t->kind == TY_PTR || t->kind == TY_ARRAY;
	switch (sz) {
	case 1: return un ? (long)(unsigned char)v  : (long)(signed char)v;
	case 2: return un ? (long)(unsigned short)v : (long)(short)v;
	case 4: return un ? (long)(unsigned int)v   : (long)(int)v;
	default: return v;
	}
}
static int ty_uns(Type *t) { return t && (t->is_unsigned || t->kind == TY_PTR || t->kind == TY_ARRAY); }
static long eval_rel(Node *n, long a, long b) {   /* compare in the operands' common type (usual arithmetic conv.) */
	Type *ct = (n->lhs->type && n->rhs->type) ? usual_arith(n->lhs->type, n->rhs->type) : NULL;
	a = fold_to(a, ct); b = fold_to(b, ct);
	unsigned long long ua = (unsigned long long)a, ub = (unsigned long long)b; int u = ty_uns(ct);
	switch (n->kind) {
	case ND_LT: return u ? ua <  ub : a <  b;
	case ND_LE: return u ? ua <= ub : a <= b;
	case ND_GT: return u ? ua >  ub : a >  b;
	default:    return u ? ua >= ub : a >= b;   /* ND_GE */
	}
}
static long eval_node(Node *n, int *ok);
static long eval_try(Node *n, int *ok) { add_type(n); return eval_node(n, ok); }   /* types first: folding is type-directed */
/* ---- floating constant folding (IEEE double on the host = the target's double; float results round to
 * float). An integer operand converts by its type's sign. */
static double eval_fp(Node *n, int *ok);
static int fp_operands(Node *n) { return (n->lhs && is_fp(n->lhs->type)) || (n->rhs && is_fp(n->rhs->type)); }
static double int_to_double(long v, Type *t) { return (t && t->is_unsigned && t->size == 8) ? (double)(unsigned long long)v : (double)v; }
static long fp_to_int(double d, Type *t) {   /* C conversion: truncate toward zero, then into t */
	if (t && t->is_bool) return d != 0;
	if (t && t->is_unsigned) return fold_to((long)(unsigned long long)d, t);
	return fold_to((long)d, t);
}
static int eval_truth(Node *n, int *ok) { return is_fp(n->type) ? eval_fp(n, ok) != 0 : eval_node(n, ok) != 0; }
static double fp_round(double d, Type *t) { return t && t->kind == TY_FLOAT ? (double)(float)d : d; }   /* a value converted to t */
static double eval_fp(Node *n, int *ok) {
	double r;
	if (!is_fp(n->type)) return int_to_double(eval_node(n, ok), n->type);
	#define OPND(x) fp_round(eval_fp(x, ok), n->type)   /* an operand, converted to the operation's type first */
	switch (n->kind) {
	case ND_NUM:  return n->fval;
	case ND_CAST: r = eval_fp(n->lhs, ok); break;
	case ND_NEG:  r = -eval_fp(n->lhs, ok); break;
	case ND_ADD:  r = OPND(n->lhs) + OPND(n->rhs); break;
	case ND_SUB:  r = OPND(n->lhs) - OPND(n->rhs); break;
	case ND_MUL:  r = OPND(n->lhs) * OPND(n->rhs); break;
	case ND_DIV:  r = OPND(n->lhs) / OPND(n->rhs); break;
	case ND_COND: r = eval_truth(n->cond, ok) ? eval_fp(n->then, ok) : eval_fp(n->els, ok); break;
	default: *ok = 0; return 0;
	}
	#undef OPND
	return n->type->kind == TY_FLOAT ? (double)(float)r : r;
}
/* The address of an lvalue whose base is an integer constant address (`((T *)K)->m.n`, `*(T *)K`): K plus the
 * member offsets; anything else isn't a constant. */
static long int_address(Node *lv, int *ok) {
	long a = 0;
	for (;;) {
		if (lv->kind == ND_MEMBER && !lv->bit_width) { a += lv->offset; lv = lv->lhs; }
		else if (lv->kind == ND_DEREF) return a + eval_node(lv->lhs, ok);
		else { *ok = 0; return 0; }
	}
}
static long eval_node(Node *n, int *ok) {
	long r;
	if (is_fp(n->type)) { *ok = 0; return 0; }   /* not an INTEGER constant (casts/compares of one fold below) */
	if (fp_operands(n)) switch (n->kind) {        /* an integer result from floating operands */
	case ND_CAST: return fp_to_int(eval_fp(n->lhs, ok), n->type);
	#define CMPND(x) fp_round(eval_fp(x, ok), usual_arith(n->lhs->type, n->rhs->type))   /* both in the common type */
	case ND_EQ: return CMPND(n->lhs) == CMPND(n->rhs);
	case ND_NE: return CMPND(n->lhs) != CMPND(n->rhs);
	case ND_LT: return CMPND(n->lhs) <  CMPND(n->rhs);
	case ND_LE: return CMPND(n->lhs) <= CMPND(n->rhs);
	case ND_GT: return CMPND(n->lhs) >  CMPND(n->rhs);
	case ND_GE: return CMPND(n->lhs) >= CMPND(n->rhs);
	#undef CMPND
	case ND_NOT: return !eval_truth(n->lhs, ok);
	case ND_AND: return eval_truth(n->lhs, ok) && eval_truth(n->rhs, ok);
	case ND_OR:  return eval_truth(n->lhs, ok) || eval_truth(n->rhs, ok);
	default: break;
	}
	switch (n->kind) {
	case ND_NUM:    return n->val;
	case ND_ADDR: r = int_address(n->lhs, ok); break;   /* &((T *)K)->m, &((T *)K)[i]: the classic offsetof */
	case ND_MEMBER: case ND_DEREF:                      /* an array lvalue decays to its address: &((T *)0)->arr[i] */
		if (!n->type || n->type->kind != TY_ARRAY) { *ok = 0; return 0; }
		r = int_address(n, ok); break;
	case ND_NEG:    r = -eval_node(n->lhs, ok); break;
	case ND_BITNOT: r = ~eval_node(n->lhs, ok); break;
	case ND_NOT:    return !eval_node(n->lhs, ok);
	case ND_CAST:   r = eval_node(n->lhs, ok); break;   /* fold_to(n->type) below does the conversion */
	case ND_ADD:    r = eval_node(n->lhs, ok) +  eval_node(n->rhs, ok); break;
	case ND_SUB:    r = eval_node(n->lhs, ok) -  eval_node(n->rhs, ok); break;
	case ND_MUL:    r = (long)((unsigned long long)eval_node(n->lhs, ok) * (unsigned long long)eval_node(n->rhs, ok)); break;
	case ND_DIV: case ND_MOD: {
		long a = eval_node(n->lhs, ok), d = eval_node(n->rhs, ok);
		if (!d) { *ok = 0; return 0; }   /* x/0 is not a constant expression */
		if (ty_uns(n->type)) { unsigned long long ua = (unsigned long long)fold_to(a, n->type), ud = (unsigned long long)fold_to(d, n->type);
			r = (long)(n->kind == ND_DIV ? ua / ud : ua % ud); }
		else r = n->kind == ND_DIV ? a / d : a % d;
		break; }
	case ND_BITAND: r = eval_node(n->lhs, ok) &  eval_node(n->rhs, ok); break;
	case ND_BITOR:  r = eval_node(n->lhs, ok) |  eval_node(n->rhs, ok); break;
	case ND_BITXOR: r = eval_node(n->lhs, ok) ^  eval_node(n->rhs, ok); break;
	case ND_SHL:    r = (long)((unsigned long long)eval_node(n->lhs, ok) << eval_node(n->rhs, ok)); break;
	case ND_SHR: {  /* logical for an unsigned left operand, arithmetic for signed */
		long a = fold_to(eval_node(n->lhs, ok), n->lhs->type), b = eval_node(n->rhs, ok);
		r = ty_uns(n->lhs->type) ? (long)((unsigned long long)a >> b) : a >> b; break; }
	case ND_EQ: case ND_NE: {
		Type *ct = (n->lhs->type && n->rhs->type) ? usual_arith(n->lhs->type, n->rhs->type) : NULL;
		long a = fold_to(eval_node(n->lhs, ok), ct), b = fold_to(eval_node(n->rhs, ok), ct);
		return n->kind == ND_EQ ? a == b : a != b; }
	case ND_LT: case ND_LE: case ND_GT: case ND_GE: { long a = eval_node(n->lhs, ok), b = eval_node(n->rhs, ok); return eval_rel(n, a, b); }
	case ND_AND:    return eval_node(n->lhs, ok) && eval_node(n->rhs, ok);
	case ND_OR:     return eval_node(n->lhs, ok) || eval_node(n->rhs, ok);
	case ND_COND:   r = eval_truth(n->cond, ok) ? eval_node(n->then, ok) : eval_node(n->els, ok); break;
	case ND_CALL:   /* fold the __attribute__((const)) bit-count builtins over a constant argument */
		if (n->name[0] && n->args) {
			long a = eval_node(n->args, ok);
			if (!strcmp(n->name, "__builtin_clz"))    return clz_bits((unsigned int)a, 32);
			if (!strcmp(n->name, "__builtin_clzll") || !strcmp(n->name, "__builtin_clzl")) return clz_bits((unsigned long long)a, 64);
			if (!strcmp(n->name, "__builtin_ctz"))    return ctz_bits((unsigned int)a, 32);
			if (!strcmp(n->name, "__builtin_ctzll") || !strcmp(n->name, "__builtin_ctzl")) return ctz_bits((unsigned long long)a, 64);
			if (!strcmp(n->name, "__builtin_ffs"))    return a ? ctz_bits((unsigned int)a, 32) + 1 : 0;
			if (!strcmp(n->name, "__builtin_ffsll"))  return a ? ctz_bits((unsigned long long)a, 64) + 1 : 0;
			if (!strcmp(n->name, "__builtin_bswap16")) { unsigned u = (unsigned)a; return ((u & 0xff) << 8) | ((u >> 8) & 0xff); }
			if (!strcmp(n->name, "__builtin_bswap32")) { unsigned u = (unsigned)a; return (long)(((u & 0xffu) << 24) | ((u & 0xff00u) << 8) | ((u >> 8) & 0xff00u) | ((u >> 24) & 0xffu)); }
			if (!strcmp(n->name, "__builtin_bswap64")) { unsigned long long u = (unsigned long long)a, r = 0; for (int i = 0; i < 8; i++) r = (r << 8) | ((u >> (8 * i)) & 0xff); return (long)r; }
		}
		*ok = 0; return 0;
	default: *ok = 0; return 0;
	}
	return fold_to(r, n->type);   /* the value an object of the result type holds */
}
static long eval_const(Node *n) {
	int ok = 1; long v = eval_try(n, &ok);
	if (!ok) die("parse: not a constant expression (node %d, near line %d)", n->kind, tk->line);
	return v;
}
static Init *mkinit(int kind) { Init *i = calloc(1, sizeof *i); i->kind = kind; return i; }

/* Decode a raw string-literal body (escapes kept intact by the lexer) into bytes. Returns the byte count;
 * writes up to `cap` bytes into `out`. Mirrors the char-literal escapes the lexer already handles, so a
 * `char arr[] = "..."` global lowers to the same bytes GCC would emit. */
static int str_decode(const char *s, unsigned char *out, int cap);
/* Adjacent string literals from the cursor, concatenated — each decoded on its own (an escape never runs into the next
 * piece: "\x41" "B" is "AB", "\1" "2" is "\0012"). Narrow: the bytes (no terminator counted); wide: the code units. */
static unsigned char *strs_bytes(size_t *len) {
	size_t cap = 1; for (Token *t = tk; t->kind == TK_STR; t = t->next) cap += strlen(t->sval);
	unsigned char *b = malloc(cap); size_t n = 0;
	while (tk->kind == TK_STR) { n += str_decode(tk->sval, b + n, (int)(cap - n)); tk = tk->next; }   /* (decoding never lengthens) */
	*len = n; return b;
}
static unsigned *strs_wide(int *len) {
	int cap = 1; for (Token *t = tk; t->kind == TK_STR; t = t->next) cap += (int)strlen(t->sval);
	unsigned *w = malloc(cap * sizeof *w); int n = 0;
	while (tk->kind == TK_STR) { n += wstr_decode(tk->sval, w + n, cap - n); tk = tk->next; }
	*len = n; return w;
}
static int str_decode(const char *s, unsigned char *out, int cap) {
	int n = 0;
	while (*s) {
		unsigned char v;
		if (*s == '\\') {
			s++;
			switch (*s) {
			case 'n': v = '\n'; s++; break;  case 't': v = '\t'; s++; break;  case 'r': v = '\r'; s++; break;
			case 'a': v = '\a'; s++; break;  case 'b': v = '\b'; s++; break;  case 'f': v = '\f'; s++; break;
			case 'v': v = '\v'; s++; break;  case '\\': v = '\\'; s++; break;
			case '"': v = '"'; s++; break;   case '\'': v = '\''; s++; break; case '?': v = '?'; s++; break;
			case 'x': { s++; v = 0; while ((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'f') || (*s >= 'A' && *s <= 'F')) {
				int d = *s <= '9' ? *s - '0' : (*s | 0x20) - 'a' + 10; v = (v << 4) | d; s++; } break; }
			case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': {
				v = 0; int k = 0; while (k < 3 && *s >= '0' && *s <= '7') { v = (v << 3) | (*s - '0'); s++; k++; } break; }
			default: v = (unsigned char)*s; if (*s) s++; break;
			}
		} else { v = (unsigned char)*s; s++; }
		if (n < cap) out[n] = v;
		n++;
	}
	return n;
}
static InitPlace *pi_append(InitPlace **tail, int off, Type *ty, Node *expr, int bw, int bo) {
	InitPlace *p = calloc(1, sizeof *p);
	p->off = off; p->ty = ty; p->expr = expr; p->bit_width = bw; p->bit_offset = bo; p->sso = init_sso;
	(*tail)->next = p; *tail = p; return p;
}
struct pp_ent { int off, seq; Type *ty; Node *expr; };
static int cmp_pp(const void *a, const void *b) {
	const struct pp_ent *x = a, *y = b;
	if (x->off != y->off) return x->off - y->off;
	return x->seq - y->seq;
}

/* Parse the initializer for an object of type `ty` sitting at absolute byte offset `base`, appending scalar
 * leaf placements. Returns the byte extent (used to size an unsized top-level array). Handles: braces,
 * positional + `.field`/`[i]` designators (absolute-seek cursor; last-writer-wins is applied at lowering),
 * chained/braceless designators (`.a.b = v`), GNU ranges `[lo...hi]`, `char[] = "..."` (inline bytes), and
 * compound literals `(T){...}`. This is the ONE place that understands initializer syntax. */
/* C11 6.7.9p20 brace elision: an aggregate initialized WITHOUT its own braces takes initializers for its
 * members, in order, from the enclosing list (`struct P a[] = { 1, 2, 3, 4 }` == `{ {1, 2}, {3, 4} }`).
 * *pending is an already-parsed first scalar (consumed here); later members consume `,` + the next
 * initializer, stopping at the list's `}` or a designator (the enclosing list takes over from there). A
 * braced member (`{`) or a string for a char array is parsed normally; a union takes its first member. */
static int elide_more(void) {   /* consume the `,` before the next member's initializer, if one follows */
	if (!is(",") || !strcmp(tk->next->text, "}") || !strcmp(tk->next->text, ".") || !strcmp(tk->next->text, "[")) return 0;
	tk = tk->next; return 1;
}
static void elide_init(Type *ty, int base, InitPlace **tail, Node **pending) {
	if (ty->kind == TY_ARRAY || is_vec(ty)) {                /* a vector's lanes elide like an array's elements (GCC) */
		if (ty->len == 0) die("parse: brace elision into an unsized array (line %d)", tk->line);
		for (int k = 0; k < ty->len; k++) {
			if (!*pending && (k ? !elide_more() : 0)) return;
			if (*pending) elide_init(ty->base, base + k * ty->base->size, tail, pending);
			else if (is("{") || (tk->kind == TK_STR && ty->base->kind == TY_ARRAY)) parse_init(ty->base, base + k * ty->base->size, tail);
			else elide_init(ty->base, base + k * ty->base->size, tail, pending);
		}
		return;
	}
	if (ty->kind == TY_STRUCT) {
		if (!*pending && tk->kind != TK_STR) { *pending = assign(); add_type(*pending); }   /* maybe a whole struct value (a string
		                                                                                     * never is: it's a member char array's) */
		if (*pending && (*pending)->type && (*pending)->type->kind == TY_STRUCT && (*pending)->type->size == ty->size) {   /* a whole struct value */
			pi_append(tail, base, ty, *pending, 0, 0); *pending = NULL; return;
		}
		int first = 1, uni = 1;
		for (Member *m = ty->members; m; m = m->next) if (m->offset) uni = 0;
		for (Member *m = ty->members; m; m = m->next) {
			if (m->is_bitfield && !m->name[0]) continue;       /* unnamed bitfield: no initializer */
			if (!first && !*pending && !elide_more()) return;
			if (!first && uni) return;                         /* union: first member only */
			first = 0;
			if (m->is_bitfield) { Node *e = *pending ? *pending : assign(); *pending = NULL; pi_append(tail, base + m->offset, m->type, e, m->bit_width, m->bit_offset); }
			else if (!*pending && (is("{") || (tk->kind == TK_STR && m->type->kind == TY_ARRAY))) parse_init(m->type, base + m->offset, tail);
			else elide_init(m->type, base + m->offset, tail, pending);
		}
		return;
	}
	Node *e = *pending ? *pending : assign(); *pending = NULL;   /* scalar leaf */
	init_leaf(tail, base, ty, e);
}
static int init_nest;                                   /* braces around the object being initialized (0: a whole declarator's) */
static int parse_init1(Type *ty, int base, InitPlace **tail);
static int literal_is_element(Token *t, int np);
static int parse_init(Type *ty, int base, InitPlace **tail) {
	int braced = is("{");
	init_nest += braced; int r = parse_init1(ty, base, tail); init_nest -= braced;
	return r;
}
static int parse_init1(Type *ty, int base, InitPlace **tail) {
	if (is("{") && ty->kind == TY_ARRAY && tk->next->kind == TK_STR
	    && (tk->next->wide ? ty->base->size == tk->next->wide : ty->base->kind == TY_CHAR)) {   /* `char a[] = { "..." }`: */
		Token *t = tk->next; while (t->kind == TK_STR) t = t->next;                          /* the string, braced */
		if ((t->kind == TK_PUNCT && !strcmp(t->text, "}")) || (t->kind == TK_PUNCT && !strcmp(t->text, ",") && t->next->kind == TK_PUNCT && !strcmp(t->next->text, "}"))) {
			expect("{"); int r = parse_init1(ty, base, tail); consume(","); expect("}");
			return r;
		}
	}
	if (is("{")) {
		expect("{");
		if (ty->kind == TY_STRUCT) {
			#define INIT_MEMBER(m) (!((m)->is_bitfield && !(m)->name[0]))   /* unnamed bitfields (`int :4`) take no initializer */
			int nm = 0; for (Member *m = ty->members; m; m = m->next) nm += INIT_MEMBER(m);
			Member **marr = malloc((nm ? nm : 1) * sizeof *marr);
			{ int i = 0; for (Member *m = ty->members; m; m = m->next) if (INIT_MEMBER(m)) marr[i++] = m; }
			#undef INIT_MEMBER
			int at = 0, outer_sso = init_sso; init_sso = ty->sso;
			while (!is("}")) {
				int olddes = tk->kind == TK_IDENT && tk->next && !strcmp(tk->next->text, ":");   /* GNU old-style `member: value` */
				if (is(".") || olddes) {   /* designated: reposition the member cursor absolutely */
					char mn[64];
					if (!olddes) expect(".");
					ident(mn);
					if (olddes) expect(":"); else consume("=");
					int f = -1; for (int i = 0; i < nm; i++) if (!strcmp(marr[i]->name, mn)) { f = i; break; }
					if (f < 0) die("parse: struct has no member '%s'", mn);
					at = f;
				}
				if (at >= nm) die("parse: excess elements in struct initializer");
				Member *m = marr[at];
				if (m->is_bitfield) { Node *e = assign(); pi_append(tail, base + m->offset, m->type, e, m->bit_width, m->bit_offset); }
				else parse_init(m->type, base + m->offset, tail);
				at++;
				if (!consume(",")) break;
			}
			free(marr); expect("}"); init_sso = outer_sso; return ty->size;
		}
		if (ty->kind == TY_ARRAY || is_vec(ty)) {           /* a vector: its lanes, in order (no designators, as GCC) */
			int esz = ty->base->size, idx = 0, maxidx = -1;
			while (!is("}")) {
				int lo = idx, hi = idx;
				if (is_vec(ty) && is("[")) die("parse: a designator in a vector initializer (line %d)", tk->line);
				if (is_vec(ty) && idx >= ty->len) die("parse: excess elements in a vector initializer (line %d)", tk->line);
				if (is("[")) { expect("["); lo = hi = (int)eval_const(assign()); if (consume("...")) hi = (int)eval_const(assign()); expect("]"); consume("="); }
				InitPlace th = {0}, *tt = &th; parse_init(ty->base, 0, &tt);   /* parse element once, replicate across the range */
				for (int k = lo; k <= hi; k++) {
					if (ty->len > 0 && k >= ty->len) continue;   /* sized array: drop excess elements */
					for (InitPlace *p = th.next; p; p = p->next) { int s = init_sso; init_sso = p->sso; pi_append(tail, base + k * esz + p->off, p->ty, p->expr, p->bit_width, p->bit_offset); init_sso = s; }
					if (k > maxidx) maxidx = k;
				}
				idx = hi + 1;
				if (!consume(",")) break;
			}
			expect("}");
			if (ty->len == 0) { ty->len = maxidx + 1; ty->size = (maxidx + 1) * esz; }
			return ty->size;
		}
		Node *e = assign(); init_leaf(tail, base, ty, e);   /* scalar in braces: { e } */
		while (consume(",")) { if (is("}")) break; assign(); }
		expect("}"); return ty->size;
	}
	if (ty->kind == TY_STRUCT && is(".")) {   /* braceless designated continuation: `.a.b = v` == `.a = { .b = v }` */
		expect("."); char mn[64]; ident(mn);
		Member *m = NULL; for (Member *mm = ty->members; mm; mm = mm->next) if (!strcmp(mm->name, mn)) { m = mm; break; }
		if (!m) die("parse: struct has no member '%s'", mn);
		consume("=");
		int outer_sso = init_sso; init_sso = ty->sso;
		if (m->is_bitfield) { Node *e = assign(); pi_append(tail, base + m->offset, m->type, e, m->bit_width, m->bit_offset); }
		else parse_init(m->type, base + m->offset, tail);
		init_sso = outer_sso;
		return ty->size;
	}
	if (ty->kind == TY_ARRAY && is("[")) {   /* braceless `[i] = v` continuation */
		int esz = ty->base->size;
		expect("["); int lo = (int)eval_const(assign()); int hi = lo; if (consume("...")) hi = (int)eval_const(assign()); expect("]"); consume("=");
		InitPlace th = {0}, *tt = &th; parse_init(ty->base, 0, &tt);
		for (int k = lo; k <= hi; k++) for (InitPlace *p = th.next; p; p = p->next) pi_append(tail, base + k * esz + p->off, p->ty, p->expr, p->bit_width, p->bit_offset);
		return ty->size;
	}
	if (ty->kind == TY_ARRAY && tk->kind == TK_STR && tk->wide && ty->base->size == tk->wide) {   /* wchar_t arr[] = L"..." */
		int wl; unsigned *w = strs_wide(&wl);
		int total = ty->len > 0 ? ty->len : wl + 1;
		if (ty->len == 0) { ty->len = total; ty->size = total * ty->base->size; }
		for (int i = 0; i < wl && i < total; i++) pi_append(tail, base + i * ty->base->size, ty->base, num(w[i]), 0, 0);
		free(w);
		return ty->size;
	}
	if (ty->kind == TY_ARRAY && ty->base->kind == TY_CHAR && tk->kind == TK_STR) {   /* char arr[] = "..." -> inline bytes */
		size_t n; unsigned char *dbuf = strs_bytes(&n); int dl = (int)n;
		int total = ty->len > 0 ? ty->len : dl + 1;
		if (ty->len == 0) { ty->len = total; ty->size = total; }
		for (int i = 0; i < dl && i < total; i++) pi_append(tail, base + i, ty_char, num((unsigned char)dbuf[i]), 0, 0);
		free(dbuf);
		return total;
	}
	if (is("(") && !cast_ahead()) {   /* grouping parens around a compound literal: ((T){...}) (kernel cap_t/kuid_t macros) */
		Token *save = tk; int np = 0;
		while (is("(")) {
			tk = tk->next; np++;
			if (cast_ahead() && literal_is_element(tk, np)) {
				char d[64]; expect("("); Type *t = declarator(declspec(NULL, NULL), d); expect(")");
				if (is("{")) { int r = parse_init(t, base, tail); while (np--) expect(")"); return r; }
				break;   /* a cast, not a compound literal */
			}
			if (!is("(")) break;   /* not nested grouping */
		}
		tk = save;   /* ordinary parenthesized expression — fall through to the scalar leaf */
	}
	if (cast_ahead() && literal_is_element(tk, 0)) {   /* compound literal (T){...}: initialize as if the braces were written directly */
		Token *save = tk; char d[64];
		expect("("); Type *t = declarator(declspec(NULL, NULL), d); expect(")");
		if (is("{")) return parse_init(t, base, tail);
		tk = save;   /* just a cast of a constant — fall through to the scalar leaf */
	}
	if (tk->kind == TK_STR && ty->kind == TY_STRUCT) { Node *none = NULL; elide_init(ty, base, tail, &none); return ty->size; }   /* elided
	                                                                           * braces: the string initializes the first char array */
	Node *e = assign(); add_type(e);
	if (is_vec(ty) && !is_vec(e->type) && init_nest) { elide_init(ty, base, tail, &e); return ty->size; }   /* elided braces */
	if (is_vec(ty) || is_vec(e->type)) {   /* a whole vector value (a whole vector object from a scalar is an error, as in GCC) */
		vec_assign_ok(ty, e->type, "an initializer");
		pi_append(tail, base, ty, e, 0, 0); return ty->size;
	}
	if ((ty->kind == TY_STRUCT || ty->kind == TY_ARRAY) && !(e->type && e->type->kind == TY_STRUCT && e->type->size == ty->size)) {
		elide_init(ty, base, tail, &e);   /* brace elision: this scalar starts the aggregate's flattened member list */
		return ty->size;
	}
	init_leaf(tail, base, ty, e);   /* scalar (or whole-aggregate copy) leaf */
	return ty->size;
}

/* Is the compound literal `(T){...}` at t, inside np grouping parens, the whole initializer element? Else it's an
 * operand of a larger expression (`((T){4}).a`, `(T){1}.x + 2`), parsed as one. */
static int literal_is_element(Token *t, int np) {
	#define P(t, s) ((t)->kind == TK_PUNCT && !strcmp((t)->text, s))
	t = skip_balanced(t);                                    /* (T) */
	if (!P(t, "{")) return 0;
	int d = 0; do { if (P(t, "{")) d++; else if (P(t, "}")) d--; t = t->next; } while (d && t->kind != TK_EOF);
	while (np--) { if (!P(t, ")")) return 0; t = t->next; }
	return P(t, ",") || P(t, "}") || P(t, ";");
	#undef P
}
/* Fold an initializer expression to a link-time address constant `symbol + byte-addend` (returns 0 if it
 * isn't one). Handles &sym, &sym.member, &arr[i] (new_add bakes the *elem-size scale into an ND_MUL, so the
 * addend reads straight out of the tree), pointer +/- constant, casts, and a bare array/function/global name
 * that decays to its address. Mirrors a real compiler's constant-address evaluator. */
static int as_addr_const(Node *e, char *sym, long *ad);
static int addr_of_lval(Node *lv, char *sym, long *ad) {
	if (!lv) return 0;
	switch (lv->kind) {
	case ND_GVAR: case ND_VAR: strncpy(sym, lv->name, 63); return 1;
	case ND_MEMBER: { int r = addr_of_lval(lv->lhs, sym, ad); *ad += lv->offset; return r; }
	case ND_DEREF:  return as_addr_const(lv->lhs, sym, ad);   /* &*p == p */
	case ND_CAST:   return addr_of_lval(lv->lhs, sym, ad);
	default: return 0;
	}
}
static int as_addr_const(Node *e, char *sym, long *ad) {
	if (!e) return 0;
	int ok = 1; long c;
	switch (e->kind) {
	case ND_ADDR: { long s = *ad; if (addr_of_lval(e->lhs, sym, ad)) return 1; *ad = s; return as_addr_const(e->lhs, sym, ad); }   /* &lval, or &&func: a bare function name is already ND_ADDR(GVAR), so `&func` == `func` */
	case ND_CAST: return as_addr_const(e->lhs, sym, ad);
	case ND_GVAR: case ND_VAR:   /* a bare name: an array's (or function's) address; a scalar's VALUE isn't an address */
		if (!e->type || !(e->type->kind == TY_ARRAY || e->type->fn_ret)) return 0;
		strncpy(sym, e->name, 63); return 1;
	case ND_LABELADDR: if (snprintf(sym, SYMEXPR_MAX, CLABEL_FMT, cur_func_name, e->name) >= SYMEXPR_MAX) die("parse: label symbol too long ('%s')", e->name); return 1;   /* static void *t[] = { &&l } */
	case ND_MEMBER: case ND_DEREF:   /* an array-typed member / row used as a value decays to its address (&s.arr[0], &a[3][5]) */
		return e->type && e->type->kind == TY_ARRAY ? addr_of_lval(e, sym, ad) : 0;
	case ND_ADD:
		if (as_addr_const(e->lhs, sym, ad)) { c = eval_try(e->rhs, &ok); if (!ok) return 0; *ad += c; return 1; }
		ok = 1;
		if (as_addr_const(e->rhs, sym, ad)) { c = eval_try(e->lhs, &ok); if (!ok) return 0; *ad += c; return 1; }
		return 0;
	case ND_SUB:
		if (as_addr_const(e->lhs, sym, ad)) {
			c = eval_try(e->rhs, &ok); if (ok) { *ad -= c; return 1; }
			char s2[SYMEXPR_MAX] = ""; long a2 = 0;   /* address - address: the assembler folds `a - b` (&&l1 - &&l0) */
			if (!as_addr_const(e->rhs, s2, &a2) || strchr(s2, '-')) return 0;
			if (strlen(sym) + strlen(s2) + 4 >= SYMEXPR_MAX) die("parse: address difference too long");
			strcat(sym, " - "); strcat(sym, s2); *ad -= a2; return 1;
		}
		return 0;
	case ND_DIV:   /* (p - q) / sizeof *p of byte-sized pointers (void* / label addresses): the difference itself */
		c = eval_try(e->rhs, &ok); if (!ok || c != 1) return 0;
		return as_addr_const(e->lhs, sym, ad);
	case ND_COND:   /* CONST ? addrA : addrB (kernel: `(false) ? fnA : fnB`) -> fold the taken branch */
		c = eval_try(e->cond, &ok); if (!ok) return 0;
		return as_addr_const(c ? e->then : e->els, sym, ad);
	default: return 0;
	}
}

/* Lower placements to a .data byte image. Leaves apply in source order (a later designator overrides an
 * earlier one); a constant writes its bytes little-endian, a bitfield merges its bits into its unit, and an
 * address constant claims a word for an R_ARM_ABS32 symbol (dropped again if a later constant overwrites
 * it). The image is then emitted as symbol words, constant words/bytes, and zero runs. */
/* GCC folds a read of a const (non-volatile) scalar global with a constant initializer in a static initializer
 * (`const char a = 0x42; double b = a;`): its value, from its initializer's bytes. */
static Node *fold_const_globals(Node *e) {
	if (!e || e->kind == ND_ADDR) return e;           /* an address, not a read */
	if (e->kind == ND_GVAR) {
		Gvar *g = global_find(e->name); Type *t = g ? g->type : NULL;
		if (!t || !g->init || g->is_extern || g->is_tls || t->kind >= TY_PTR || !(t->quals & 1) || (t->quals & 2)) return e;
		unsigned char b[8] = {0}; int at = 0;
		for (Init *it = g->init; it; it = it->next) {
			if (it->kind == INIT_SYM || at + it->size > 8) return e;
			if (it->kind == INIT_CONST) for (int k = 0; k < it->size; k++) b[at + k] = (unsigned char)(it->val >> (8 * k));
			at += it->size;
		}
		unsigned long long v = 0; for (int k = 7; k >= 0; k--) v = v << 8 | b[k];
		if (t->kind == TY_FLOAT) { unsigned u = (unsigned)v; float f; memcpy(&f, &u, 4); return fnum(f, t); }
		if (t->kind == TY_DOUBLE) { double d; memcpy(&d, &v, 8); return fnum(d, t); }
		long sv = t->size < 8 ? (t->is_unsigned ? (long)(v & ((1ULL << (8 * t->size)) - 1)) : (long)((long long)(v << (64 - 8 * t->size)) >> (64 - 8 * t->size))) : (long)v;
		return tnum(sv, t);
	}
	e->lhs = fold_const_globals(e->lhs); e->rhs = fold_const_globals(e->rhs);
	e->cond = fold_const_globals(e->cond); e->then = fold_const_globals(e->then); e->els = fold_const_globals(e->els);
	return e;
}
static int same_symbol_difference(const char *sym) {   /* "a - a": a constant (the addend alone) */
	const char *m = strstr(sym, " - "); return m && strlen(m + 3) == (size_t)(m - sym) && !strncmp(sym, m + 3, m - sym);
}
static Init *lower_global(InitPlace *places, int total) {
	int size = total;
	for (InitPlace *p = places; p; p = p->next) if (p->off + p->ty->size > size) size = p->off + p->ty->size;   /* flexible array tail */
	unsigned char *img = calloc(size + 1, 1); char **symat = calloc(size + 1, sizeof *symat); long *symadd = calloc(size + 1, sizeof *symadd);
	int *symsz = calloc(size + 1, sizeof *symsz);    /* a symbol's slot: 4 bytes, or 2 for a label difference in a short */
	for (InitPlace *p = places; p; p = p->next) {
		char sym[SYMEXPR_MAX] = ""; long addend = 0; int have = 0; unsigned long long v = 0;
		for (int k = 0; k < p->ty->size; k++) for (int q = p->off + k - 3; q <= p->off + k; q++) if (q >= 0 && symat[q] && q + symsz[q] > p->off + k) symat[q] = NULL;   /* overwritten */
		p->expr = fold_const_globals(p->expr);
		if (!p->bit_width && as_addr_const(p->expr, sym, &addend)) {
			int diff = strstr(sym, " - ") != NULL;
			if (diff && same_symbol_difference(sym)) { v = (unsigned long long)addend; have = 1; }   /* &s.f - &s.b */
			else if (p->ty->size == 4 || (p->ty->size == 2 && diff)) {
				symat[p->off] = strdup(sym); symadd[p->off] = addend; symsz[p->off] = p->ty->size == 2 ? 2 : 4; continue;
			}
			else die("parse: address constant in a %d-byte initializer", p->ty->size);
		}
		add_type(p->expr);
		if (have) ;
		else if (is_fp(p->ty) || is_fp(p->expr->type)) {   /* a floating value, or into a floating object: convert */
			int ok = 1; double d = eval_fp(p->expr, &ok);
			if (!ok) die("parse: not a constant floating expression (near line %d)", tk->line);
			if (p->ty->kind == TY_FLOAT) { float f = (float)d; unsigned u; memcpy(&u, &f, 4); v = u; }
			else if (p->ty->kind == TY_DOUBLE) memcpy(&v, &d, 8);
			else v = (unsigned long long)fp_to_int(d, p->ty);
		} else v = (unsigned long long)eval_const(p->expr);
		if (p->ty->is_bool) v = v != 0;
		if (p->bit_width && p->sso) {                    /* big-endian unit: the field at mirrored bits, bytes MSB first */
			int sz = p->ty->size, lo = 8 * sz - p->bit_offset - p->bit_width;
			for (int bit = 0; bit < p->bit_width; bit++) {
				int ub = lo + bit, at = p->off * 8 + (sz - 1 - ub / 8) * 8 + ub % 8;
				if ((v >> bit) & 1) img[at / 8] |= 1 << (at % 8); else img[at / 8] &= ~(1 << (at % 8));
			}
		} else if (p->bit_width) {
			for (int bit = 0; bit < p->bit_width; bit++) {
				int at = p->off * 8 + p->bit_offset + bit;
				if ((v >> bit) & 1) img[at / 8] |= 1 << (at % 8); else img[at / 8] &= ~(1 << (at % 8));
			}
		} else for (int k = 0; k < p->ty->size; k++)     /* little-endian, or byte-reversed for a reverse-order struct */
			img[p->off + (p->sso ? p->ty->size - 1 - k : k)] = (unsigned char)(k < 8 ? v >> (8 * k) : 0);
	}
	Init head = {0}, *c = &head;
	for (int at = 0; at < size; ) {
		int nz = 0; while (at + nz < size && !img[at + nz] && !symat[at + nz]) nz++;   /* zero run (no symbol starts in it) */
		if (nz >= 4 || at + nz == size) { if (nz) { c = c->next = mkinit(INIT_ZERO); c->size = nz; at += nz; } continue; }
		if (symat[at]) { c = c->next = mkinit(INIT_SYM); snprintf(c->sym, sizeof c->sym, "%s", symat[at]); c->val = symadd[at]; c->size = symsz[at]; at += symsz[at]; continue; }
		int w = at % 4 == 0 && at + 4 <= size && !symat[at + 1] && !symat[at + 2] && !symat[at + 3];
		c = c->next = mkinit(INIT_CONST); c->size = w ? 4 : 1;
		c->val = w ? (long)(img[at] | img[at + 1] << 8 | img[at + 2] << 16 | (unsigned long)img[at + 3] << 24) : img[at];
		at += c->size;
	}
	free(img); free(symat); free(symadd); free(symsz);
	return head.next;
}

/* Zero the byte range [off, off+len) of `dest` with word stores (4-aligned bulk) + byte stores (remainder). */
static void emit_zero_local(Node *dest, int off, int len, Node **c) {
	int p = off, end = off + len;
	while (p < end) {
		int word = (p % 4 == 0 && end - p >= 4);
		Node *dm = node(ND_MEMBER); dm->lhs = dest; dm->offset = p; dm->type = word ? ty_int : ty_char;
		(*c)->next = unary(ND_EXPRSTMT, binary(ND_ASSIGN, dm, num(0))); *c = (*c)->next;
		p += word ? 4 : 1;
	}
}

/* Lower placements to runtime stores against `dest`: zero the never-covered gaps (C-correct partial init),
 * then store each leaf in SOURCE order so a later designator override wins at runtime. */
static Node *lower_local(Node *dest, InitPlace *places, int total) {
	Node blk = {0}, *c = &blk;
	int n = 0; for (InitPlace *p = places; p; p = p->next) n++;
	if (n == 0) { emit_zero_local(dest, 0, total, &c); }
	else {
		struct pp_ent *a = malloc(n * sizeof *a);
		{ int i = 0; for (InitPlace *p = places; p; p = p->next) { a[i].off = p->off; a[i].seq = i; a[i].ty = p->ty; a[i].expr = p->expr; i++; } }
		qsort(a, n, sizeof *a, cmp_pp);
		int cur = 0;
		for (int i = 0; i < n; i++) {
			if (a[i].off > cur) emit_zero_local(dest, cur, a[i].off - cur, &c);
			int e = a[i].off + a[i].ty->size; if (e > cur) cur = e;
		}
		if (cur < total) emit_zero_local(dest, cur, total - cur, &c);
		free(a);
		for (InitPlace *p = places; p; p = p->next) {
			Node *dm = node(ND_MEMBER); dm->lhs = dest; dm->offset = p->off; dm->type = p->ty; dm->sso = p->sso;
			if (p->bit_width) bitfield_node(dm, p->ty, p->bit_width, p->bit_offset);
			c->next = unary(ND_EXPRSTMT, binary(ND_ASSIGN, dm, p->expr)); c = c->next;
		}
	}
	Node *nn = node(ND_BLOCK); nn->body = blk.next; return nn;
}

static Init *global_init(Type *ty) {
	InitPlace head = {0}, *tail = &head;
	int sz = parse_init(ty, 0, &tail);
	return lower_global(head.next, ty->size ? ty->size : sz);
}

/* Function-signature table: a callee's return + parameter types. The return type gives ND_CALL result
 * nodes the right width; the parameter types let a caller place/widen each argument per AAPCS (a 64-bit
 * param needs its arg in an even register pair, and an int arg to a 64-bit param must be widened).
 * Populated for every prototype/definition. */
typedef struct { Type *ret; Type **params; int nparams; int variadic, base_pcs, align; Attr attr; int ext_decl; } FuncSig;   /* ext_decl: some
                                     * file-scope declaration was not `inline` without extern (so this TU defines it externally) */   /* align: aligned(N) */   /* variadic: 0 = prototype, 1 = `...`, 2 = params unknown */
static StrMap func_sigs;                                 /* name -> its FuncSig */
static FuncSig *sig_of(const char *name) { return name && name[0] ? strmap_get(&func_sigs, name) : NULL; }
static void record_func_sig(const char *name, Type *ret, Type **params, int np, int variadic) {
	FuncSig *f = sig_of(name);
	if (!f) { f = calloc(1, sizeof *f); strmap_put(&func_sigs, strdup(name), f); }
	f->ret = ret;
	if (variadic == 2 && f->nparams) return;             /* an unknown-params redeclaration keeps a known prototype */
	f->variadic = variadic;
	f->nparams = np;
	f->params = np ? malloc(np * sizeof *params) : NULL;
	for (int i = 0; i < np; i++) f->params[i] = params[i];
}
Type *func_ret_type(const char *name) { FuncSig *f = sig_of(name); return f ? f->ret : NULL; }
static Type *fn_type_of(const char *name) {   /* a declared function's type (typeof(f)) */
	FuncSig *f = sig_of(name); Type *ft = func_type(f->ret);
	ft->params = f->params; ft->nparams = f->nparams; ft->variadic = f->variadic;
	return ft;
}
int func_declared(const char *name) { return sig_of(name) != NULL; }
static Func *parsed_funcs;                               /* the unit's definitions (for the symbol pragmas) */
static int func_defined(const char *name) { for (Func *f = parsed_funcs; f; f = f->next) if (!strcmp(f->name, name) && !f->no_emit) return 1; return 0; }
static int func_unprototyped(const char *name) { FuncSig *f = sig_of(name); return !f || f->variadic == 2; }
int func_base_pcs(const char *name) {   /* a `...` prototype or pcs("aapcs"); unknown params (2) use the normal (VFP) PCS, like GCC */
	FuncSig *f = sig_of(name); return f && (f->variadic == 1 || f->base_pcs);
}
static int sig_align(const char *name, int align) {    /* record a declaration's aligned(N); returns the largest so far */
	FuncSig *f = sig_of(name); if (!f) return align;
	if (align > f->align) f->align = align;
	return f->align;
}
/* __alignof__ of an expression: a DECLARATION's alignment when it names one (GCC) — a function's (at least 4, the ARM
 * code alignment; more with aligned(N)), an object's aligned(N) — else its type's. */
static int decl_align(Node *e) {
	Node *d = e->kind == ND_ADDR && e->lhs && e->lhs->kind == ND_GVAR ? e->lhs : e;
	if (d->kind == ND_GVAR && func_declared(d->name) && !global_find(d->name)) { int a = sig_align(d->name, 0); return a > 4 ? a : 4; }
	int t = e->type ? align_of(e->type) : 4;
	if (d->kind == ND_GVAR) { Gvar *g = global_find(d->name); if (g && g->attr.align > t) return g->attr.align; }
	return t;
}
/* C11 6.7.4p7: when EVERY file-scope declaration of a function in this unit says `inline` and none says `extern`, the
 * definition here is an INLINE definition — no external definition is emitted, and calls go to the one another unit
 * provides (GCC does exactly this, which is why its `inline` bodies are never even checked at -O0). */
static void sig_note_decl(const char *name, int sc) {
	FuncSig *f = sig_of(name); if (!f) return;
	if (!(sc & SC_INLINE) || (sc & (SC_EXTERN | SC_STATIC))) f->ext_decl = 1;
}
static int sig_inline_only(const char *name) { FuncSig *f = sig_of(name); return f && !f->ext_decl; }
static void sig_attrs(const char *name, const Attr *a) {   /* a function declaration's attributes: pcs(...) -> its calls; all kept for copy() */
	FuncSig *f = sig_of(name); if (!f) return;
	if (a->pcs) f->base_pcs = a->pcs == 1;
	attr_merge(&f->attr, a);
}
static const Attr *decl_attrs_of(const char *name) {   /* the attributes a declared function or file-scope object has so far */
	FuncSig *f = sig_of(name); if (f) return &f->attr;
	Gvar *g = global_find(name); return g ? &g->attr : NULL;
}
Type *func_param_type(const char *name, int i) {
	FuncSig *f = sig_of(name);
	return f && i < f->nparams ? f->params[i] : NULL;   /* NULL => unknown or a vararg */
}

/* The symbol pragmas, applied after the whole unit is parsed (they may name something declared later, and GCC applies
 * them to whatever the file ends up defining or declaring): `weak N` makes N's symbol weak — when nothing defines it,
 * a reference stays undefined-weak (its address is 0); `weak N = T` is that plus an alias to T; redefine_extname N T
 * renames N's symbol to T, exactly like `asm("T")`. #ident lines become .ident. */
static void apply_pragma_names(void) {
	for (PragmaName *p = idents; p; p = p->next) topasm("\t.ident %s%s", p->name, "");
	for (PragmaName *p = pragma_redefines; p; p = p->next) {
		if (global_find(p->name) || func_declared(p->name) || strmap_get(&asm_labels, p->name)) {
			const char *old = strmap_get(&asm_labels, p->name);
			if (old && strcmp(old, p->target)) die("parse: '%s' relabeled: #pragma redefine_extname after asm(\"%s\")", p->name, old);
			if (!old) strmap_put(&asm_labels, strdup(p->name), strdup(p->target));
		}
	}
	for (PragmaName *p = pragma_weaks; p; p = p->next) {
		Gvar *g = global_find(p->name); int defined = (g && !g->is_extern) || func_defined(p->name);
		if (p->target) {                                     /* weak N = T: N is a weak alias for T */
			if (defined) die("parse: #pragma weak %s = %s: '%s' is defined here", p->name, p->target, p->name);
			Attr a = (Attr){0}; a.weak = 1; snprintf(a.alias, sizeof a.alias, "%s", p->target);
			decl_symbol_attrs(p->name, &a, 0);
		} else if (defined) {
			Attr a = (Attr){0}; a.weak = 1; decl_symbol_attrs(p->name, &a, 0);   /* a definition here: emit it weak */
		} else {                                             /* nothing defines it: references become weak-undefined */
			strmap_put(&weak_names, strdup(p->name), (void *)1);
			topasm("\t.weak %s%s", asm_name(p->name), "");
		}
	}
}

Func *parse(Token *tok) {
	tk = tok;
	add_typedef("__builtin_va_list", pointer_to(ty_char));   /* va_list is a char* walking the arg block */
	{   /* signatures of the builtins gen_builtin expands inline: result type + parameter type (so e.g. an int
	     * argument to __builtin_clzll is widened to 64 bits, and bswap64's result isn't truncated) */
		Type *vp = pointer_to(ty_char), *ull = ty_ullong, *ui = ty_uint;
		struct { const char *n; Type *r, *p; } bt[] = {
			{"__builtin_clz",ty_int,ui}, {"__builtin_clzl",ty_int,ui}, {"__builtin_clzll",ty_int,ull},
			{"__builtin_ctz",ty_int,ui}, {"__builtin_ctzl",ty_int,ui}, {"__builtin_ctzll",ty_int,ull},
			{"__builtin_ffs",ty_int,ty_int}, {"__builtin_ffsl",ty_int,ty_int}, {"__builtin_ffsll",ty_int,ty_llong},
			{"__builtin_bswap16",ty_ushort,ty_ushort}, {"__builtin_bswap32",ui,ui}, {"__builtin_bswap64",ull,ull},
			{"__builtin_return_address",vp,ui}, {"__builtin_frame_address",vp,ui}, {"__builtin_extract_return_addr",vp,vp},
			{"__builtin_thread_pointer",vp,NULL},
			{"__builtin_fabs",ty_double,ty_double}, {"__builtin_fabsf",ty_float,ty_float}, {"__builtin_fabsl",ty_double,ty_double},
			{"__builtin_copysign",ty_double,ty_double}, {"__builtin_copysignf",ty_float,ty_float}, {"__builtin_copysignl",ty_double,ty_double},
			{"__builtin_signbit",ty_int,NULL}, {"__builtin_signbitf",ty_int,ty_float}, {"__builtin_signbitl",ty_int,ty_double},
		};
		for (unsigned i = 0; i < sizeof bt / sizeof *bt; i++) { Type *pt[1] = { bt[i].p }; record_func_sig(bt[i].n, bt[i].r, pt, bt[i].p ? 1 : 0, 0); }
		Type *f3[3] = { ty_float, ty_float, ty_float }, *d3[3] = { ty_double, ty_double, ty_double };   /* fused multiply-add */
		record_func_sig("__builtin_fma", ty_double, d3, 3, 0); record_func_sig("__builtin_fmaf", ty_float, f3, 3, 0); record_func_sig("__builtin_fmal", ty_double, d3, 3, 0);
		Type *cc2[2] = { vp, vp };   /* the instruction cache over [begin, end): the runtime's __clear_cache (GCC: a libgcc call) */
		record_func_sig("__clear_cache", ty_char, cc2, 2, 0);
		record_func_sig("__builtin_stack_save", vp, NULL, 0, 0); record_func_sig("__builtin_stack_restore", ty_char, cc2, 1, 0);
		Type *ap[3] = { vp, vp, ui };   /* untyped call forwarding */
		record_func_sig("__builtin_apply_args", vp, NULL, 0, 0); record_func_sig("__builtin_apply", vp, ap, 3, 0); record_func_sig("__builtin_return", ty_char, ap + 1, 1, 0);
	}
	Func head = {0}, *cur = &head;
	while (tk->kind != TK_EOF) {
		if (tk->kind == TK_IDENT && !strcmp(tk->text, "_Static_assert")) { tk = tk->next; skip_parens(); consume(";"); continue; }
		if (is("asm")) {   /* file-scope basic asm: `asm("...");` — emit its text verbatim (kernel COND_SYSCALL .weak/.set) */
			tk = tk->next; consume("volatile"); expect("(");
			size_t cap = 1, bl = 0; for (Token *t = tk; t->kind == TK_STR; t = t->next) cap += strlen(t->sval);
			char *buf = malloc(cap);
			while (tk->kind == TK_STR) {   /* concatenate + decode adjacent string literals (decoding never lengthens) */
				int dl = str_decode(tk->sval, (unsigned char *)buf + bl, (int)(cap - bl)); bl += dl; tk = tk->next;
			}
			buf[bl] = 0;
			expect(")"); expect(";");
			Gvar *g = add_global(); g->is_topasm = 1; g->str = buf;
			continue;
		}
		decl_attr = (Attr){0};
		int pvis = tk->vis;                                   /* #pragma GCC visibility push in force: these definitions' */
		int td, sc; Type *base = declspec(&td, &sc);               /* type keywords/qualifiers + storage class; struct/enum defs register */
		Attr battr = decl_attr;                               /* declspec attributes apply to every declarator */
		if (pvis && !battr.vis) battr.vis = decl_attr.vis = pvis;
		if (consume(";")) continue;                          /* type-only declaration, e.g. `struct P { ... };`   */
		if (td) { typedef_decl(base, battr); continue; }   /* (file scope: type_suffix rejects a VLA) */
		char name[64]; Type *ty = declarator(base, name);    /* *s + name + array suffix */
		int defined = 0;
		for (;;) {   /* each declarator of `T a, *f(void), b[2], g();`: a function or an object */
			Token *fparams = NULL;
			if (is("asm")) {   /* an asm label (a symbol rename), or a global register variable's register */
				tk = tk->next;
				if (sc & SC_REGISTER) { global_register(name); if (!consume(",")) break; decl_attr = battr; ty = declarator(base, name); continue; }
				asm_label_for(name);
			}
			if (ty->fn_ret && decl_fparams && (is("{") || (is_typename() && !is("__attribute__")))) { fparams = decl_fparams; ty = ty->fn_ret; }   /* a
			                                                                        * definition through a group: `void (*f(void))(float) {` */
			if (ty->fn_ret) {   /* a PROTOTYPE, no storage: through a group, typeof(f), or a function typedef (`fn_t f;`) */
				while (consume("__attribute__")) attribute();
				no_type_attrs(); no_cleanup("a function");
				record_func_sig(name, ty->fn_ret, ty->params, ty->nparams, ty->variadic); sig_attrs(name, &decl_attr); sig_align(name, decl_attr.align);
				sig_note_decl(name, sc);
				decl_symbol_attrs(name, &decl_attr, sc & SC_STATIC);
			} else if (is("(") || fparams) {   /* records its own signature; NULL = a prototype */
				decl_sc_inline_extern = (sc & SC_EXTERN) && (sc & SC_INLINE);
				Func *fn = function_tail(name, ty, fparams); decl_sc_inline_extern = 0;
				sig_note_decl(name, sc);
				if (fn) {   /* a definition ends the declaration */
					fn->is_static = (sc & SC_STATIC) != 0; if (fn->attr.alias[0]) die("parse: alias on a function definition '%s'", name);
					if ((sc & SC_EXTERN) && (sc & SC_INLINE) && (fn->attr.gnu_inline || gnu89_inline)) fn->no_emit = 1;   /* gnu89
					                                     * `extern inline`: inline-only, the external definition is another unit's */
					if (gnu89_inline || fn->attr.gnu_inline) fn->gnu89_inline = 1;   /* ...so the C11 rule below doesn't apply to it */
					cur = cur->next = fn; defined = 1;
					for (int i = 0; i < nnested_pend; i++) cur = cur->next = nested_pend[i];   /* its nested functions, after it */
					nnested_pend = 0;
					break;
				}
				decl_symbol_attrs(name, &decl_attr, sc & SC_STATIC);
			} else {
				/* File-scope redeclarations are ONE object (C11 6.9.2): `static T x;` (tentative) then `static T x = {...};`
				 * (kernel trace events), or `extern T x;` then `T x;`. Merge instead of emitting two definitions. */
				Gvar *g = global_find(name);
				if (!g) { g = new_global(name); g->type = ty;
					g->is_extern = (sc & SC_EXTERN) != 0; g->is_static = (sc & SC_STATIC) != 0; }
				if (sc & SC_TLS) g->is_tls = 1;
				else if (g->is_tls && !(sc & SC_EXTERN)) die("parse: '%s' redeclared without _Thread_local (line %d)", name, tk->line);
				else {
					if (!(sc & SC_EXTERN)) g->is_extern = 0;          /* any non-extern declaration makes it a definition */
					if (sc & SC_STATIC) g->is_static = 1;
					if (ty->size > 0 && g->type->size == 0) g->type = ty;   /* a later declaration completes the type */
				}
				while (consume("__attribute__")) attribute();
				no_type_attrs(); no_cleanup("a global");
				attr_merge(&g->attr, &decl_attr);
				if (decl_attr.alias[0] || (decl_attr.weak && g->is_extern))   /* `extern weak T x;`: a .weak reference, so an
				                                                              * undefined one resolves to 0 (musl's _DYNAMIC) */
					decl_symbol_attrs(name, &decl_attr, g->is_static);
				if (consume("=")) {
					if (g->init) die("parse: redefinition of '%s' (line %d)", name, tk->line);
					g->init = global_init(ty); g->type = ty;           /* the defining declaration's (possibly now-sized) type */
				}
				if (g->attr.common && !g->is_static && !g->is_extern && !g->init)   /* (GCC ignores it on a static) */
					die("parse: '%s' __attribute__((common)): a COMMON symbol (this toolchain is -fno-common) (line %d)", name, tk->line);
			}
			if (!consume(",")) break;
			decl_attr = battr; ty = declarator(base, name);
		}
		if (!defined) expect(";");
	}
	for (Func *f = head.next; f; f = f->next)                /* a C11 inline definition provides no external one */
		if (!f->is_static && !f->nested && !f->gnu89_inline && sig_inline_only(f->name)) f->no_emit = 1;
	parsed_funcs = head.next;
	apply_pragma_names();                                    /* #pragma weak / redefine_extname / #ident (lex.c collected them) */
	for (Func *f = head.next; f; f = f->next)                /* type every body now that all signatures are known */
		for (Node *s = f->body; s; s = s->next) add_type(s);
	return head.next;
}
