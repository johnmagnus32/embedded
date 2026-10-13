/*
 * gen.c — the codegen backend: walk the AST and emit ARMv7-A/A32 assembly text (assembled later by our
 * as). A simple STACK MACHINE, no optimization: every expression leaves its result in r0; a binary op
 * evaluates its left operand into r0, pushes it, evaluates the right into r0, then pops the left into r1
 * and combines. Functions follow AAPCS: args arrive in r0..r3, the result returns in r0, r11 is the frame
 * pointer. Locals live at [r11, #-4*(slot+1)]; the frame is set up in the prologue and torn down at the
 * single return label the body branches to.
 */
#define _POSIX_C_SOURCE 200809L   /* strdup */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cc.h"
#include "strmap.h"

static FILE *o;
static int label_id;                 /* source of unique .L labels */
static int ret_label;                /* the current function's return label id */
static int brk_lbl, cont_lbl;        /* innermost loop's break/continue targets (0 = not in a loop) */
static int uniq(void) { return ++label_id; }   /* 1-based, so 0 is a valid "none" sentinel */

/* A per-function id (used to name this function's PIC .LGOT/.LGA labels). */
static int cur_func_id, func_seq;
static const char *cur_gen_func = "?";   /* name of the function being emitted, for diagnostics */
static int cur_nfixed;                 /* current function's incoming argument words (va_start) */
static int cur_vfp;                    /* current function uses AAPCS-VFP (FP results in s0/d0) */
static int cur_core_home;              /* frame offset of the homed r0 (8, or 72 above a d0-d7 save) */
static Type *cur_ret;                  /* current function's return type (so `return e` widens to 64-bit) */
static int ngot;   /* -fPIC: per-function counter for GOT-access labels (.LGOT/.LGA) */

static void gen_expr(Node *n);
static void gen_stmt(Node *n);
static void gen_addr(Node *n);
static void gen_binary64(Node *n);
static void load(Type *ty);
static void emit_addimm(const char *dst, const char *src, int imm);

/* Call a runtime helper (memcpy, __divdi3, ...) with its args already in r0-r3: realign sp to 8 (AAPCS) around
 * the call — the stack machine's one-word pushes may leave it 4-aligned — and restore it from a saved slot. */
static int log2i(int n) { int k = 0; while ((1 << k) < n) k++; return k; }
static void emit_libcall(const char *fn) {   /* saves the DISTANCE to the old sp (relative: survives an alloca slide) */
	fprintf(o, "\tmov ip, sp\n\tbic sp, sp, #7\n\tsub sp, sp, #8\n\tsub ip, ip, sp\n\tstr ip, [sp]\n\tbl %s\n\tldr ip, [sp]\n\tadd sp, sp, ip\n", fn);
}
/* `reg = &sym`: movw/movt (no literal pool, so no +-4095 range limit) — or under -mword-relocations (GCC: only
 * word-sized absolute relocations, which is all a self-relocating -pie image like U-Boot fixes up) a literal word
 * inline, branched over. */
static void sym_addr(const char *reg, const char *sym) {
	if (!word_relocs) { fprintf(o, "\tmovw %s, #:lower16:%s\n\tmovt %s, #:upper16:%s\n", reg, sym, reg, sym); return; }
	int l = uniq();
	fprintf(o, "\tldr %s, .L%d\n\tb .L%da\n.L%d:\n\t.word %s\n.L%da:\n", reg, l, l, l, sym, l);
}
static Func *gen_prog;   /* every function (a non-local goto looks up the frame of its label's function;
                          * -fPIC asks whether a name is this file's own) */
/* Can another module interpose this symbol? Under -fPIC only a preemptible symbol needs the GOT; a
 * non-preemptible one (a local label — string/compound literal, static local —, a `static` definition, or
 * one hidden/internal from other modules) is addressed PC-relative, as GCC does. The difference is not an
 * optimization: a PC-relative address is correct the instant the code is loaded, while a GOT slot still
 * holds a link-time address until something relocates it. Linux's decompressor runs C code
 * (fdt_check_mem_start, atags_to_fdt) BEFORE head.S fixes up .got, and relies on literals being reachable
 * there — "none of the code we're about to call uses any global variables" (head.S). */
static int sym_preemptible(const char *name) {
	if (name[0] == '.') return 0;                                 /* .LSTR<n> / .Lcl<n>: this file's own label */
	for (Gvar *g = globals; g; g = g->next)
		if (!strcmp(g->name, name))
			return g->is_extern || !(g->is_static || vis_local(g->attr.vis ? g->attr.vis : default_vis));
	for (Func *f = gen_prog; f; f = f->next)
		if (!strcmp(f->name, name))
			return !(f->is_static || vis_local(f->attr.vis ? f->attr.vis : default_vis));
	return 1;                                                     /* not defined here: another module's */
}
/* `reg = &sym` PC-relative. The `add` sits exactly 8 bytes before the inline literal, so its pc equals the
 * literal's own address and `.word sym - .` (R_ARM_REL32) is simply sym - pc. A branch skips the literal. */
static void sym_addr_pcrel(const char *reg, const char *sym) {
	int l = uniq();
	fprintf(o, "\tldr %s, .L%d\n\tadd %s, pc, %s\n\tb .L%da\n.L%d:\n\t.word %s - .\n.L%da:\n",
	        reg, l, reg, reg, l, l, sym, l);
}
/* Materialize a 32-bit constant into r0 with movw (+movt for the high half) — no literal pool needed. */
static void load_imm(const char *reg, long v) {
	unsigned u = (unsigned)v;
	fprintf(o, "\tmovw %s, #%u\n", reg, u & 0xffff);
	if (u >> 16) fprintf(o, "\tmovt %s, #%u\n", reg, (u >> 16) & 0xffff);
}

/* Emit `cmp r0,r1` then set r0 to 0/1 by the given condition — the shape of every comparison operator. */
static void gen_setcc(const char *cc) {
	fprintf(o, "\tcmp r0, r1\n\tmov r0, #0\n\tmov%s r0, #1\n", cc);
}

/* Is a scalar type unsigned AFTER integer promotion? (char/short promote to signed int; unsigned int/long/
 * long long stay unsigned.) Drives lsr-vs-asr and the shift result sign. */
static int uns(Type *t) { return t && t->is_unsigned && t->size >= 4; }
/* Signedness of a binary op under the usual arithmetic conversions (correct across mixed 32/64 widths). */
static int op_uns(Node *n) {   /* an address compares UNSIGNED: kernel pointers are >= 0x80000000 (`NULL < p` must hold) */
	Type *l = n->lhs->type, *r = n->rhs->type;
	if (l->kind == TY_PTR || l->kind == TY_ARRAY || l->fn_ret || r->kind == TY_PTR || r->kind == TY_ARRAY || r->fn_ret) return 1;
	return usual_arith(l, r)->is_unsigned;
}

/* A 64-bit value lives in the register PAIR r0(low):r1(high); everything else lives in r0. */
static int is64(Type *t) { return t && t->size == 8; }

int arg_words(Type *t) {
	if (!t) return 1;
	if (is_aggr(t)) return (t->size + 3) / 4;
	return (t->kind != TY_ARRAY && t->size == 8) ? 2 : 1;   /* an array argument decays to a pointer */
}
/* A VFP "co-processor register candidate": float / double / a 64- or 128-bit vector, or a homogeneous aggregate of
 * 1-4 of one of them (nested structs/arrays allowed, no bitfields, no padding). Its class: 1 (float elements) /
 * 2 (double) / 3 (64-bit vectors, any lanes) / 4 (128-bit vectors) / 0. An element takes vfp_regs(class)
 * consecutive s-registers, aligned to that count (a double an even pair, a 128-bit vector a q register). */
static int vfp_regs(int k) { return k == 1 ? 1 : k == 4 ? 4 : 2; }
static int hfa_scan(Type *t, int *kind, int *nel) {
	int k = t->kind == TY_FLOAT ? 1 : t->kind == TY_DOUBLE ? 2 : is_vec(t) && t->size == 8 ? 3 : is_vec(t) && t->size == 16 ? 4 : 0;
	if (k) { if (*kind && *kind != k) return 0; *kind = k; (*nel)++; return 1; }
	if (t->kind == TY_ARRAY || (is_cplx(t) && is_fp(t->base))) { for (int i = 0; i < t->len; i++) if (!hfa_scan(t->base, kind, nel)) return 0; return t->len > 0; }   /* a complex float/double: 2 elements */
	if (t->kind == TY_STRUCT) { int any = 0; for (Member *m = t->members; m; m = m->next) { if (m->promoted) continue; if (m->is_bitfield || !hfa_scan(m->type, kind, nel)) return 0; any = 1; } return any; }
	return 0;
}
int vfp_class(Type *t, int *nel) {
	int kind = 0, n = 0;
	if (!t || t->kind == TY_ARRAY) return 0;   /* an array argument is a pointer */
	if (!hfa_scan(t, &kind, &n) || n > 4 || t->size != n * 4 * vfp_regs(kind)) return 0;
	*nel = n; return kind;
}
/* Returned through a caller-supplied buffer: a struct or complex > 4 bytes (unless a VFP candidate), a vector > 16
 * bytes — a smaller vector comes back in registers (GCC): d0/q0 under VFP (64/128-bit), else r0-r3. */
int is_sret(Type *t, int vfp) {
	int n;
	if (is_vec(t)) return t->size > 16;
	return t && (t->kind == TY_STRUCT || is_cplx(t)) && t->size > 4 && !(vfp && vfp_class(t, &n));   /* a complex: as a struct of its parts */
}
/* AAPCS doubleword alignment of an argument (an even register pair / 8-aligned stack slot), as GCC (9+) decides it: a
 * scalar, vector or complex by its type's NATURAL alignment (an aligned() typedef doesn't count); a struct or union by
 * its members' (a member's aligned() does, the struct's own doesn't). */
static int arg_align8(Type *t) {
	if (!t || t->kind == TY_ARRAY) return 0;   /* an array argument is a pointer */
	if (t->kind == TY_STRUCT) { for (Member *m = t->members; m; m = m->next) if (!m->promoted && m->ealign > 4) return 1; return 0; }
	if (is_vec(t)) return t->size >= 8;
	if (is_cplx(t)) return t->base->size > 4;
	return t->kind != TY_PTR && t->size > 4;
}
/* A class-k VFP candidate of nel elements moved between memory at [base] and s0.. (float elements) / d0.. */
static void vfp_block(const char *op, const char *base, int k, int nel) {
	int ns = nel * vfp_regs(k);
	if (k == 1) fprintf(o, "\t%s %s, {s0-s%d}\n", op, base, ns - 1);
	else fprintf(o, "\t%s %s, {d0-d%d}\n", op, base, ns / 2 - 1);
}
/* Values of vector and complex types move as their bytes: a conversion between different ones is the parser's to
 * lower (GCC's vector rules, see vec_convertible; complex part conversion). This is the net for any it missed. */
static void vec_check(Type *to, Type *from, const char *what) {
	if ((is_vec(to) || is_vec(from)) && !vec_convertible(to, from)) die("cc: incompatible vector types in %s in %s", what, cur_gen_func);
	if ((is_cplx(to) || is_cplx(from)) && !same_cplx(to, from)) die("cc: a complex conversion in %s not lowered in %s", what, cur_gen_func);
}
/* AAPCS argument placement (rules C.1-C.5; C.1.vfp-C.2.vfp when vfp), shared by caller and callee. Core words are
 * numbered in one space: word w < 4 is register rw, w >= 4 is stack word w-4 (pos[i]). NCRN and NSAA advance
 * separately: 8-byte-aligned args start at an even register / stack word; a 64-bit scalar that doesn't fit the
 * registers goes wholly to the stack; a composite may SPLIT across r3 and the stack only while nothing is on the
 * stack yet (so its words stay contiguous in this space). Under vfp a float/double/HFA takes the lowest free
 * run of s0-s15 (a double an even pair; back-filling); once one doesn't fit, all VFP registers close and it and
 * later ones go to the stack. `first` reserves leading core words (the sret pointer). Returns the core words.
 * aapcs_place places one argument after the ones before it (a declaration's parameters, as they're parsed). */
void aapcs_place(Aapcs *a, Type *t, int *pos, int *vreg) {   /* the next argument (a = aapcs_start(...)) */
	int nel, k = a->vfp ? vfp_class(t, &nel) : 0, al8 = arg_align8(t);
	if (vreg) *vreg = -1;
	if (k) {
		int w = vfp_regs(k), need = nel * w, got = -1;
		for (int s = 0; !a->vclosed && s + need <= 16 && got < 0; s += w) if (!((a->sused >> s) & ((1u << need) - 1))) got = s;
		if (got >= 0) { a->sused |= ((1u << need) - 1) << got; *vreg = got; *pos = -1; return; }
		a->vclosed = 1; a->sused = 0xffff;
		if (al8) a->nsaa = (a->nsaa + 1) & ~1;
		*pos = 4 + a->nsaa; a->nsaa += arg_words(t); return;
	}
	int nw = arg_words(t), comp = is_aggr(t);
	if (nw && al8) a->ncrn = (a->ncrn + 1) & ~1;
	if (a->ncrn + nw <= 4) { *pos = a->ncrn; a->ncrn += nw; return; }
	if (comp && a->ncrn < 4 && a->nsaa == 0) { *pos = a->ncrn; a->nsaa = nw - (4 - a->ncrn); a->ncrn = 4; return; }   /* split r3 | stack */
	a->ncrn = 4; if (al8) a->nsaa = (a->nsaa + 1) & ~1;
	*pos = 4 + a->nsaa; a->nsaa += nw;
}
int aapcs_words(const Aapcs *a) { return a->nsaa ? 4 + a->nsaa : a->ncrn; }
int aapcs_layout(Type **ty, int n, int first, int vfp, int *pos, int *vreg) {
	Aapcs a = aapcs_start(first, vfp);
	for (int i = 0; i < n; i++) aapcs_place(&a, ty[i], &pos[i], vreg ? &vreg[i] : NULL);
	return aapcs_words(&a);
}
/* Widen the 32-bit value in r0 into the pair r0:r1 (sign- or zero-extend by the source's sign). */
static void extend64(Type *from) {
	fprintf(o, from && from->is_unsigned ? "\tmov r1, #0\n" : "\tasr r1, r0, #31\n");
}
/* Evaluate n; if the surrounding context wants 64 bits but n is 32-bit, widen r0 into r0:r1. */
static void gen_expr_w(Node *n, int want64) {
	gen_expr(n);
	if (want64 && !is64(n->type)) extend64(n->type);
}
/* ---- floating point: a float/double value lives in r0 / r0:r1 as its IEEE bits (so the stack machine's loads,
 * stores, copies and pushes are unchanged); arithmetic, conversions and compares move it into VFP registers. */
/* About to touch the FP unit. Under -mfloat-abi=soft there is no FP unit in the ABI at all; under
 * -mgeneral-regs-only there is one but its registers are off limits. gcc's own enforcement of the latter is
 * partial (a local double compiles silently, a global one makes it print "unrecognizable insn:" — an ICE), so
 * we are deliberately STRICTER: any floating-point code is refused, which is what the flag promises. */
static void need_fp(void) {
	if (soft_float) die("floating-point code in %s with -mfloat-abi=soft", cur_gen_func);
	if (general_regs_only) die("floating-point code in %s not permitted with '-mgeneral-regs-only'", cur_gen_func);
}
static void to_vfp(int dbl, int r)   { need_fp(); fprintf(o, dbl ? "\tvmov d%d, r0, r1\n" : "\tvmov s%d, r0\n", r); }
static void from_vfp(int dbl, int r) { fprintf(o, dbl ? "\tvmov r0, r1, d%d\n" : "\tvmov r0, s%d\n", r); }
/* Z/N/C/V from comparing the value in r0(:r1) (floating) with zero: Z = equal (a NaN is unordered -> true). */
static void fp_test_zero(int dbl) { to_vfp(dbl, 0); fprintf(o, "\tvcmp.f%d %c0, #0\n\tvmrs APSR_nzcv, fpscr\n", dbl ? 64 : 32, dbl ? 'd' : 's'); }
static void to_bool(Type *dst, Type *src);
static void emit_libcall(const char *fn);
/* The integer divide instructions: -mcpu=cortex-a7 (the target's own, ARMv7VE) has them; -march=armv7-a doesn't — GCC
 * calls the RTABI helpers there (the kernel provides them, and patches them to udiv/sdiv on CPUs that have it). */
static int has_idiv(void) { return opt_march == NULL; }
/* Convert the value in r0(:r1) from type `from` to type `to` — the ONE implicit/explicit conversion routine
 * (assignment, op=, arguments, return, ?: arms, casts). Integer narrowing is left to the store / gen_cast. */
static void conv(Type *from, Type *to) {
	if (!from || !to) return;
	if (is_vec(from) || is_vec(to) || is_cplx(from) || is_cplx(to)) { vec_check(to, from, "a conversion"); return; }   /* the same bytes */
	int ff = is_fp(from), tf = is_fp(to), fd = from->kind == TY_DOUBLE, td = to->kind == TY_DOUBLE;
	if (!ff && !tf) { if (is64(to) && to->kind == TY_LLONG && !is64(from)) extend64(from); to_bool(to, from); return; }
	need_fp();
	if (ff && tf) {
		if (fd == td) return;
		if (td) fprintf(o, "\tvmov s0, r0\n\tvcvt.f64.f32 d0, s0\n\tvmov r0, r1, d0\n");
		else    fprintf(o, "\tvmov d0, r0, r1\n\tvcvt.f32.f64 s0, d0\n\tvmov r0, s0\n");
		return;
	}
	if (tf) {   /* integer -> floating (a narrow integer is already extended in r0) */
		int u = from->is_unsigned && from->size >= 4;
		if (is64(from)) { emit_libcall(u ? (td ? "__aeabi_ul2d" : "__aeabi_ul2f") : (td ? "__aeabi_l2d" : "__aeabi_l2f")); return; }   /* RTABI: base PCS */
		fprintf(o, "\tvmov s0, r0\n\tvcvt.f%d.%c32 %c0, s0\n", td ? 64 : 32, u ? 'u' : 's', td ? 'd' : 's'); from_vfp(td, 0);
		return;
	}
	/* floating -> integer: truncate toward zero */
	if (to->is_bool) { fp_test_zero(fd); fprintf(o, "\tmov r0, #0\n\tmovne r0, #1\n"); return; }
	if (is64(to)) { emit_libcall(to->is_unsigned ? (fd ? "__aeabi_d2ulz" : "__aeabi_f2ulz") : (fd ? "__aeabi_d2lz" : "__aeabi_f2lz")); return; }
	to_vfp(fd, 0);
	fprintf(o, "\tvcvt.%c32.f%d s0, %c0\n\tvmov r0, s0\n", to->is_unsigned && to->size == 4 ? 'u' : 's', fd ? 64 : 32, fd ? 'd' : 's');
}
static void gen_as(Node *e, Type *to) { gen_expr(e); conv(e->type, to); }   /* evaluate e as a value of type `to` */
/* Values the code holds on the stack (pushed operands, a call's staged arguments) or in a statement's frame temporaries
 * while it evaluates a subexpression. A returns-twice call (setjmp, vfork) must come with none: after it returns the
 * first time the code goes on, reusing those slots, so a second return would find them overwritten. */
static int live_temps;
#define WITH_TEMPS(stmt) do { live_temps++; stmt; live_temps--; } while (0)
static int is_twice_call(Node *n) {   /* a direct call of a returns-twice function, or __builtin_setjmp */
	if (n->kind != ND_CALL || n->lhs || n->chain || !n->name[0]) return 0;
	return !strncmp(n->name, "__builtin_", 10) ? !strcmp(n->name, "__builtin_setjmp") : func_returns_twice(n->name);
}
static void twice_check(Node *n) {
	if (live_temps) die("cc: %s() in %s inside an expression that holds values on the stack: it returns twice — "
	                    "call it as a statement, a condition, an argument, or `x = %s(...)`", n->name, cur_gen_func, n->name);
}
/* A binary operator on floating operands: both converted to the common type, into d0/d1 (s0/s1). */
static void gen_fp_binary(Node *n) {
	int cmp = n->kind == ND_EQ || n->kind == ND_NE || n->kind == ND_LT || n->kind == ND_LE || n->kind == ND_GT || n->kind == ND_GE;
	Type *T = cmp ? usual_arith(n->lhs->type, n->rhs->type) : n->type;
	int dbl = T->kind == TY_DOUBLE, bits = dbl ? 64 : 32; char k = dbl ? 'd' : 's';
	gen_as(n->lhs, T); fprintf(o, dbl ? "\tpush {r0, r1}\n" : "\tpush {r0}\n");
	WITH_TEMPS(gen_as(n->rhs, T)); to_vfp(dbl, 1);
	fprintf(o, dbl ? "\tpop {r0, r1}\n" : "\tpop {r0}\n"); to_vfp(dbl, 0);
	if (cmp) {   /* vcmpe (signals on NaN) for ordering, vcmp for equality — as GCC; the conditions are false when unordered */
		const char *cc = n->kind == ND_EQ ? "eq" : n->kind == ND_NE ? "ne" : n->kind == ND_LT ? "mi" : n->kind == ND_LE ? "ls" : n->kind == ND_GT ? "gt" : "ge";
		fprintf(o, "\tvcmp%s.f%d %c0, %c1\n\tvmrs APSR_nzcv, fpscr\n\tmov r0, #0\n\tmov%s r0, #1\n",
		        n->kind == ND_EQ || n->kind == ND_NE ? "" : "e", bits, k, k, cc);
		return;
	}
	const char *op = n->kind == ND_ADD ? "vadd" : n->kind == ND_SUB ? "vsub" : n->kind == ND_MUL ? "vmul" : n->kind == ND_DIV ? "vdiv" : NULL;
	if (!op) die("cc: operator (node %d) is not defined on floating operands", n->kind);
	fprintf(o, "\t%s.f%d %c0, %c0, %c1\n", op, bits, k, k, k); from_vfp(dbl, 0);
}
/* Evaluate a condition and set Z from its truth: a 64-bit value is true if EITHER word is nonzero. */
static void gen_test(Node *n) {
	if (is_cplx(n->type)) {   /* a complex is true when either part is nonzero (a NaN part counts as nonzero) */
		Type *e = n->type->base; int w = e->size, t = uniq();
		gen_expr(n); fprintf(o, "\tmov r2, r0\n");
		for (int k = 0; k < 2; k++) {
			fprintf(o, "\tmov r0, r2\n"); if (k) emit_addimm("r0", "r0", w);
			load(e);
			if (is_fp(e)) fp_test_zero(e->kind == TY_DOUBLE); else fprintf(o, w == 8 ? "\torrs r0, r0, r1\n" : "\tcmp r0, #0\n");
			if (!k) fprintf(o, "\tbne .L%d\n", t);
		}
		fprintf(o, ".L%d:\n", t);   /* Z clear iff a part is nonzero */
		return;
	}
	if (is_aggr(n->type)) die("cc: a %s used where a scalar is required in %s", is_vec(n->type) ? "vector" : "struct", cur_gen_func);
	gen_expr(n);
	if (is_fp(n->type)) { fp_test_zero(n->type->kind == TY_DOUBLE); return; }
	fprintf(o, is64(n->type) ? "\torrs r0, r0, r1\n" : "\tcmp r0, #0\n");
}

/* Load/store through an address by WIDTH: 1/2/4 -> ldrb|ldrsb / ldrh|ldrsh / ldr (narrow loads sign- or
 * zero-extend by the type's sign); 8 -> a register pair (r0=low, r1=high). Stores don't care about sign. */
static void load(Type *ty) {   /* r0=addr -> r0(:r1)=value */
	if      (ty->size == 1) fprintf(o, ty->is_unsigned ? "\tldrb r0, [r0]\n"  : "\tldrsb r0, [r0]\n");
	else if (ty->size == 2) fprintf(o, ty->is_unsigned ? "\tldrh r0, [r0]\n"  : "\tldrsh r0, [r0]\n");
	else if (ty->size == 8) fprintf(o, "\tldr r1, [r0, #4]\n\tldr r0, [r0]\n");   /* high first (r0 is the addr) */
	else                    fprintf(o, "\tldr r0, [r0]\n");
}
static void store(Type *ty) {   /* addr in r1 (32-bit) / r2 (64-bit); value in r0(:r1) */
	if      (ty->size == 1) fprintf(o, "\tstrb r0, [r1]\n");
	else if (ty->size == 2) fprintf(o, "\tstrh r0, [r1]\n");
	else if (ty->size == 8) fprintf(o, "\tstr r0, [r2]\n\tstr r1, [r2, #4]\n");   /* value pair r0:r1, addr r2 */
	else                    fprintf(o, "\tstr r0, [r1]\n");
}
/* Reverse-storage-order scalars (scalar_storage_order("big-endian")): their bytes are big-endian in memory. swap(reg,
 * size) reverses a value's bytes in place; load_lv/store_lv are load/store for an lvalue that may be one. */
static void swap_reg(const char *r, int size) {
	if (size == 2) fprintf(o, "\trev16 %s, %s\n", r, r);
	else if (size == 4) fprintf(o, "\trev %s, %s\n", r, r);
}
static void swap_value(int size) {   /* r0(:r1) */
	if (size == 8) fprintf(o, "\trev ip, r0\n\trev r0, r1\n\tmov r1, ip\n");
	else swap_reg("r0", size);
}
static int reversed(Node *lv) { return lv && (lv->kind == ND_MEMBER || lv->kind == ND_DEREF) && lv->sso; }
static void load_lv(Node *lv, Type *ty) {   /* r0 = &lvalue -> r0(:r1) = its value */
	if (!reversed(lv) || ty->size == 1) { load(ty); return; }
	if (ty->size == 2) { fprintf(o, "\tldrh r0, [r0]\n\trev16 r0, r0\n"); if (!ty->is_unsigned) fprintf(o, "\tsxth r0, r0\n"); return; }
	load(ty); swap_value(ty->size);
}
static void store_lv(Node *lv, Type *ty) {   /* as store(); r0(:r1) keeps the value */
	if (!reversed(lv) || ty->size == 1) { store(ty); return; }
	swap_value(ty->size); store(ty); swap_value(ty->size);
}
/* Convert the value in r0(:r1) of type src for storing into a _Bool: nonzero (either word) -> 1. Other narrow
 * targets need nothing here (the store truncates). */
static void to_bool(Type *dst, Type *src) {
	if (!dst || !dst->is_bool) return;
	if (is64(src)) fprintf(o, "\torr r0, r0, r1\n");
	fprintf(o, "\tcmp r0, #0\n\tmovne r0, #1\n");
}
/* (type) cast on the value in r0: narrowing to char/short truncates + re-extends per the target's sign;
 * widening to int/pointer is a no-op (a narrow load already extended). Non-scalar targets: nothing to do. */
static void gen_cast(Type *ty) {
	if (ty->kind >= TY_PTR) return;
	if      (ty->size == 1) fprintf(o, ty->is_unsigned ? "\tuxtb r0, r0\n" : "\tsxtb r0, r0\n");
	else if (ty->size == 2) fprintf(o, ty->is_unsigned ? "\tuxth r0, r0\n" : "\tsxth r0, r0\n");
}

/* Is v an ARM data-processing modified-immediate (an 8-bit value rotated right by an even amount)? */
static int arm_enc(unsigned v) { for (int r = 0; r < 16; r++) { unsigned s = 2u * r; unsigned x = (v << s) | (v >> ((32 - s) & 31)); if (x <= 0xffu) return 1; } return 0; }
/* Emit `dst = src +/- imm`, materializing a non-encodable immediate via ip (movw/movt) — a struct-member
 * offset or frame size can exceed the ARM modified-immediate range. */
static void emit_addimm(const char *dst, const char *src, int imm) {
	if (imm == 0) { if (strcmp(dst, src)) fprintf(o, "\tmov %s, %s\n", dst, src); return; }
	int neg = imm < 0; unsigned a = neg ? (unsigned)(-imm) : (unsigned)imm;
	if (arm_enc(a)) { fprintf(o, "\t%s %s, %s, #%u\n", neg ? "sub" : "add", dst, src, a); return; }
	fprintf(o, "\tmovw ip, #%u\n", a & 0xffffu); if (a >> 16) fprintf(o, "\tmovt ip, #%u\n", a >> 16);
	fprintf(o, "\t%s %s, %s, ip\n", neg ? "sub" : "add", dst, src);
}
/* ldr/str `reg, [base, #off]`; an offset past the 12-bit immediate goes through ip (so reg may be ip/sp only
 * as the loaded register, never as base). */
static void mem_op(const char *op, const char *reg, const char *base, int off) {
	if (off > -4096 && off < 4096) { fprintf(o, "\t%s %s, [%s, #%d]\n", op, reg, base, off); return; }
	emit_addimm("ip", base, off); fprintf(o, "\t%s %s, [ip]\n", op, reg);
}
/* Copy `size` bytes [src+soff] -> [dst+doff]; dst/src must not be r2 (the scratch). Big copies call memcpy
 * (as GCC does), which clobbers r0-r3/r12/lr — callers treat every register as clobbered. */
static void emit_copy(const char *dst, int doff, const char *src, int soff, int size) {
	if (size > 64 || doff + size > 4095 || soff + size > 4095) {   /* big, or past the ldr/str immediate range */
		fprintf(o, "\tmov r3, %s\n", src); emit_addimm("r0", dst, doff); emit_addimm("r1", "r3", soff);
		load_imm("r2", size); emit_libcall("memcpy"); return;
	}
	int i;
	for (i = 0; i + 4 <= size; i += 4) fprintf(o, "\tldr r2, [%s, #%d]\n\tstr r2, [%s, #%d]\n", src, soff + i, dst, doff + i);
	for (; i < size; i++) fprintf(o, "\tldrb r2, [%s, #%d]\n\tstrb r2, [%s, #%d]\n", src, soff + i, dst, doff + i);
}
/* r0 = the frame `hops` static-chain links up (0: this one): each nested function keeps its chain at [r11, #-4]. */
static void chain_frame(int hops) { fprintf(o, "\tmov r0, r11\n"); while (hops-- > 0) fprintf(o, "\tldr r0, [r0, #-4]\n"); }
/* Put the ADDRESS of an lvalue in r0. A variable's address is fp+offset; *p's address is p's value. */
static void gen_addr(Node *n) {
	switch (n->kind) {
	case ND_VAR:   /* fp-relative: locals are below fp (negative), stack params above it (positive); an enclosing
	                * function's (nested functions) relative to its frame, up the static chain */
		if (n->chain) { chain_frame(n->chain); emit_addimm("r0", "r0", n->offset); }
		else emit_addimm("r0", "r11", n->offset);
		if (n->vla_obj) fprintf(o, "\tldr r0, [r0]\n");   /* a VLA: the slot holds its block's address */
		return;
	case ND_DEREF: gen_expr(n->lhs); return;                                 /* the pointer value IS the address */
	case ND_MEMBER: gen_addr(n->lhs); emit_addimm("r0", "r0", n->offset); return;
	case ND_GVAR: {
		if (n->tls) {
			/* A thread-local object: its address is the thread pointer (TPIDRURO) plus an offset. Each literal sits
			 * 8 bytes after its `add` so pc there is the literal's own address (P), as for the GOT below.
			 *   -fPIC:        general-dynamic — r0 = &GOT pair (module, offset); __tls_get_addr(r0) = &sym
			 *   local def:    local-exec      — the offset itself, fixed at link time (an executable)
			 *   extern:       initial-exec    — the offset from a GOT slot (another module may define it) */
			int g = ngot++;
			if (pic) {
				fprintf(o, "\tldr r0, .LTLS%d_%d\n\tadd r0, pc, r0\n\tb .LTA%d_%d\n.LTLS%d_%d:\n\t.word %s(tlsgd)\n.LTA%d_%d:\n",
				        cur_func_id, g, cur_func_id, g, cur_func_id, g, asm_name(n->name), cur_func_id, g);
				emit_libcall("__tls_get_addr");
			} else if (n->tls_local)
				fprintf(o, "\tldr ip, .LTLS%d_%d\n\tb .LTA%d_%d\n.LTLS%d_%d:\n\t.word %s(tpoff)\n.LTA%d_%d:\n"
				           "\tmrc p15, 0, r0, c13, c0, 3\n\tadd r0, r0, ip\n",
				        cur_func_id, g, cur_func_id, g, cur_func_id, g, asm_name(n->name), cur_func_id, g);
			else
				fprintf(o, "\tldr ip, .LTLS%d_%d\n\tadd ip, pc, ip\n\tb .LTA%d_%d\n.LTLS%d_%d:\n\t.word %s(gottpoff)\n.LTA%d_%d:\n"
				           "\tldr ip, [ip]\n\tmrc p15, 0, r0, c13, c0, 3\n\tadd r0, r0, ip\n",
				        cur_func_id, g, cur_func_id, g, cur_func_id, g, asm_name(n->name), cur_func_id, g);
			return;
		}
		if (pic && !sym_preemptible(n->name)) { sym_addr_pcrel("r0", asm_name(n->name)); return; }
		if (pic) {
			/* PIC: r0 = &sym via the GOT. The `add` sits exactly 8 bytes before the inline literal so
			 * its pc equals the literal's address (P) — the R_ARM_GOT_PREL is then just GOT(sym)-P.
			 * A branch skips the literal so it isn't executed. No absolute address touches .text. */
			int g = ngot++;
			fprintf(o,
			    "\tldr r0, .LGOT%d_%d\n"        /* r0 = GOT(sym) - P  (P = address of the .word below) */
			    "\tadd r0, pc, r0\n"            /* r0 = &GOT[sym]     (pc here == P)                    */
			    "\tb .LGA%d_%d\n"               /* skip the inline literal                              */
			    ".LGOT%d_%d:\n\t.word %s(GOT_PREL)\n"   /* GAS: (GOT_PREL) = R_ARM_GOT_PREL; (GOT) would be R_ARM_GOT32 */
			    ".LGA%d_%d:\n"
			    "\tldr r0, [r0]\n",             /* r0 = GOT[sym] = &sym                                 */
			    cur_func_id, g, cur_func_id, g, cur_func_id, g, asm_name(n->name), cur_func_id, g);
			return;
		}
		sym_addr("r0", asm_name(n->name));
		return;
	}
	case ND_STMTEXPR:   /* ({ ...; lvalue; }) as an lvalue: run the body, take the last expr's address */
		for (Node *s = n->body; s; s = s->next) {
			if (!s->next && s->kind == ND_EXPRSTMT) { gen_addr(s->lhs); return; }
			gen_stmt(s);
		}
		die("cc: statement-expression has no lvalue result");
		return;
	case ND_CALL: if (!is_aggr(n->type)) break; gen_expr(n); return;   /* f().m: the call's result temp */
	case ND_ADDR: gen_addr(n->lhs); return;                                  /* &(addr-expr): a function designator is ND_ADDR(GVAR), so `&func` == `func` (and `&*p` == p) */
	default: break;
	}
	die("cc: not an lvalue (nodekind=%d) in %s", n->kind, cur_gen_func);
}

/* Variable 64-bit shift of the pair r0:r1 by the count in r2 (0..63), result in r0:r1. Uses the canonical
 * predicated sequence (no branches): `subs r12,r2,#32` splits the count<32 (mi) and count>=32 (pl) cases;
 * r3 = 32-count feeds the cross-word carry. `right`=1 shifts right (`arith`=1 -> asr sign fill, else lsr);
 * `right`=0 is left. r12/r3 are caller-saved scratch. */
static void gen_shift64(int right, int arith) {
	fprintf(o, "\tsubs r12, r2, #32\n\trsb r3, r2, #32\n");
	if (!right)          /* LSL */
		fprintf(o, "\tmovpl r1, r0, lsl r12\n\tmovmi r1, r1, lsl r2\n\torrmi r1, r1, r0, lsr r3\n\tlsl r0, r0, r2\n");
	else if (arith)      /* ASR (signed) */
		fprintf(o, "\tmovpl r0, r1, asr r12\n\tmovmi r0, r0, lsr r2\n\torrmi r0, r0, r1, lsl r3\n\tasr r1, r1, r2\n");
	else                 /* LSR (unsigned) */
		fprintf(o, "\tmovpl r0, r1, lsr r12\n\tmovmi r0, r0, lsr r2\n\torrmi r0, r0, r1, lsl r3\n\tlsr r1, r1, r2\n");
}

/* A binary op whose operands are 64-bit: evaluate lhs -> r0:r1 (widened), stash it, evaluate rhs -> r2:r3,
 * restore lhs, then combine into r0:r1 (a comparison instead leaves a 32-bit 0/1 in r0). Signedness `u`
 * (usual arithmetic conversions) selects lsr/asr, the signed/unsigned divmod helper, and compare conditions. */
/* A 64-bit division by a constant, the ways GCC does it rather than call __aeabi_[u]ldivmod (which the kernel doesn't
 * provide, precisely to catch run-time 64-bit division): by a power of two, shifts and masks (signed: biased toward
 * zero); a zero-extended 32-bit dividend by a 32-bit constant, a 32-bit division. 1: emitted, result in r0:r1. */
static long const_divisor(Node *d, int *ok) {   /* an integer constant expression's value (the divisor as written: `-7`) */
	*ok = 0; if (!d || !d->type || is_fp(d->type)) return 0;
	int a, b; long x, y;
	switch (d->kind) {
	case ND_NUM: *ok = 1; return d->val;
	case ND_CAST: if (!d->lhs->type || is_fp(d->lhs->type)) return 0; return const_divisor(d->lhs, ok);
	case ND_NEG: x = const_divisor(d->lhs, ok); return -x;
	case ND_BITNOT: x = const_divisor(d->lhs, ok); return ~x;
	case ND_ADD: case ND_SUB: case ND_MUL:
		x = const_divisor(d->lhs, &a); y = const_divisor(d->rhs, &b); *ok = a && b;
		return d->kind == ND_ADD ? x + y : d->kind == ND_SUB ? x - y : x * y;
	default: return 0;
	}
}
static int gen_div64_const(Node *n, int u) {
	int ok; long dv = const_divisor(n->rhs, &ok);
	if (!ok) return 0;
	unsigned long long v = (unsigned long long)dv;
	if (v == 1) { gen_expr_w(n->lhs, 1); if (n->kind == ND_MOD) fprintf(o, "\tmov r0, #0\n\tmov r1, #0\n"); return 1; }
	if (v >= 2 && !(v & (v - 1)) && (u || (long long)v > 0)) {
		int k = __builtin_ctzll(v);
		gen_expr_w(n->lhs, 1);                           /* x in r0:r1 */
		if (u) {
			if (n->kind == ND_DIV) { fprintf(o, "\tmov r2, #%d\n", k); gen_shift64(1, 0); return 1; }
			if (k < 32) fprintf(o, "\tubfx r0, r0, #0, #%d\n\tmov r1, #0\n", k);
			else if (k == 32) fprintf(o, "\tmov r1, #0\n");
			else fprintf(o, "\tubfx r1, r1, #0, #%d\n", k - 32);
			return 1;
		}
		/* signed: b = x < 0 ? 2^k - 1 : 0 in r2:r3; x / 2^k = (x + b) >> k; x % 2^k = x - ((x + b) & -2^k) */
		fprintf(o, "\tasr r3, r1, #31\n");
		if (k < 32) fprintf(o, "\tubfx r2, r3, #0, #%d\n\tmov r3, #0\n", k);
		else if (k == 32) fprintf(o, "\tmov r2, r3\n\tmov r3, #0\n");
		else fprintf(o, "\tmov r2, r3\n\tubfx r3, r3, #0, #%d\n", k - 32);
		if (n->kind == ND_DIV) { fprintf(o, "\tadds r0, r0, r2\n\tadc r1, r1, r3\n\tmov r2, #%d\n", k); gen_shift64(1, 1); return 1; }
		fprintf(o, "\tadds r2, r0, r2\n\tadc r3, r1, r3\n");
		if (k < 32) fprintf(o, "\tbfc r2, #0, #%d\n", k); else { fprintf(o, "\tmov r2, #0\n"); if (k > 32) fprintf(o, "\tbfc r3, #0, #%d\n", k - 32); }
		fprintf(o, "\tsubs r0, r0, r2\n\tsbc r1, r1, r3\n");
		return 1;
	}
	Node *x = n->lhs;   /* (u64)(u32 x) / c: the quotient/remainder of 32-bit values, zero-extended */
	if (x->kind == ND_CAST && x->lhs && x->lhs->type && !is_fp(x->lhs->type) && x->lhs->type->size <= 4 && uns(x->lhs->type)
	    && v >= 2 && v <= 0xffffffffULL) {
		gen_expr(x->lhs); load_imm("r1", (long)v);
		if (has_idiv()) fprintf(o, n->kind == ND_DIV ? "\tudiv r0, r0, r1\n" : "\tudiv r2, r0, r1\n\tmls r0, r2, r1, r0\n");
		else { emit_libcall("__aeabi_uidivmod"); if (n->kind == ND_MOD) fprintf(o, "\tmov r0, r1\n"); }
		fprintf(o, "\tmov r1, #0\n");
		return 1;
	}
	return 0;
}
/* A 32-bit division (and remainder) by a constant: never a divide — a multiply by the divisor's "magic" reciprocal and
 * shifts (Granlund & Montgomery; Hacker's Delight 10-1/10-8), as GCC does. The kernel needs it: its vDSO has no divide
 * helper to call, and a plain `ns / 1000` must not become one. Remainders: n - q*d (mls). */
struct magu { unsigned M; int a, s; };
static struct magu magic_u(unsigned d) {   /* d >= 2, not a power of two, < 2^31 */
	struct magu m = { 0, 0, 0 }; unsigned nc = (unsigned)-1 - (0u - d) % d, q1 = 0x80000000u / nc, r1 = 0x80000000u - q1 * nc;
	unsigned q2 = 0x7fffffffu / d, r2 = 0x7fffffffu - q2 * d, delta; int p = 31;
	do {
		p++;
		if (r1 >= nc - r1) { q1 = 2 * q1 + 1; r1 = 2 * r1 - nc; } else { q1 = 2 * q1; r1 = 2 * r1; }
		if (r2 + 1 >= d - r2) { if (q2 >= 0x7fffffffu) m.a = 1; q2 = 2 * q2 + 1; r2 = 2 * r2 + 1 - d; }
		else { if (q2 >= 0x80000000u) m.a = 1; q2 = 2 * q2; r2 = 2 * r2 + 1; }
		delta = d - 1 - r2;
	} while (p < 64 && (q1 < delta || (q1 == delta && r1 == 0)));
	m.M = q2 + 1; m.s = p - 32; return m;
}
struct mags { int M, s; };
static struct mags magic_s(int d) {   /* |d| >= 2, not a power of two */
	const unsigned two31 = 0x80000000u; unsigned ad = d < 0 ? 0u - (unsigned)d : (unsigned)d, t = two31 + ((unsigned)d >> 31);
	unsigned anc = t - 1 - t % ad, q1 = two31 / anc, r1 = two31 - q1 * anc, q2 = two31 / ad, r2 = two31 - q2 * ad, delta; int p = 31;
	do {
		p++; q1 *= 2; r1 *= 2; if (r1 >= anc) { q1++; r1 -= anc; }
		q2 *= 2; r2 *= 2; if (r2 >= ad) { q2++; r2 -= ad; }
		delta = ad - r2;
	} while (q1 < delta || (q1 == delta && r1 == 0));
	struct mags m = { (int)(q2 + 1), p - 32 }; if (d < 0) m.M = -m.M; return m;
}
static int gen_div32_const(Node *n) {
	int ok; long dv = const_divisor(n->rhs, &ok);
	if (!ok || !n->type || n->type->size > 4) return 0;
	int u = op_uns(n), mod = n->kind == ND_MOD;
	unsigned ud = (unsigned)dv; int sd = (int)dv;
	if (u ? ud == 0 : sd == 0) return 0;   /* (a division by zero: left as it is written) */
	gen_expr(n->lhs);                      /* n in r0 */
	if (mod) fprintf(o, "\tmov r3, r0\n");
	if (u) {
		if (ud == 1) { fprintf(o, mod ? "\tmov r0, #0\n" : ""); return 1; }
		if (!(ud & (ud - 1))) {
			int k = __builtin_ctz(ud);
			if (mod) fprintf(o, "\tubfx r0, r0, #0, #%d\n", k); else fprintf(o, "\tlsr r0, r0, #%d\n", k);
			return 1;
		}
		if (ud >= 0x80000000u) { load_imm("r1", (long)ud); fprintf(o, "\tcmp r0, r1\n\tmovhs r0, #1\n\tmovlo r0, #0\n"); }
		else {
			struct magu m = magic_u(ud); load_imm("r1", (long)m.M);
			if (!m.a) { fprintf(o, "\tumull r2, r0, r0, r1\n"); if (m.s) fprintf(o, "\tlsr r0, r0, #%d\n", m.s); }
			else { fprintf(o, "\tumull r2, r1, r0, r1\n\tsub r0, r0, r1\n\tadd r0, r1, r0, lsr #1\n"); if (m.s > 1) fprintf(o, "\tlsr r0, r0, #%d\n", m.s - 1); }
		}
		if (mod) { load_imm("r1", (long)ud); fprintf(o, "\tmls r0, r0, r1, r3\n"); }
		return 1;
	}
	if (sd == 1 || sd == -1) { if (mod) fprintf(o, "\tmov r0, #0\n"); else if (sd == -1) fprintf(o, "\trsb r0, r0, #0\n"); return 1; }
	unsigned ad = sd < 0 ? 0u - (unsigned)sd : (unsigned)sd;
	if (sd == (int)0x80000000) { fprintf(o, "\tcmp r0, #0x80000000\n\tmoveq r0, #1\n\tmovne r0, #0\n"); }   /* INT_MIN */
	else if (!(ad & (ad - 1))) {           /* +-2^k: (n + (n < 0 ? 2^k - 1 : 0)) >> k, round toward zero */
		int k = __builtin_ctz(ad);
		fprintf(o, "\tasr r1, r0, #31\n\tadd r0, r0, r1, lsr #%d\n\tasr r0, r0, #%d\n", 32 - k, k);
		if (sd < 0) fprintf(o, "\trsb r0, r0, #0\n");
	} else {
		struct mags m = magic_s(sd); load_imm("r1", (long)(unsigned)m.M);
		fprintf(o, "\tsmull r2, r1, r0, r1\n");
		if (sd > 0 && m.M < 0) fprintf(o, "\tadd r1, r1, r0\n"); else if (sd < 0 && m.M > 0) fprintf(o, "\tsub r1, r1, r0\n");
		if (m.s) fprintf(o, "\tasr r1, r1, #%d\n", m.s);
		fprintf(o, "\tadd r0, r1, r1, lsr #31\n");
	}
	if (mod) { load_imm("r1", (long)(unsigned)sd); fprintf(o, "\tmls r0, r0, r1, r3\n"); }
	return 1;
}
static void gen_binary64(Node *n) {
	int u = op_uns(n);
	if ((n->kind == ND_DIV || n->kind == ND_MOD) && gen_div64_const(n, u)) return;
	gen_expr_w(n->lhs, 1); fprintf(o, "\tpush {r0, r1}\n");
	WITH_TEMPS(gen_expr_w(n->rhs, 1)); fprintf(o, "\tmov r2, r0\n\tmov r3, r1\n\tpop {r0, r1}\n");   /* rhs->r2:r3, lhs->r0:r1 */
	switch (n->kind) {
	case ND_ADD:    fprintf(o, "\tadds r0, r0, r2\n\tadc r1, r1, r3\n"); return;
	case ND_SUB:    fprintf(o, "\tsubs r0, r0, r2\n\tsbc r1, r1, r3\n"); return;
	case ND_BITAND: fprintf(o, "\tand r0, r0, r2\n\tand r1, r1, r3\n"); return;
	case ND_BITOR:  fprintf(o, "\torr r0, r0, r2\n\torr r1, r1, r3\n"); return;
	case ND_BITXOR: fprintf(o, "\teor r0, r0, r2\n\teor r1, r1, r3\n"); return;
	case ND_MUL:    /* low 64 of the product: al*bl (full) + (al*bh + ah*bl)<<32 */
		fprintf(o, "\tmul r1, r1, r2\n\tmla r1, r0, r3, r1\n\tumull r0, r12, r0, r2\n\tadd r1, r1, r12\n"); return;
	case ND_SHL:    gen_shift64(0, 0);   return;   /* shift count is already in r2 (rhs low word) */
	case ND_SHR:    gen_shift64(1, !u);  return;   /* right: logical if unsigned, arithmetic if signed */
	case ND_DIV:    emit_libcall(u ? "__aeabi_uldivmod" : "__aeabi_ldivmod"); return;   /* RTABI: n=r0:r1 d=r2:r3 -> q=r0:r1, r=r2:r3 */
	case ND_MOD:    emit_libcall(u ? "__aeabi_uldivmod" : "__aeabi_ldivmod"); fprintf(o, "\tmov r0, r2\n\tmov r1, r3\n"); return;
	case ND_EQ:     fprintf(o, "\tcmp r0, r2\n\tcmpeq r1, r3\n\tmov r0, #0\n\tmoveq r0, #1\n"); return;
	case ND_NE:     fprintf(o, "\tcmp r0, r2\n\tcmpeq r1, r3\n\tmov r0, #0\n\tmovne r0, #1\n"); return;
	/* ordering via a full 64-bit subtract (subs/sbcs set N,V,C for the whole result); only lt/ge (signed)
	 * or lo/hs (unsigned) are used — they don't need Z, which sbcs sets from the high word alone. a>b and
	 * a<=b subtract in the reverse order (b-a) so the same two conditions cover all four. */
	case ND_LT: fprintf(o, "\tsubs r0, r0, r2\n\tsbcs r1, r1, r3\n\tmov r0, #0\n\tmov%s r0, #1\n", u ? "lo" : "lt"); return;
	case ND_GE: fprintf(o, "\tsubs r0, r0, r2\n\tsbcs r1, r1, r3\n\tmov r0, #0\n\tmov%s r0, #1\n", u ? "hs" : "ge"); return;
	case ND_GT: fprintf(o, "\tsubs r0, r2, r0\n\tsbcs r1, r3, r1\n\tmov r0, #0\n\tmov%s r0, #1\n", u ? "lo" : "lt"); return;
	case ND_LE: fprintf(o, "\tsubs r0, r2, r0\n\tsbcs r1, r3, r1\n\tmov r0, #0\n\tmov%s r0, #1\n", u ? "hs" : "ge"); return;
	default: die("cc: unsupported 64-bit operator (node %d)", n->kind);
	}
}

/* Constant 64-bit shifts of the pair r0:r1 (0 < k < 64). */
static void shl64c(int k) {
	if (k >= 32) { if (k > 32) fprintf(o, "\tlsl r1, r0, #%d\n", k - 32); else fprintf(o, "\tmov r1, r0\n"); fprintf(o, "\tmov r0, #0\n"); }
	else if (k) fprintf(o, "\tlsl r1, r1, #%d\n\torr r1, r1, r0, lsr #%d\n\tlsl r0, r0, #%d\n", k, 32 - k, k);
}
static void shr64c(int k, int arith) {
	const char *sh = arith ? "asr" : "lsr";
	if (k >= 32) { if (k > 32) fprintf(o, "\t%s r0, r1, #%d\n", sh, k - 32); else fprintf(o, "\tmov r0, r1\n"); fprintf(o, arith ? "\tasr r1, r1, #31\n" : "\tmov r1, #0\n"); }
	else if (k) fprintf(o, "\tlsr r0, r0, #%d\n\torr r0, r0, r1, lsl #%d\n\t%s r1, r1, #%d\n", k, 32 - k, sh, k);
}
/* A field needs the 64-bit path when its unit is 8 bytes (long long fields) or, in a packed struct, it
 * spans past the first 32 bits of its unit. */
static int bf_wide(Node *m) { return m->bf_type->size == 8 || m->bit_offset + m->bit_width > 32; }
/* A PACKED field (`bf_dense`) was laid out bit by bit, so its declared unit need not be aligned and may
 * reach PAST THE END of the object: `struct __attribute__((packed)) { unsigned f : 20; }` is 3 bytes with a
 * 4-byte unit, and `{ char pad; unsigned f : 20; }` is 4 bytes with its unit at byte 1. Neither the
 * unit-wide access below nor bf_wide's two words may be used there — a store would write a byte past the
 * object and corrupt whatever follows. Touch only the bytes the field itself occupies, one at a time, as
 * GCC does: byte `lo` of the unit for `nb` bytes, the field starting `sbo` bits into byte lo. */
static void bf_span(Node *n, int *lo, int *nb, int *sbo) {
	int bo = n->bit_offset, hi = (bo + n->bit_width - 1) / 8;
	*lo = bo / 8; *nb = hi - *lo + 1; *sbo = bo - 8 * *lo;
}
static void bf_dense_check(Node *n) {
	int lo, nb, sbo; bf_span(n, &lo, &nb, &sbo);
	if (n->sso) die("cc: a packed bit-field in a reverse-storage-order struct is not supported");
	if (sbo + n->bit_width > 64) die("cc: a packed bit-field of %d bits at bit %d spans more than 64", n->bit_width, n->bit_offset);
}
/* Bitfield read: r0 = &storage-unit on entry -> r0(:r1) = the field value. Two shifts isolate the field —
 * left so its top bit reaches the top, then right (arithmetic if signed) down to bit 0. */
static int bf_bo(Node *n) {   /* the field's bit offset in its unit's VALUE: big-endian bit order in a reversed unit */
	return n->sso ? 8 * n->bf_type->size - n->bit_offset - n->bit_width : n->bit_offset;
}
static void gen_bitfield_load(Node *n) {
	int sz = n->bf_type->size, bo = bf_bo(n), bw = n->bit_width, sg = !n->bf_type->is_unsigned;
	if (n->bf_dense) {                                   /* only the field's own bytes (see bf_span) */
		bf_dense_check(n);
		int lo, nb, sbo; bf_span(n, &lo, &nb, &sbo);
		fprintf(o, "\tmov r3, r0\n\tmov r0, #0\n\tmov r1, #0\n");
		for (int i = nb - 1; i >= 0; i--) {              /* little-endian: the span's top byte first */
			if (i != nb - 1) shl64c(8);
			fprintf(o, "\tldrb ip, [r3, #%d]\n\torr r0, r0, ip\n", lo + i);
		}
		shl64c(64 - sbo - bw); shr64c(64 - bw, sg);      /* isolate, then sign/zero-extend down to bit 0 */
		return;
	}
	if (bf_wide(n)) {
		fprintf(o, "\tldr r1, [r0, #4]\n\tldr r0, [r0]\n");
		if (n->sso) swap_value(8);
		shl64c(64 - bo - bw); shr64c(64 - bw, sg);   /* sz <= 4: the (<= 32-bit) value is in r0 */
		return;
	}
	int lsh = 32 - bo - bw, rsh = 32 - bw;
	fprintf(o, sz == 1 ? "\tldrb r0, [r0]\n" : sz == 2 ? "\tldrh r0, [r0]\n" : "\tldr r0, [r0]\n");
	if (n->sso) swap_reg("r0", sz);
	if (lsh) fprintf(o, "\tlsl r0, r0, #%d\n", lsh);
	if (rsh) fprintf(o, sg ? "\tasr r0, r0, #%d\n" : "\tlsr r0, r0, #%d\n", rsh);
}
/* Bitfield put: value r0(:r1 when wide), r2 = &unit -> clear the field bits (bic) and OR the masked, shifted
 * value in, leaving the unit's other bits. Leaves r0 = the field as stored (re-read: truncated to its width),
 * which is the value of the assignment expression. */
static void bitfield_put(Node *lhs) {
	int sz = lhs->bf_type->size, bo = bf_bo(lhs), bw = lhs->bit_width;
	unsigned long long fm = bw >= 64 ? ~0ULL : (1ULL << bw) - 1, pm = fm << bo;   /* field mask, placed mask */
	if (lhs->bf_dense) {                                 /* byte-at-a-time RMW over the field's own bytes */
		bf_dense_check(lhs);
		int lo, nb, sbo; bf_span(lhs, &lo, &nb, &sbo);
		unsigned long long placed = fm << sbo;
		load_imm("r3", (long)(fm & 0xffffffff)); fprintf(o, "\tand r0, r0, r3\n");          /* value &= fieldmask */
		load_imm("r3", (long)(fm >> 32));        fprintf(o, "\tand r1, r1, r3\n");          /* (clears r1 for a <=32-bit field) */
		shl64c(sbo);                                     /* the pair's byte k is now the byte for unit+lo+k */
		for (int k = 0; k < nb; k++) {
			unsigned mb = (unsigned)((placed >> (8 * k)) & 0xff);   /* this byte's share of the field */
			if (mb) fprintf(o, "\tldrb r3, [r2, #%d]\n\tbic r3, r3, #%u\n\tand ip, r0, #%u\n"
			                   "\torr r3, r3, ip\n\tstrb r3, [r2, #%d]\n", lo + k, mb, mb, lo + k);
			if (k != nb - 1) shr64c(8, 0);
		}
		fprintf(o, "\tmov r0, r2\n"); gen_bitfield_load(lhs);   /* the assignment's value: read it back */
		return;
	}
	if (bf_wide(lhs)) {
		load_imm("r3", fm & 0xffffffff); fprintf(o, "\tand r0, r0, r3\n"); load_imm("r3", fm >> 32); fprintf(o, "\tand r1, r1, r3\n");
		shl64c(bo);
		for (int w = 0; w < 2; w++) {                    /* value word w (0 = low) lives at word 1-w when reversed */
			int at = lhs->sso ? 4 * (1 - w) : 4 * w;
			fprintf(o, "\tldr r3, [r2, #%d]\n", at); if (lhs->sso) swap_reg("r3", 4);
			load_imm("ip", w ? pm >> 32 : pm & 0xffffffff);
			fprintf(o, "\tbic r3, r3, ip\n\torr r3, r3, r%d\n", w);
			if (lhs->sso) swap_reg("r3", 4);
			fprintf(o, "\tstr r3, [r2, #%d]\n", at);
		}
	} else {
		fprintf(o, sz == 1 ? "\tldrb r3, [r2]\n" : sz == 2 ? "\tldrh r3, [r2]\n" : "\tldr r3, [r2]\n");
		if (lhs->sso) swap_reg("r3", sz);
		load_imm("ip", fm); fprintf(o, "\tand r0, r0, ip\n");      /* value &= fieldmask */
		if (bo) fprintf(o, "\tlsl r0, r0, #%d\n\tlsl ip, ip, #%d\n", bo, bo);   /* value + mask into place */
		fprintf(o, "\tbic r3, r3, ip\n\torr r3, r3, r0\n");
		if (lhs->sso) swap_reg("r3", sz);
		fprintf(o, sz == 1 ? "\tstrb r3, [r2]\n" : sz == 2 ? "\tstrh r3, [r2]\n" : "\tstr r3, [r2]\n");
	}
	fprintf(o, "\tmov r0, r2\n"); gen_bitfield_load(lhs);
}
static void gen_bitfield_store(Node *n) {
	gen_addr(n->lhs); fprintf(o, "\tpush {r0}\n");
	WITH_TEMPS(gen_as(n->rhs, n->lhs->bf_type)); fprintf(o, "\tpop {r2}\n");
	bitfield_put(n->lhs);
}
static void fp_mem(const char *op, const char *reg, int off) { mem_op(op, reg, "r11", off); }   /* a frame slot [r11, #off] */

/* GCC builtins that reach codegen with a non-constant argument (constant ones were folded by the parser).
 * They are never real functions — emitting `bl __builtin_clz` leaves an undefined symbol — so expand inline,
 * with the same instructions GCC uses on ARMv7. Anything unhandled is a hard error. */
static long builtin_const_arg(Node *n) {
	if (!n->args || n->args->kind != ND_NUM) die("cc: %s needs a constant argument", n->name);
	return n->args->val;
}
static int cur_alloca_slot;   /* frame slot holding the alloca floor (0 = the function uses no alloca) */
static Func *find_func(Func *prog, const char *name);
static Node *vla_marks[256]; static int nvla_marks;   /* this function's VLA declarations, in source order */
static int cur_setjmp_save;   /* frame slots for r4-r10 in a function using __builtin_setjmp (0 = none) */
static void gen_builtin(Node *n) {
	const char *b = n->name + 10;   /* after "__builtin_" */
	if (!strcmp(b, "alloca")) {
		/* The temporaries live in [sp, floor); the new block goes right under floor, so slide the temporaries down
		 * by the (8-rounded) size, then floor -= size. Nothing holds an absolute pointer into the temporaries
		 * (call sites restore sp relatively), so the slide is invisible. */
		if (!cur_alloca_slot || !n->args || n->args->next) die("cc: alloca: bad call");
		int lp = uniq(), done = uniq();
		gen_expr(n->args);
		fprintf(o, "\tadd r0, r0, #7\n\tbic r0, r0, #7\n"); fp_mem("ldr", "r2", cur_alloca_slot);
		fprintf(o, "\tsub r1, r2, sp\n\tmov r3, sp\n\tsub sp, sp, r0\n\tsub r2, r2, r0\n\tmov ip, #0\n"
		           ".L%d:\n\tcmp ip, r1\n\tbeq .L%d\n\tldr r0, [r3, ip]\n\tstr r0, [sp, ip]\n\tadd ip, ip, #4\n\tb .L%d\n.L%d:\n", lp, done, lp, done);
		fp_mem("str", "r2", cur_alloca_slot); fprintf(o, "\tmov r0, r2\n");
		return;
	}
	if (!strcmp(b, "return_address")) {   /* level 0 = our saved lr ([fp,#4] after `push {r11, lr}`); deeper: 0, like GCC on ARM */
		fprintf(o, builtin_const_arg(n) == 0 ? "\tldr r0, [r11, #4]\n" : "\tmov r0, #0\n"); return; }
	if (!strcmp(b, "frame_address")) { fprintf(o, builtin_const_arg(n) == 0 ? "\tmov r0, r11\n" : "\tmov r0, #0\n"); return; }
	if (!strcmp(b, "setjmp")) {   /* GCC's builtin: buf[0] = fp, buf[1] = resume address, buf[2] = sp; 0 now, 1 via longjmp */
		int land = uniq(), done = uniq();
		if (!n->args) die("cc: __builtin_setjmp needs a buffer");
		twice_check(n);
		gen_expr(n->args);
		char lab[32]; snprintf(lab, sizeof lab, ".L%d", land);
		fprintf(o, "\tstr r11, [r0]\n\tstr sp, [r0, #8]\n"); sym_addr("r1", lab);
		fprintf(o, "\tstr r1, [r0, #4]\n\tmov r0, #0\n\tb .L%d\n.L%d:\n\tmov r0, #1\n.L%d:\n", done, land, done);
		return;
	}
	if (!strcmp(b, "longjmp")) {   /* (buf, 1): resume at buf's setjmp with its fp + sp */
		if (!n->args || !n->args->next) die("cc: __builtin_longjmp needs (buffer, 1)");
		if (n->args->next->kind != ND_NUM || n->args->next->val != 1) die("cc: __builtin_longjmp's value must be the constant 1");
		gen_expr(n->args);
		fprintf(o, "\tldr r1, [r0, #4]\n\tldr r11, [r0]\n\tldr sp, [r0, #8]\n\tbx r1\n");
		return;
	}
	if (!strncmp(b, "fabs", 4)) {   /* clear the sign bit */
		int dbl = b[4] != 'f'; if (!n->args) die("cc: %s needs an argument", n->name);
		gen_as(n->args, dbl ? ty_double : ty_float); need_fp(); fprintf(o, dbl ? "\tbic r1, r1, #0x80000000\n" : "\tbic r0, r0, #0x80000000\n"); return;
	}
	if (!strncmp(b, "copysign", 8)) {   /* x's magnitude, y's sign */
		int dbl = b[8] != 'f'; Type *T = dbl ? ty_double : ty_float;
		if (!n->args || !n->args->next) die("cc: %s needs two arguments", n->name);
		gen_as(n->args->next, T); fprintf(o, "\tpush {%s}\n", dbl ? "r1" : "r0");   /* the word holding y's sign */
		WITH_TEMPS(gen_as(n->args, T)); need_fp(); fprintf(o, "\tpop {r2}\n\tand r2, r2, #0x80000000\n");
		fprintf(o, dbl ? "\tbic r1, r1, #0x80000000\n\torr r1, r1, r2\n" : "\tbic r0, r0, #0x80000000\n\torr r0, r0, r2\n"); return;
	}
	if (!strncmp(b, "signbit", 7)) {   /* type-generic (signbit) or fixed (signbitf/l): the sign bit, as 0/1 */
		if (!n->args) die("cc: %s needs an argument", n->name);
		Type *T = b[7] == 'f' ? ty_float : b[7] == 'l' ? ty_double : n->args->type;
		if (!is_fp(T)) die("cc: %s needs a floating argument", n->name);
		gen_as(n->args, T); fprintf(o, "\tlsr r0, %s, #31\n", T->kind == TY_DOUBLE ? "r1" : "r0"); return;
	}
	if (!strncmp(b, "fma", 3) && (!b[3] || !strcmp(b + 3, "f") || !strcmp(b + 3, "l"))) {   /* x*y + z, rounded once (VFPv4 vfma) */
		int dbl = b[3] != 'f'; Type *T = dbl ? ty_double : ty_float; char k = dbl ? 'd' : 's';
		if (!n->args || !n->args->next || !n->args->next->next || n->args->next->next->next) die("cc: %s needs three arguments", n->name);
		const char *pr = dbl ? "{r0, r1}" : "{r0}";
		gen_as(n->args, T); fprintf(o, "\tpush %s\n", pr); gen_as(n->args->next, T); fprintf(o, "\tpush %s\n", pr);
		gen_as(n->args->next->next, T); to_vfp(dbl, 2);
		fprintf(o, "\tpop %s\n", pr); to_vfp(dbl, 1); fprintf(o, "\tpop %s\n", pr); to_vfp(dbl, 0);
		fprintf(o, "\tvfma.f%d %c2, %c0, %c1\n", dbl ? 64 : 32, k, k, k); from_vfp(dbl, 2);
		return;
	}
	/* GCC's untyped call forwarding. apply_args' block (a frame temp): [0] the incoming core argument words (the homed
	 * r0-r3, contiguous with the stack arguments), [4] the saved d0-d7 (0 if none came in VFP registers). apply(fn,
	 * block, size) calls fn with those registers and `size` bytes of the stack arguments, keeping the result
	 * registers in its own frame temp — [16] r0, [20] r1, [24] d0-d3 — which it returns; return(result) returns them. */
	/* The stack level, as the alloca floor (the stack machine's temporaries live below it): save reads it, restore
	 * frees everything allocated since (a VLA / alloca block) — sp and the floor back to it. */
	if (!strcmp(b, "stack_save")) { if (!cur_alloca_slot) die("cc: internal: no alloca floor for __builtin_stack_save"); fp_mem("ldr", "r0", cur_alloca_slot); return; }
	if (!strcmp(b, "stack_restore")) {
		if (!n->args || n->args->next || !cur_alloca_slot) die("cc: __builtin_stack_restore needs one pointer");
		gen_expr(n->args); fprintf(o, "\tmov sp, r0\n"); fp_mem("str", "r0", cur_alloca_slot); return;
	}
	if (!strcmp(b, "apply_args")) {
		emit_addimm("r0", "r11", cur_core_home); fp_mem("str", "r0", n->offset);
		if (cur_core_home == 72) fprintf(o, "\tadd r0, r11, #8\n"); else fprintf(o, "\tmov r0, #0\n");   /* d0-d7 saved below r0-r3 */
		fp_mem("str", "r0", n->offset + 4); emit_addimm("r0", "r11", n->offset); return;
	}
	if (!strcmp(b, "apply")) {
		if (!n->args || !n->args->next || !n->args->next->next || n->args->next->next->next) die("cc: __builtin_apply needs (function, arguments, size)");
		int t = n->offset, skip = uniq();   /* [0] fn [4] block [8] size [12] sp [16..56) the result registers */
		gen_expr(n->args); fp_mem("str", "r0", t); gen_expr(n->args->next); fp_mem("str", "r0", t + 4); gen_expr(n->args->next->next); fp_mem("str", "r0", t + 8);
		fprintf(o, "\tmov ip, sp\n"); fp_mem("str", "ip", t + 12);
		fprintf(o, "\tadd r0, r0, #7\n\tbic r0, r0, #7\n\tsub sp, sp, r0\n\tbic sp, sp, #7\n");   /* the stack arguments' copy */
		fp_mem("ldr", "r1", t + 4); fprintf(o, "\tldr r1, [r1]\n\tadd r1, r1, #16\n"); fp_mem("ldr", "r2", t + 8); fprintf(o, "\tmov r0, sp\n\tbl memcpy\n");
		fp_mem("ldr", "r1", t + 4); fprintf(o, "\tldr r1, [r1, #4]\n\tcmp r1, #0\n\tbeq .L%d\n\tvldmia r1, {d0-d7}\n.L%d:\n", skip, skip);
		fp_mem("ldr", "r0", t + 4); fprintf(o, "\tldr r0, [r0]\n\tldm r0, {r0, r1, r2, r3}\n");
		fp_mem("ldr", "ip", t); fprintf(o, "\tblx ip\n");
		fp_mem("str", "r0", t + 16); fp_mem("str", "r1", t + 20); emit_addimm("ip", "r11", t + 24); fprintf(o, "\tvstmia ip, {d0-d3}\n");
		fp_mem("ldr", "ip", t + 12); fprintf(o, "\tmov sp, ip\n"); emit_addimm("r0", "r11", t + 16);
		return;
	}
	if (!strcmp(b, "return")) {
		if (!n->args || n->args->next) die("cc: __builtin_return needs a result block");
		gen_expr(n->args); fprintf(o, "\tmov ip, r0\n\tadd r0, ip, #8\n\tvldmia r0, {d0-d3}\n\tldr r1, [ip, #4]\n\tldr r0, [ip]\n\tb .L%d\n", ret_label);
		return;
	}
	if (!strcmp(b, "trap")) { fprintf(o, "\t.inst 0xe7f000f0\n"); return; }   /* GCC's ARM trap: a permanently-undefined insn */
	if (!strcmp(b, "prefetch")) {   /* (addr[, rw[, locality]]): evaluate every argument, prefetch addr */
		if (!n->args) die("cc: __builtin_prefetch needs an address");
		for (Node *a = n->args->next; a; a = a->next) gen_expr(a);
		gen_expr(n->args); fprintf(o, "\tpld [r0]\n"); return;
	}
	if (!strcmp(b, "thread_pointer")) { fprintf(o, "\tmrc p15, 0, r0, c13, c0, 3\n"); return; }   /* TPIDRURO, as GCC reads it */
	if (!n->args || n->args->next) die("cc: unsupported builtin '%s'", n->name);
	gen_expr(n->args);   /* the one operand: r0, or r0:r1 if 64-bit */
	{ Type *pt = func_param_type(n->name, 0), *at = n->args->type;   /* widen a 32-bit arg to a 64-bit param (clzll(int)) */
	  if (is64(pt) && !is64(at)) fprintf(o, (at && at->is_unsigned) ? "\tmov r1, #0\n" : "\tasr r1, r0, #31\n"); }
	if (!strcmp(b, "extract_return_addr")) return;
	if (!strcmp(b, "clz") || !strcmp(b, "clzl")) { fprintf(o, "\tclz r0, r0\n"); return; }
	if (!strcmp(b, "clzll"))   /* hi ? clz(hi) : 32 + clz(lo) */
		{ fprintf(o, "\tclz r2, r1\n\tclz r0, r0\n\tcmp r1, #0\n\taddeq r0, r0, #32\n\tmovne r0, r2\n"); return; }
	if (!strcmp(b, "ctz") || !strcmp(b, "ctzl")) { fprintf(o, "\trbit r0, r0\n\tclz r0, r0\n"); return; }
	if (!strcmp(b, "ctzll"))   /* lo ? ctz(lo) : 32 + ctz(hi) */
		{ fprintf(o, "\trbit r2, r0\n\tclz r2, r2\n\trbit r3, r1\n\tclz r3, r3\n\tcmp r0, #0\n\tmovne r0, r2\n\taddeq r0, r3, #32\n"); return; }
	if (!strcmp(b, "ffs") || !strcmp(b, "ffsl"))   /* x ? ctz(x) + 1 : 0 */
		{ fprintf(o, "\trbit r1, r0\n\tclz r1, r1\n\tcmp r0, #0\n\taddne r0, r1, #1\n"); return; }
	if (!strcmp(b, "ffsll"))   /* lo ? ctz(lo)+1 : hi ? ctz(hi)+33 : 0 */
		{ fprintf(o, "\trbit r2, r0\n\tclz r2, r2\n\trbit r3, r1\n\tclz r3, r3\n\tcmp r0, #0\n\taddne r0, r2, #1\n\tbne 1f\n"
		             "\tcmp r1, #0\n\tmoveq r0, #0\n\taddne r0, r3, #33\n1:\n"); return; }
	if (!strcmp(b, "bswap16")) { fprintf(o, "\trev16 r0, r0\n\tuxth r0, r0\n"); return; }
	if (!strcmp(b, "bswap32")) { fprintf(o, "\trev r0, r0\n"); return; }
	if (!strcmp(b, "bswap64")) { fprintf(o, "\trev r2, r0\n\trev r0, r1\n\tmov r1, r2\n"); return; }
	die("cc: unsupported builtin '%s' (would be an undefined symbol)", n->name);
}

/* A wide bit-field type's arithmetic wraps at its width: cut r0:r1 back to prec bits (zero- or sign-extended). */
static void gen_expr1(Node *n);
static void gen_expr(Node *n) {
	gen_expr1(n);
	Type *t = n->type;
	if (!t || !t->prec || t->size != 8) return;
	switch (n->kind) {
	case ND_ADD: case ND_SUB: case ND_MUL: case ND_DIV: case ND_MOD: case ND_SHL: case ND_SHR:
	case ND_NEG: case ND_BITNOT: case ND_BITAND: case ND_BITOR: case ND_BITXOR:
		fprintf(o, "\tlsl r1, r1, #%d\n\t%s r1, r1, #%d\n", 64 - t->prec, t->is_unsigned ? "lsr" : "asr", 64 - t->prec);
	default: break;
	}
}
static void gen_expr1(Node *n) {
	switch (n->kind) {
	case ND_NUM:                                                 /* 64-bit literal fills the pair r0:r1 */
		if (is64(n->type)) { load_imm("r0", n->val & 0xffffffff); load_imm("r1", (n->val >> 32) & 0xffffffff); }
		else load_imm("r0", n->val);
		return;
	case ND_MEMBER:
		gen_addr(n);
		if (n->bit_width) { gen_bitfield_load(n); return; }              /* bitfield: extract from its unit */
		if (n->type->kind != TY_ARRAY && !is_aggr(n->type)) load_lv(n, n->type);   /* struct member that is itself an aggregate decays to its address */
		return;
	case ND_VAR: case ND_GVAR:                             /* address -> r0; scalars then load; arrays/structs decay to their address */
		gen_addr(n); if (n->type->kind != TY_ARRAY && !is_aggr(n->type)) load(n->type); return;
	case ND_REGVAR: fprintf(o, "\tmov r0, %s\n", n->reg); return;   /* read a global register variable */
	case ND_ADDR: gen_addr(n->lhs); return;                 /* &lvalue -> the address itself */
	case ND_LABELADDR: {                                    /* &&label -> the label's code address (movw/movt) */
		const char *fn = n->owner ? n->owner : cur_gen_func;   /* an enclosing function's __label__ (nested functions) */
		char lab[2 * IDMAX + 16]; snprintf(lab, sizeof lab, CLABEL_FMT, fn, n->name);
		sym_addr("r0", lab);
		return;
	}
	case ND_DEREF: gen_expr(n->lhs);                        /* pointer -> r0, then load the pointee by width */
		if (n->type->kind != TY_ARRAY && !is_aggr(n->type) && !n->type->fn_ret) load_lv(n, n->type);   /* an aggregate *p IS its address; *fp is the function */
		return;
	case ND_ASSIGN:
		if (n->lhs->kind == ND_REGVAR) { gen_expr(n->rhs); fprintf(o, "\tmov %s, r0\n", n->lhs->reg); return; }   /* write a global reg var */
		if (n->lhs->kind == ND_MEMBER && n->lhs->bit_width) { gen_bitfield_store(n); return; }   /* bitfield RMW */
		if (is_aggr(n->lhs->type)) {   /* whole struct/union/vector copy — memcpy, not a scalar store */
			vec_check(n->lhs->type, n->rhs->type, "an assignment");
			if (!is_aggr(n->rhs->type)) die("cc: assigning a scalar to a struct in %s", cur_gen_func);
			if (n->lhs->type->vsize_off) die("cc: internal: a variable-size struct copy in %s", cur_gen_func);   /* the parser's memcpy */
			gen_addr(n->lhs); fprintf(o, "\tpush {r0}\n");   /* dest addr */
			WITH_TEMPS(gen_expr(n->rhs));                    /* a struct-typed rhs leaves its ADDRESS in r0 */
			fprintf(o, "\tpop {r1}\n");                      /* r1 = dest, r0 = src */
			emit_copy("r1", 0, "r0", 0, n->lhs->type->size);
			return;
		}
		if (n->rhs->twice) {                                 /* x = setjmp(b), x = ({ … vfork() … }): the value first, with
		                                                     * nothing on the stack (the operands' order is unspecified) */
			int w = is64(n->lhs->type);
			gen_as(n->rhs, n->lhs->type); fprintf(o, w ? "\tpush {r0, r1}\n" : "\tpush {r0}\n");
			WITH_TEMPS(gen_addr(n->lhs)); fprintf(o, w ? "\tmov r2, r0\n\tpop {r0, r1}\n" : "\tmov r1, r0\n\tpop {r0}\n");
			store_lv(n->lhs, n->lhs->type); if (!w && n->lhs->type->size < 4) gen_cast(n->lhs->type);
			return;
		}
		gen_addr(n->lhs); fprintf(o, "\tpush {r0}\n");      /* destination address */
		WITH_TEMPS(gen_as(n->rhs, n->lhs->type));           /* value in r0(:r1), converted to the dest type */
		if (is64(n->lhs->type)) { fprintf(o, "\tpop {r2}\n"); store_lv(n->lhs, n->lhs->type); }   /* addr r2; str r0:r1 */
		else { fprintf(o, "\tpop {r1}\n"); store_lv(n->lhs, n->lhs->type); if (n->lhs->type->size < 4) gen_cast(n->lhs->type); }   /* addr r1; value = as stored (narrowed) */
		return;                                             /* r0(:r1) keeps the value (assignment result) */
	case ND_CAST:   gen_expr(n->lhs);
		if (n->type->is_bool || is_fp(n->type) || is_fp(n->lhs->type)) { conv(n->lhs->type, n->type); if (!is_fp(n->type)) gen_cast(n->type); return; }
		if (is64(n->type)) { if (!is64(n->lhs->type)) extend64(n->lhs->type); }   /* widen 32->64 (sign/zero) */
		else gen_cast(n->type);                                                   /* 64->32 keeps r0 low word; then narrow to char/short */
		return;
	case ND_COMMA:  gen_expr(n->lhs); gen_expr(n->rhs); return;   /* evaluate left (discard), then right */
	case ND_STMTEXPR:                                            /* ({...}): run the block; the last expr leaves its value in r0(:r1) */
		for (Node *s = n->body; s; s = s->next) gen_stmt(s);
		return;
	case ND_CPAIR: {   /* a complex from its parts, built in its frame temp: the value is the temp's address */
		Type *e = n->type->base;
		for (int k = 0; k < 2; k++) {
			gen_as(k ? n->rhs : n->lhs, e);
			emit_addimm(is64(e) ? "r2" : "r1", "r11", n->offset + k * e->size); store(e);
		}
		emit_addimm("r0", "r11", n->offset);
		return;
	}
	case ND_CUR: {                                               /* the ND_RMW's saved old value (val 0) / operand (val 1) */
		int at = n->target->offset + (n->val ? 12 : 4);
		fp_mem("ldr", "r0", at); if (is64(n->type)) fp_mem("ldr", "r1", at + 4);
		return;
	}
	case ND_RMW: {   /* slot [off] = &lvalue, [off+4..] = old value, [off+12..] = operand; rhs combines them */
		Node *lv = n->lhs; int off = n->offset, bf = lv->kind == ND_MEMBER && lv->bit_width, w = is64(lv->type);
		gen_addr(lv); fp_mem("str", "r0", off);
		WITH_TEMPS(gen_expr(n->init)); fp_mem("str", "r0", off + 12); if (is64(n->init->type)) fp_mem("str", "r1", off + 16);
		fp_mem("ldr", "r0", off);
		if (bf) gen_bitfield_load(lv); else load_lv(lv, lv->type);
		fp_mem("str", "r0", off + 4); if (w) fp_mem("str", "r1", off + 8);
		WITH_TEMPS(gen_as(n->rhs, bf ? lv->bf_type : lv->type));
		if (bf) { fp_mem("ldr", "r2", off); bitfield_put(lv); }
		else if (w) { fp_mem("ldr", "r2", off); store_lv(lv, lv->type); }
		else { fp_mem("ldr", "r1", off); store_lv(lv, lv->type); if (lv->type->size < 4 && !n->is_post) { fprintf(o, "\tmov r0, r1\n"); load_lv(lv, lv->type); } }   /* value = as stored (narrowed) */
		if (n->is_post) { fp_mem("ldr", "r0", off + 4); if (w) fp_mem("ldr", "r1", off + 8); }
		return;
	}
	case ND_VA_START:                                            /* ap = &(first variadic arg) */
		gen_addr(n->lhs); emit_addimm("r1", "r11", cur_core_home + 4 * cur_nfixed); fprintf(o, "\tstr r1, [r0]\n");
		return;
	case ND_VA_ARG:                                              /* fetch *ap, advance ap by the arg width */
		gen_addr(n->lhs);
		if (is_aggr(n->type)) {   /* by-value composite: its words ARE at ap; the result is that address */
			fprintf(o, "\tldr r1, [r0]\n");
			if (arg_align8(n->type)) fprintf(o, "\tadd r1, r1, #7\n\tbic r1, r1, #7\n");
			emit_addimm("r2", "r1", 4 * arg_words(n->type)); fprintf(o, "\tstr r2, [r0]\n\tmov r0, r1\n");
		}
		else if (is64(n->type))   /* 64-bit: 8-align ap (AAPCS even-word), then r0=low@[ap], r1=high@[ap+4]; ap += 8 */
			fprintf(o, "\tldr r2, [r0]\n\tadd r2, r2, #7\n\tbic r2, r2, #7\n\tldr r1, [r2, #4]\n\tadd r3, r2, #8\n\tstr r3, [r0]\n\tldr r0, [r2]\n");
		else
			fprintf(o, "\tldr r1, [r0]\n\tadd r2, r1, #4\n\tstr r2, [r0]\n\tldr r0, [r1]\n");
		return;
	case ND_NEG:
		if (is_aggr(n->lhs->type)) die("cc: '-' on a struct or vector operand in %s", cur_gen_func);
		if (is_fp(n->type)) { need_fp(); gen_as(n->lhs, n->type); fprintf(o, n->type->kind == TY_DOUBLE ? "\teor r1, r1, #0x80000000\n" : "\teor r0, r0, #0x80000000\n"); return; }   /* flip the sign bit */
		gen_expr_w(n->lhs, is64(n->type));
		if (is64(n->type)) fprintf(o, "\trsbs r0, r0, #0\n\trsc r1, r1, #0\n");   /* 0 - value (64-bit) */
		else               fprintf(o, "\trsb r0, r0, #0\n");
		return;
	case ND_BITNOT: if (is_aggr(n->lhs->type)) die("cc: '~' on a struct or vector operand in %s", cur_gen_func);
		gen_expr_w(n->lhs, is64(n->type));
		if (is64(n->type)) fprintf(o, "\tmvn r0, r0\n\tmvn r1, r1\n");
		else               fprintf(o, "\tmvn r0, r0\n");
		return;
	case ND_NOT:    gen_test(n->lhs); fprintf(o, "\tmov r0, #0\n\tmoveq r0, #1\n"); return;
	case ND_AND: {                                          /* a && b — short-circuit */
		int f = uniq(), e = uniq();
		gen_test(n->lhs); fprintf(o, "\tbeq .L%d\n", f);
		gen_test(n->rhs); fprintf(o, "\tbeq .L%d\n", f);
		fprintf(o, "\tmov r0, #1\n\tb .L%d\n.L%d:\n\tmov r0, #0\n.L%d:\n", e, f, e);
		return;
	}
	case ND_OR: {                                           /* a || b — short-circuit */
		int t = uniq(), e = uniq();
		gen_test(n->lhs); fprintf(o, "\tbne .L%d\n", t);
		gen_test(n->rhs); fprintf(o, "\tbne .L%d\n", t);
		fprintf(o, "\tmov r0, #0\n\tb .L%d\n.L%d:\n\tmov r0, #1\n.L%d:\n", e, t, e);
		return;
	}
	case ND_COND: {                                         /* cond ? then : els — both arms widened to the result width */
		int els = uniq(), end = uniq();
		gen_test(n->cond); fprintf(o, "\tbeq .L%d\n", els);
		gen_as(n->then, n->type); fprintf(o, "\tb .L%d\n.L%d:\n", end, els);
		gen_as(n->els, n->type);  fprintf(o, ".L%d:\n", end);
		return;
	}
	case ND_CALL: {
		if (n->name[0] && !strncmp(n->name, "__builtin_", 10)) { gen_builtin(n); return; }   /* never a real call */
		int nargs = 0; for (Node *a = n->args; a; a = a->next) nargs++;
		Node **av = malloc((nargs + 1) * sizeof *av); Type **at = malloc((nargs + 1) * sizeof *at);
		int *pos = malloc((nargs + 1) * sizeof *pos), *vreg = malloc((nargs + 1) * sizeof *vreg);
		nargs = 0; for (Node *a = n->args; a; a = a->next) av[nargs++] = a;
		const char *callee = n->lhs ? 0 : n->name;                 /* a direct call's signature, or the pointed-to function type's */
		Type *ft = n->lhs ? n->lhs->type : NULL; if (ft && is_ptr(ft)) ft = ft->base; if (ft && !ft->fn_ret) ft = NULL;
		for (int i = 0; i < nargs; i++) {                          /* a declared param type decides the slot (a 64-bit param takes a narrower arg widened) */
			Type *pt = callee ? (n->unproto ? NULL : func_param_type(callee, i)) : ft && i < ft->nparams ? ft->params[i] : 0;
			if (pt && pt->transparent && !is_aggr(av[i]->type)) pt = pt->members->type;   /* a member's value: passed as the first member */
			at[i] = pt ? pt : av[i]->type->kind == TY_FLOAT ? ty_double : av[i]->type;   /* no prototype / a vararg: float promotes to double */
		}
		int vfp = !soft_float && !(callee ? func_base_pcs(callee) : ft && ft->variadic == 1);   /* AAPCS-VFP unless the callee is variadic / pcs("aapcs") */
		int sret = is_sret(n->type, vfp), total = aapcs_layout(at, nargs, sret, vfp, pos, vreg);
		int nstk = total > 4 ? total - 4 : 0, cw = n->lhs || n->chain ? 1 : 0, regw = total ? 4 : 0, vs = 0;   /* vs: s-registers used;
		                                                                                                            * cw: a slot for the callee or the static chain */
		for (int i = 0; i < nargs; i++) if (vreg[i] >= 0) { int nel, k = vfp_class(at[i], &nel), e = vreg[i] + nel * vfp_regs(k); if (e > vs) vs = e; }
		vs = (vs + 1) & ~1;   /* staged and loaded as d registers */
		/* One area from sp, laid out in argument-word order: [0,16) = r0..r3 staging, then the outgoing stack
		 * words, then the callee-pointer slot, then the caller's sp. Addressed off sp, so nested calls (which
		 * move sp and restore it) never disturb placed args. The stack machine's one-word pushes leave sp only
		 * 4-aligned, so the area is placed 8-ALIGNED (AAPCS: sp % 8 == 0 at a call; doubleword stack args and
		 * va_arg rely on it) and the saved DISTANCE to the caller's sp restores it after (relative, so an
		 * alloca that slides the live temporaries down stays correct). No value is live in r0-r3 here. */
		int area = regw + nstk + cw + vs + 1; area += area & 1;   /* ... + VFP staging (s0..s(vs-1)) */
		int cslot = 4 * (regw + nstk), vslot = 4 * (regw + nstk + cw), spslot = 4 * (area - 1);
		/* A returns-twice call keeps the caller's sp in its own frame slot instead (the area is gone, maybe reused, by
		 * the time it returns again), and must have nothing else on the stack (see live_temps). */
		int twice = is_twice_call(n);
		if (twice) twice_check(n);
		/* An argument making a returns-twice call — baz(setjmp(b), a) — is evaluated FIRST (the order is unspecified)
		 * and pushed, before the area exists: its second return must not find staged arguments it would have to trust. */
		int pre = -1;
		for (int i = 0; i < nargs; i++) if (av[i]->twice) {
			if (pre >= 0) die("cc: two arguments of a call in %s make returns-twice calls: unsupported", cur_gen_func);
			if (is_aggr(at[i])) die("cc: a struct argument making a returns-twice call in %s: unsupported", cur_gen_func);
			pre = i;
		}
		if (n->lhs && n->lhs->twice) die("cc: a callee expression making a returns-twice call in %s: unsupported", cur_gen_func);
		if (pre >= 0) { gen_as(av[pre], at[pre]); fprintf(o, is64(at[pre]) ? "\tpush {r0, r1}\n" : "\tpush {r0}\n"); }
		if (twice && !n->offset) die("cc: internal: %s() in %s has no frame slot for its sp", n->name, cur_gen_func);
		if (twice) { fprintf(o, "\tmov r3, sp\n"); fp_mem("str", "r3", n->offset); }
		fprintf(o, "\tmov r3, sp\n"); emit_addimm("sp", "sp", -4 * area); fprintf(o, "\tbic sp, sp, #7\n\tsub r3, r3, sp\n"); mem_op("str", "r3", "sp", spslot);
		live_temps++;   /* the staged arguments */
		for (int i = 0; i < nargs; i++) {
			int at_off = vreg[i] >= 0 ? vslot + 4 * vreg[i] : 4 * pos[i];   /* its s-register's staging slot, or its core/stack word */
			if (is_aggr(at[i])) {                                  /* by value: copy its bytes into its words / s-registers */
				vec_check(at[i], av[i]->type, "an argument");
				if (!is_aggr(av[i]->type)) die("cc: a scalar argument for a struct parameter in %s", cur_gen_func);
				gen_expr(av[i]); emit_copy("sp", at_off, "r0", 0, at[i]->size);
				continue;
			}
			if (i == pre) {   /* pushed just above the area: at the caller's sp = the area base + the saved distance */
				mem_op("ldr", "ip", "sp", spslot); fprintf(o, "\tadd ip, sp, ip\n\tldr r0, [ip]\n");
				if (is64(at[i])) fprintf(o, "\tldr r1, [ip, #4]\n");
			}
			else gen_as(av[i], at[i]);
			mem_op("str", "r0", "sp", at_off);
			if (is64(at[i])) mem_op("str", "r1", "sp", at_off + 4);
		}
		if (sret) { emit_addimm("r0", "r11", n->offset); fprintf(o, "\tstr r0, [sp]\n"); }   /* hidden r0 = &result temp */
		if (n->lhs) { gen_expr(n->lhs); mem_op("str", "r0", "sp", cslot); }
		else if (n->chain) { chain_frame(n->chain - 1); mem_op("str", "r0", "sp", cslot); }   /* a nested callee's static chain -> ip */
		if (vs) { emit_addimm("ip", "sp", vslot); fprintf(o, "\tvldm ip, {d0-d%d}\n", vs / 2 - 1); }   /* after anything that could use VFP */
		if (cw) mem_op("ldr", "r12", "sp", cslot);
		if (regw) fprintf(o, "\tpop {r0, r1, r2, r3}\n");
		live_temps--;
		if (n->lhs) fprintf(o, "\tblx r12\n");
		else        fprintf(o, "\tbl %s\n", asm_name(n->name));   /* result in r0(:r1); an asm label renames it */
		if (twice) { fp_mem("ldr", "ip", n->offset); fprintf(o, "\tmov sp, ip\n"); }
		else {
			/* back to the caller's (possibly 4-aligned) sp in ONE move: the distance was measured from the area base, below the
			 * popped r0-r3, so take those 16 off it first — sp must never pass above a live word, even for an instruction
			 * (an interrupt or a signal pushes its frame below sp: it would overwrite the caller's pushed operands) */
			mem_op("ldr", "ip", "sp", spslot - 4 * regw);
			if (regw) fprintf(o, "\tsub ip, ip, #16\n");
			fprintf(o, "\tadd sp, sp, ip\n");
		}
		if (pre >= 0) fprintf(o, "\tadd sp, sp, #%d\n", is64(at[pre]) ? 8 : 4);   /* the pre-evaluated argument */
		int rnel, rk = vfp ? vfp_class(n->type, &rnel) : 0;   /* a float/double/HFA result comes back in s0../d0.. */
		if (is_aggr(n->type)) {                                    /* a struct/vector call yields its temp's ADDRESS */
			if (rk) { emit_addimm("r0", "r11", n->offset); vfp_block("vstmia", "r0", rk, rnel); }
			else if (!sret && n->type->size > 4) { emit_addimm("ip", "r11", n->offset); fprintf(o, n->type->size == 8 ? "\tstm ip, {r0, r1}\n" : "\tstm ip, {r0, r1, r2, r3}\n"); }   /* a vector: r0-r3 */
			else if (!sret && n->type->size) { emit_addimm("r1", "r11", n->offset); fprintf(o, "\tstr r0, [r1]\n"); }   /* <= 4 bytes came back in r0 */
			emit_addimm("r0", "r11", n->offset);
		}
		else if (rk) from_vfp(rk == 2, 0);
		free(av); free(at); free(pos); free(vreg);
		return;
	}
	default: break;
	}

	/* 64-bit operand(s) -> the register-pair path. A comparison yields a 32-bit bool but reads 64-bit
	 * operands, so decide by the operands; every other op's result width is exactly n->type (add_type set
	 * shifts to promote(lhs), arithmetic to usual_arith), so decide by that. */
	if (is_aggr(n->lhs->type) || (n->rhs && is_aggr(n->rhs->type))) die("cc: operator (node %d) on a struct or vector operand in %s", n->kind, cur_gen_func);
	int is_cmp = n->kind==ND_EQ||n->kind==ND_NE||n->kind==ND_LT||n->kind==ND_LE||n->kind==ND_GT||n->kind==ND_GE;
	if (is_cmp ? (is_fp(n->lhs->type) || is_fp(n->rhs->type)) : is_fp(n->type)) { gen_fp_binary(n); return; }
	if (is_cmp ? (is64(n->lhs->type) || is64(n->rhs->type)) : is64(n->type)) { gen_binary64(n); return; }

	if ((n->kind == ND_DIV || n->kind == ND_MOD) && gen_div32_const(n)) return;
	/* binary operators: left -> r0 (saved), right -> r1, combine into r0 */
	gen_expr(n->lhs); fprintf(o, "\tpush {r0}\n");
	WITH_TEMPS(gen_expr(n->rhs)); fprintf(o, "\tmov r1, r0\n\tpop {r0}\n");
	switch (n->kind) {
	case ND_ADD:    fprintf(o, "\tadd r0, r0, r1\n"); break;
	case ND_SUB:    fprintf(o, "\tsub r0, r0, r1\n"); break;
	case ND_MUL:    fprintf(o, "\tmul r0, r0, r1\n"); break;
	case ND_DIV:
		if (!has_idiv()) { emit_libcall(op_uns(n) ? "__aeabi_uidiv" : "__aeabi_idiv"); break; }
		fprintf(o, op_uns(n) ? "\tudiv r0, r0, r1\n" : "\tsdiv r0, r0, r1\n"); break;
	case ND_MOD:
		if (!has_idiv()) { emit_libcall(op_uns(n) ? "__aeabi_uidivmod" : "__aeabi_idivmod"); fprintf(o, "\tmov r0, r1\n"); break; }   /* (r1: the remainder) */
		fprintf(o, op_uns(n) ? "\tudiv r2, r0, r1\n\tmls r0, r2, r1, r0\n"      /* r0 = r0 - (r0/r1)*r1 */
		                     : "\tsdiv r2, r0, r1\n\tmls r0, r2, r1, r0\n"); break;
	case ND_BITAND: fprintf(o, "\tand r0, r0, r1\n"); break;
	case ND_BITOR:  fprintf(o, "\torr r0, r0, r1\n"); break;
	case ND_BITXOR: fprintf(o, "\teor r0, r0, r1\n"); break;
	case ND_SHL:    fprintf(o, "\tlsl r0, r0, r1\n"); break;
	case ND_SHR:    fprintf(o, uns(n->lhs->type) ? "\tlsr r0, r0, r1\n"                     /* unsigned: logical */
	                                             : "\tasr r0, r0, r1\n"); break;            /* signed: arithmetic */
	case ND_EQ: gen_setcc("eq"); break;
	case ND_NE: gen_setcc("ne"); break;
	case ND_LT: gen_setcc(op_uns(n) ? "lo" : "lt"); break;   /* unsigned lower / signed less-than       */
	case ND_LE: gen_setcc(op_uns(n) ? "ls" : "le"); break;   /* unsigned lower-or-same / signed <=       */
	case ND_GT: gen_setcc(op_uns(n) ? "hi" : "gt"); break;   /* unsigned higher / signed greater-than    */
	case ND_GE: gen_setcc(op_uns(n) ? "hs" : "ge"); break;   /* unsigned higher-or-same / signed >=      */
	default: die("cc: unhandled expr node %d", n->kind);
	}
}

/* ---- extended inline asm --------------------------------------------------------------------------- */
static int asm_regnum(const char *s) {   /* a pinned register var's name ("r7"/"sp"/…) -> number, else -1 */
	if (!s || !s[0]) return -1;
	if (!strcmp(s, "sp")) return 13;
	if (!strcmp(s, "lr")) return 14;
	if (!strcmp(s, "fp")) return 11;
	if (!strcmp(s, "ip")) return 12;
	if (!strcmp(s, "pc")) return 15;
	if (!strcmp(s, "sl")) return 10;
	if (!strcmp(s, "sb")) return 9;
	if (s[0] == 'r' && s[1]) { int n = atoi(s + 1); if (n >= 0 && n <= 15) return n; }
	return -1;
}
int fixed_reg_number(const char *s) { return asm_regnum(s); }   /* (cc.c: -ffixed-REG) */
static const char *asm_regname(int n) {   /* constant strings: safe to use twice in one printf */
	static const char *const nm[16] = { "r0","r1","r2","r3","r4","r5","r6","r7","r8","r9","r10","r11","r12","sp","lr","pc" };
	if (n < 0 || n > 15) die("cc: bad register number %d", n);
	return nm[n];
}
static int asm_is_input(const char *c)  { return !strchr(c, '=') || strchr(c, '+'); }   /* "r"/"+r"/"i" read */
/* Emit the template, substituting %0..%9 with each operand's register (or immediate) and turning \n/\t
 * escapes into real newlines/tabs so our as sees one instruction per line. %% -> %, %= (unique id) dropped,
 * a leading modifier letter (%w0/%c0) is ignored. */
static void emit_asm_template(const char *t, char subst[][SYMEXPR_MAX + 24], const int *isimm, const int *hireg, const int *vfpslot, int nops, int basic, int nlabels, int goto_id) {
	fprintf(o, "\t");
	for (const char *p = t; *p; ) {
		if (*p == '%' && !basic) {
			p++;
			if (*p == '%') { fputc('%', o); p++; }
			else if (*p == '=') { p++; }
			else {
				char mod = 0;
				if (*p && !(*p >= '0' && *p <= '9')) { mod = *p; p++; }   /* modifier letter (%c0/%w0/…) */
				if (*p >= '0' && *p <= '9') { int i = 0; while (*p >= '0' && *p <= '9') i = i * 10 + (*p++ - '0');
					if (mod == 'l') {   /* asm goto: label i - nops, through its stub (outputs stored, registers restored) */
						if (i < nops || i >= nops + nlabels) die("cc: asm %%l%d names no label", i);
						fprintf(o, ".Lag%d_%d", goto_id, i - nops);
					}
					else if (i < nops) {
						if (mod == 'H' || mod == 'R') {   /* 64-bit operand in a register PAIR: %H = second reg, %R = most-significant (LE: same) */
							if (hireg[i] < 0) die("cc: asm %%%c%d on an operand that isn't a 64-bit register pair", mod, i);
							fputs(asm_regname(hireg[i]), o);
						}
						else if (mod == 'P') {   /* %P = a VFP operand as its DOUBLE-precision register (musl's `vabs.f64 %P0, %P1`) */
							if (vfpslot[i] < 0) die("cc: asm %%P%d on an operand that isn't a VFP register", i);
							fprintf(o, "d%d", vfpslot[i]);
						}
						else if (mod == 'Q') fputs(subst[i], o);   /* %Q = least-significant reg of the pair (LE: the first) */
						else if (mod == 'a' && !isimm[i] && subst[i][0] != '[') fprintf(o, "[%s]", subst[i]);   /* %a: operand as an ADDRESS (kernel prefetchw: `pldw %a0` -> `pldw [r0]`) */
						else { if (isimm[i] && mod != 'c') fputc('#', o); fputs(subst[i], o); } } }   /* ARM immediate operand prints '#N' by default; %c strips it */
			}
		} else if (*p == '\\') {
			p++;
			if (*p == 'n') { fprintf(o, "\n\t"); p++; }
			else if (*p == 't') { fputc('\t', o); p++; }
			else if (*p) { fputc(*p, o); p++; }
		} else { fputc(*p, o); p++; }
	}
	fputc('\n', o);
}
/* GCC extended asm: assign each operand a register (a pinned `register` var keeps its pin, "i" is an
 * immediate, the rest are allocated), stage inputs onto the stack then pop them into their registers, emit
 * the substituted template, then read outputs back into their lvalues. Operand regs may be r0..r10/r12 —
 * our functions never preserve r4..r10, so clobbering them (and any listed in the ignored clobber set) is
 * invisible to the rest of our code. Values are staged on the stack so operand regs and the r0..r3 scratch
 * used by gen_expr/gen_addr never collide. */
static void gen_asm_ops(Node *n);
static void gen_asm(Node *n) { WITH_TEMPS(gen_asm_ops(n)); }
static void gen_asm_ops(Node *n) {
	Node *ops[16]; int nops = 0;
	for (Node *a = n->args; a; a = a->next) { if (nops >= 16) die("cc: too many asm operands"); ops[nops++] = a; }
	int nouts = n->val, regof[16], used = n->asm_clobber & ~(1 << 14), isimm[16] = {0}; char subst[16][SYMEXPR_MAX + 24];   /* clobbered regs are off-limits for operands */
	for (int i = 0; i < nops; i++) {                       /* immediates + pinned registers; a matching digit ("0") resolves later */
		if (ops[i]->imm_bad) die("cc: asm operand %d (\"%s\") must be a constant in %s", i, ops[i]->cons, cur_gen_func);
		if (ops[i]->imm_sym || (strpbrk(ops[i]->cons, "insIJKLMN") && !strpbrk(ops[i]->cons, "rlgmQoV0123456789"))) {   /* an immediate */
			regof[i] = -2; isimm[i] = 1;
			if (ops[i]->imm_sym) { if (strlen(ops[i]->imm_sym) >= sizeof subst[i]) die("cc: asm symbolic operand too long"); strcpy(subst[i], ops[i]->imm_sym); }
			else snprintf(subst[i], sizeof subst[i], "%ld", ops[i]->val);
			continue;
		}
		if (ops[i]->cons[0] >= '0' && ops[i]->cons[0] <= '9') { if (ops[i]->cons[0] - '0' >= nouts || i < nouts) die("cc: asm matching constraint \"%s\" must be an input naming an output", ops[i]->cons); regof[i] = -3; continue; }
		int rn = asm_regnum(ops[i]->reg); regof[i] = rn; if (rn >= 0) used |= 1 << rn;
	}
	#define ASM_MEM(i) (strpbrk(ops[i]->cons, "mQoV") != NULL)   /* memory constraints: m/o/V general, Q = single-register address (ARM) */
	/* VFP register operands (ARM's "w" = a VFP register named as a double, "t"/"x" = one named as a single): each
	 * takes its own d register out of the caller-saved d0-d7 — nothing of ours is live there at a statement — and the
	 * value moves between it and the core registers our codegen keeps values in. The CONSTRAINT decides the spelling,
	 * the TYPE decides how much moves: GCC prints "t" as the low s register even for a double (`s14` for d7). */
	int vfp[16]; for (int i = 0; i < nops; i++) vfp[i] = -1;
	{
		int slot = 0;
		for (int i = 0; i < nops; i++) {
			if (regof[i] < -1 || ASM_MEM(i) || !strpbrk(ops[i]->cons, "wtx")) continue;
			if (regof[i] >= 0) die("cc: asm operand %d: a VFP constraint \"%s\" on a register variable", i, ops[i]->cons);
			need_fp();
			if (slot >= 8) die("cc: out of VFP registers for asm operands");
			vfp[i] = slot++; regof[i] = -4;
			snprintf(subst[i], sizeof subst[i], strchr(ops[i]->cons, 'w') ? "d%d" : "s%d", strchr(ops[i]->cons, 'w') ? vfp[i] : 2 * vfp[i]);
		}
	}
	#define ASM_VFP(i) (vfp[i] >= 0)
	int hireg[16]; for (int i = 0; i < nops; i++) hireg[i] = -1;
	unsigned pinned = used; used |= fixed_regs;   /* -ffixed-REG: never handed to an unpinned operand */
	for (int i = 0; i < nops; i++) if (regof[i] >= -1 && !ASM_MEM(i) && !ASM_VFP(i) && is64(ops[i]->type)) {   /* 64-bit value: an EVEN/ODD pair (ldrexd/strexd need it) */
		if (regof[i] >= 0) { hireg[i] = regof[i] + 1; used |= 1 << hireg[i]; continue; }   /* pinned: low reg given, high follows */
		int lo = -1;
		for (int c = 0; c <= 8; c += 2) if (!(used & (3 << c))) { lo = c; break; }   /* r0/r1 .. r8/r9 (r10/r11 has fp) */
		if (lo < 0) die("cc: out of register pairs for 64-bit asm operands");
		regof[i] = lo; hireg[i] = lo + 1; used |= 3 << lo;
	}
	for (int i = 0; i < nops; i++) if (regof[i] == -1) {   /* allocate the unpinned ones (avoid r11/sp/lr/pc) */
		int rn = -1;
		for (int c = 0; c <= 12; c++) { if (c == 11) continue; if (!(used & (1 << c))) { rn = c; break; } }
		if (rn < 0) die("cc: out of registers for asm operands");
		regof[i] = rn; used |= 1 << rn;
	}
	for (int i = 0; i < nops; i++) if (regof[i] == -3) {   /* "N": the SAME register as output N (a 64-bit input tied to a 32-bit output passes its low word) */
		int d = ops[i]->cons[0] - '0';
		if (ASM_VFP(d)) { vfp[i] = vfp[d]; regof[i] = -4; strcpy(subst[i], subst[d]); continue; }
		regof[i] = regof[d]; hireg[i] = is64(ops[i]->type) ? hireg[d] : -1;
		if (regof[i] < 0) die("cc: asm matching constraint names a non-register operand");
	}
	/* AAPCS: r4-r10 are callee-saved. Our stack-machine code never keeps values in them, but GCC-built callers
	 * (the rest of the kernel) do, so any we hand to the asm block must be restored afterwards. */
	used &= ~fixed_regs | pinned;
	/* (a -ffixed register is the program's — U-Boot's r9 gd pointer: what an asm writes there must stay) */
	char csave[48] = ""; for (int r = 4; r <= 10; r++) if (((used | n->asm_clobber) & ~fixed_regs) & (1 << r)) {   /* allocated OR clobbered callee-saved */ if (csave[0]) strcat(csave, ", "); strcat(csave, asm_regname(r)); }
	if (csave[0]) fprintf(o, "\tpush {%s}\n", csave);
	/* an "m" operand references memory: hold its ADDRESS in the reg and substitute "[reg]" */
	for (int i = 0; i < nops; i++) if (regof[i] >= 0) snprintf(subst[i], sizeof subst[i], ASM_MEM(i) ? "[%s]" : "%s", asm_regname(regof[i]));
	/* Each register output's lvalue is evaluated ONCE: its address goes to the asm's frame slot, read back
	 * for a "+r" input value and for the final store (asm("" : "+r"(*f())) calls f once). */
	for (int i = 0; i < nouts; i++) if ((regof[i] >= 0 || ASM_VFP(i)) && !ASM_MEM(i)) { gen_addr(ops[i]); fp_mem("str", "r0", n->offset + 4 * i); }
	for (int i = 0; i < nops; i++) if (ASM_VFP(i) && asm_is_input(ops[i]->cons)) {   /* stage each VFP input's value */
		if (i < nouts) { fp_mem("ldr", "r0", n->offset + 4 * i); load(ops[i]->type); }   /* "+w": the current value */
		else gen_expr(ops[i]);
		fprintf(o, is64(ops[i]->type) ? "\tpush {r0, r1}\n" : "\tpush {r0}\n");
	}
	for (int i = nops - 1; i >= 0; i--) if (ASM_VFP(i) && asm_is_input(ops[i]->cons)) {
		if (is64(ops[i]->type)) fprintf(o, "\tpop {r0, r1}\n\tvmov d%d, r0, r1\n", vfp[i]);
		else fprintf(o, "\tpop {r0}\n\tvmov s%d, r0\n", 2 * vfp[i]);
	}
	for (int i = 0; i < nops; i++) if (regof[i] >= 0 && (ASM_MEM(i) || asm_is_input(ops[i]->cons))) {
		if (ASM_MEM(i)) gen_addr(ops[i]);
		else if (i < nouts) { fp_mem("ldr", "r0", n->offset + 4 * i); load(ops[i]->type); }   /* "+r": the current value */
		else gen_expr(ops[i]);
		fprintf(o, hireg[i] >= 0 ? "\tpush {r0, r1}\n" : "\tpush {r0}\n");   /* a pair's value is r0:r1 */
	}
	for (int i = nops - 1; i >= 0; i--) if (regof[i] >= 0 && (ASM_MEM(i) || asm_is_input(ops[i]->cons))) {
		if (hireg[i] >= 0) fprintf(o, "\tpop {%s, %s}\n", asm_regname(regof[i]), asm_regname(hireg[i]));
		else fprintf(o, "\tpop {%s}\n", asm_regname(regof[i]));
	}
	int nlabels = 0; for (Node *l = n->body; l; l = l->next) nlabels++;
	int goto_id = nlabels ? uniq() : 0;
	emit_asm_template(n->asm_tmpl, subst, isimm, hireg, vfp, nops, n->asm_basic, nlabels, goto_id);
	/* The way out, on the fall-through path and (asm goto) before each label: the outputs stored, r4-r10 restored. */
	Node *l = n->body;
	for (int path = 0; path <= nlabels; path++) {
		if (path) fprintf(o, ".Lag%d_%d:\n", goto_id, path - 1);
		for (int i = 0; i < nops; i++) if (i < nouts && ASM_VFP(i)) {   /* a VFP output's value back to core registers */
			if (is64(ops[i]->type)) fprintf(o, "\tvmov r0, r1, d%d\n", vfp[i]);
			else fprintf(o, "\tvmov r0, s%d\n", 2 * vfp[i]);
			fp_mem("ldr", is64(ops[i]->type) ? "r2" : "r1", n->offset + 4 * i);
			store(ops[i]->type);
		}
		for (int i = 0; i < nops; i++) if (i < nouts && regof[i] >= 0 && !ASM_MEM(i)) {
			if (hireg[i] >= 0) fprintf(o, "\tpush {%s, %s}\n", asm_regname(regof[i]), asm_regname(hireg[i]));
			else fprintf(o, "\tpush {%s}\n", asm_regname(regof[i]));
		}
		for (int i = nops - 1; i >= 0; i--) if (i < nouts && regof[i] >= 0 && !ASM_MEM(i)) {
			if (hireg[i] >= 0) { fprintf(o, "\tpop {r0, r1}\n"); fp_mem("ldr", "r2", n->offset + 4 * i); }   /* 64-bit store: addr r2, value r0:r1 */
			else { fprintf(o, "\tpop {r0}\n"); fp_mem("ldr", "r1", n->offset + 4 * i); }
			store(ops[i]->type);
		}
		if (csave[0]) fprintf(o, "\tpop {%s}\n", csave);
		if (!nlabels) break;
		if (!path) fprintf(o, "\tb .Lag%d_end\n", goto_id);
		else { fprintf(o, "\tb " CLABEL_FMT "\n", cur_gen_func, l->name); l = l->next; }
	}
	if (nlabels) fprintf(o, ".Lag%d_end:\n", goto_id);
	#undef ASM_MEM
}

static void gen_stmt(Node *n) {
	switch (n->kind) {
	case ND_RETURN: {   /* widen to the return type; lhs NULL for `return;` */
		int rnel, rk = cur_vfp ? vfp_class(cur_ret, &rnel) : 0;
		if (n->lhs && is_aggr(cur_ret)) { vec_check(cur_ret, n->lhs->type, "a return"); if (!is_aggr(n->lhs->type)) die("cc: returning a scalar from a struct function %s", cur_gen_func); }
		if (n->lhs && rk && is_aggr(cur_ret)) { gen_expr(n->lhs); vfp_block("vldmia", "r0", rk, rnel); }   /* HFA / vector: s0../d0.. */
		else if (n->lhs && rk) { gen_as(n->lhs, cur_ret); to_vfp(rk == 2, 0); }   /* float/double: s0 / d0 */
		else if (n->lhs && is_aggr(cur_ret)) {   /* gen_expr leaves the struct's address */
			int sz = cur_ret->size; gen_expr(n->lhs);
			if (is_sret(cur_ret, cur_vfp)) { fp_mem("ldr", "r1", cur_core_home); emit_copy("r1", 0, "r0", 0, sz); fp_mem("ldr", "r0", cur_core_home); }   /* into the caller's buffer; r0 = it */
			else if (sz > 4) fprintf(o, sz == 8 ? "\tldm r0, {r0, r1}\n" : "\tldm r0, {r0, r1, r2, r3}\n");   /* a vector in the base PCS: r0-r3 */
			else if (sz == 4) fprintf(o, "\tldr r0, [r0]\n");   /* <= 4 bytes: the bytes themselves in r0 */
			else if (sz == 3) fprintf(o, "\tldrb r1, [r0, #2]\n\tldrh r0, [r0]\n\torr r0, r0, r1, lsl #16\n");
			else if (sz) fprintf(o, sz == 2 ? "\tldrh r0, [r0]\n" : "\tldrb r0, [r0]\n");
		}
		else if (n->lhs) { gen_as(n->lhs, cur_ret); if (cur_ret && !cur_ret->is_bool && !is_fp(cur_ret) && cur_ret->size < 4) gen_cast(cur_ret); }   /* callee narrows (AAPCS) */
		fprintf(o, "\tb .L%d\n", ret_label); return; }
	case ND_EXPRSTMT: gen_expr(n->lhs); return;
	case ND_BLOCK:    for (Node *s = n->body; s; s = s->next) gen_stmt(s); return;
	case ND_IF: {
		int els = uniq(), end = uniq();
		gen_test(n->cond); fprintf(o, "\tbeq .L%d\n", els);
		gen_stmt(n->then); fprintf(o, "\tb .L%d\n.L%d:\n", end, els);
		if (n->els) gen_stmt(n->els);
		fprintf(o, ".L%d:\n", end);
		return;
	}
	case ND_WHILE: {
		int begin = uniq(), end = uniq(), sb = brk_lbl, sc = cont_lbl;
		brk_lbl = end; cont_lbl = begin;                    /* continue -> re-test, break -> exit */
		fprintf(o, ".L%d:\n", begin);
		gen_test(n->cond); fprintf(o, "\tbeq .L%d\n", end);
		gen_stmt(n->body); fprintf(o, "\tb .L%d\n.L%d:\n", begin, end);
		brk_lbl = sb; cont_lbl = sc;
		return;
	}
	case ND_DOWHILE: {
		int begin = uniq(), end = uniq(), cont = uniq(), sb = brk_lbl, sc = cont_lbl;
		brk_lbl = end; cont_lbl = cont;                     /* continue -> re-test at the bottom */
		fprintf(o, ".L%d:\n", begin);
		gen_stmt(n->body);
		fprintf(o, ".L%d:\n", cont);
		gen_test(n->cond); fprintf(o, "\tbne .L%d\n.L%d:\n", begin, end);
		brk_lbl = sb; cont_lbl = sc;
		return;
	}
	case ND_FOR: {
		int begin = uniq(), end = uniq(), cont = uniq(), sb = brk_lbl, sc = cont_lbl;
		brk_lbl = end; cont_lbl = cont;                     /* continue -> the inc step, break -> exit */
		if (n->init) gen_stmt(n->init);
		fprintf(o, ".L%d:\n", begin);
		if (n->cond) { gen_test(n->cond); fprintf(o, "\tbeq .L%d\n", end); }
		gen_stmt(n->body);
		fprintf(o, ".L%d:\n", cont);
		if (n->inc) gen_expr(n->inc);
		fprintf(o, "\tb .L%d\n.L%d:\n", begin, end);
		brk_lbl = sb; cont_lbl = sc;
		return;
	}
	case ND_SWITCH: {                                       /* eval, compare-chain to each case, then body */
		int end = uniq(), sb = brk_lbl; brk_lbl = end;
		gen_expr(n->cond);                                  /* switch value -> r0 (r0:r1 if 64-bit) */
		int def = 0, w = is64(n->cond->type), u = n->cond->type && n->cond->type->is_unsigned;
		for (Node *c = n->case_list; c; c = c->case_next) {
			c->offset = uniq();                             /* the label this case jumps to */
			if (c->is_default) { def = c->offset; continue; }
			if (w) {                                        /* 64-bit: compare the pair (a range via 64-bit subtracts) */
				load_imm("r2", c->val & 0xffffffff); load_imm("r3", (c->val >> 32) & 0xffffffff);
				if (!c->is_range) { fprintf(o, "\tcmp r0, r2\n\tcmpeq r1, r3\n\tbeq .L%d\n", c->offset); continue; }
				int skip = uniq();
				fprintf(o, "\tsubs ip, r0, r2\n\tsbcs ip, r1, r3\n\tb%s .L%d\n", u ? "lo" : "lt", skip);   /* x < lo */
				load_imm("r2", c->val2 & 0xffffffff); load_imm("r3", (c->val2 >> 32) & 0xffffffff);
				fprintf(o, "\tsubs ip, r2, r0\n\tsbcs ip, r3, r1\n\tb%s .L%d\n\tb .L%d\n.L%d:\n", u ? "lo" : "lt", skip, c->offset, skip);   /* hi < x */
			} else if (c->is_range) {                           /* case lo ... hi: match the inclusive range */
				int skip = uniq();
				load_imm("r1", c->val);  fprintf(o, "\tcmp r0, r1\n\tb%s .L%d\n", u ? "lo" : "lt", skip);
				load_imm("r1", c->val2); fprintf(o, "\tcmp r0, r1\n\tb%s .L%d\n", u ? "hi" : "gt", skip);
				fprintf(o, "\tb .L%d\n.L%d:\n", c->offset, skip);
			} else {
				load_imm("r1", c->val); fprintf(o, "\tcmp r0, r1\n\tbeq .L%d\n", c->offset);
			}
		}
		fprintf(o, "\tb .L%d\n", def ? def : end);          /* no match -> default, else past the switch */
		gen_stmt(n->then);                                  /* body; ND_CASE nodes drop their labels inline */
		fprintf(o, ".L%d:\n", end);
		brk_lbl = sb;
		return;
	}
	case ND_CASE: fprintf(o, ".L%d:\n", n->offset); return;  /* label placed inline in the switch body */
	case ND_ASM: gen_asm(n); return;
	case ND_VLAMARK: {   /* first run: remember the floor; re-run (a loop): rewind sp/floor to it, freeing the last instance
	                      * and everything allocated after it — so later declarations' marks are reset too */
		int k = 0; while (k < nvla_marks && vla_marks[k] != n) k++;
		int set = uniq(), done = uniq();
		fp_mem("ldr", "r0", n->offset); fprintf(o, "\tcmp r0, #0\n\tbeq .L%d\n\tmov sp, r0\n", set); fp_mem("str", "r0", cur_alloca_slot);
		fprintf(o, "\tb .L%d\n.L%d:\n", done, set); fp_mem("ldr", "r0", cur_alloca_slot); fp_mem("str", "r0", n->offset);
		fprintf(o, ".L%d:\n", done);
		if (k + 1 < nvla_marks) { fprintf(o, "\tmov r0, #0\n"); for (int j = k + 1; j < nvla_marks; j++) fp_mem("str", "r0", vla_marks[j]->offset); }
		return;
	}   /* %N-substituted template + constraint-driven operand load/store */
	case ND_GOTO:
		if (n->lhs) { gen_expr(n->lhs); fprintf(o, "\tbx r0\n"); return; }   /* computed goto */
		if (n->owner) {   /* non-local (a nested function's, to an enclosing one's __label__): that function's frame + sp, then jump */
			Func *of = find_func(gen_prog, n->owner);
			if (!of || of->final_frame < 0) die("cc: internal: goto %s: its function %s is not generated yet", n->name, n->owner);
			chain_frame(n->chain); fprintf(o, "\tmov r11, r0\n");
			if (of->alloca_slot) { fp_mem("ldr", "ip", of->alloca_slot); fprintf(o, "\tmov sp, ip\n"); }   /* its dynamic stack floor */
			else emit_addimm("sp", "r11", -of->final_frame);
			fprintf(o, "\tb " CLABEL_FMT "\n", n->owner, n->name); return;
		}
		fprintf(o, "\tb " CLABEL_FMT "\n", cur_gen_func, n->name); return;
	case ND_LABEL: fprintf(o, CLABEL_FMT ":\n", cur_gen_func, n->name); return;
	case ND_BREAK:    if (!brk_lbl)  die("cc: break outside a loop");    fprintf(o, "\tb .L%d\n", brk_lbl);  return;
	case ND_CONTINUE: if (!cont_lbl) die("cc: continue outside a loop"); fprintf(o, "\tb .L%d\n", cont_lbl); return;
	default: gen_expr(n); return;   /* a bare declaration compiles to an empty ND_BLOCK; other exprs run */
	}
}

/* Frame temps below the locals (frame grows): a result slot per struct-returning call, an address+old-value slot per ND_RMW. */
/* Codegen's own frame slots, below the locals. Function-wide ones (alloca floor, setjmp save, a VLA's mark — read again
 * when its declaration re-runs, __builtin_apply's) are allocated once; an expression's temporaries (an op='s lvalue/old
 * value/operand, an aggregate call's result, a complex pair, asm output addresses) live only while their statement
 * runs, so each statement reuses the same area — within one statement they stack up (a struct result is consumed by
 * the expression around it). Stack slots per temporary would overrun the kernel's 8KB stacks. */
static int alloc_fixed(Node *n, int *frame) {   /* returns: n's subtree makes a returns-twice call (-> n->twice) */
	if (!n) return 0;
	if (n->kind == ND_CALL && (!strcmp(n->name, "__builtin_alloca") || !strcmp(n->name, "__builtin_stack_save") || !strcmp(n->name, "__builtin_stack_restore"))
	    && !cur_alloca_slot) { *frame += 4; cur_alloca_slot = -*frame; }
	if (n->kind == ND_CALL && !strcmp(n->name, "__builtin_setjmp") && !cur_setjmp_save) { *frame += 28; cur_setjmp_save = -*frame; }
	if (is_twice_call(n) && strncmp(n->name, "__builtin_", 10) && is_aggr(n->type)) die("cc: %s() returns a struct and returns twice: unsupported", n->name);
	if (is_twice_call(n) && strncmp(n->name, "__builtin_", 10)) { *frame += 4; n->offset = -*frame; }   /* the caller's sp, for each return (gen ND_CALL) */
	if (n->kind == ND_CALL && !strcmp(n->name, "__builtin_apply_args")) { *frame += 8; n->offset = -*frame; }
	if (n->kind == ND_CALL && !strcmp(n->name, "__builtin_apply")) { *frame += 56; n->offset = -*frame; }
	if (n->kind == ND_VLAMARK) { if (nvla_marks >= 256) die("cc: too many VLA declarations in one function"); *frame += 4; n->offset = -*frame; vla_marks[nvla_marks++] = n; }
	int t = is_twice_call(n);
	t |= alloc_fixed(n->lhs, frame); t |= alloc_fixed(n->rhs, frame); t |= alloc_fixed(n->cond, frame);
	t |= alloc_fixed(n->then, frame); t |= alloc_fixed(n->els, frame); t |= alloc_fixed(n->init, frame); t |= alloc_fixed(n->inc, frame);
	for (Node *c = n->body; c; c = c->next) t |= alloc_fixed(c, frame);
	for (Node *a = n->args; a; a = a->next) t |= alloc_fixed(a, frame);
	return n->twice = t;
}
static int is_stmt_kind(NodeKind k) {   /* its parts run one after another, none's temporaries needed by the next */
	return k == ND_BLOCK || k == ND_IF || k == ND_WHILE || k == ND_DOWHILE || k == ND_FOR || k == ND_SWITCH || k == ND_EXPRSTMT ||
	       k == ND_RETURN || k == ND_CASE || k == ND_LABEL || k == ND_GOTO;
}
static int alloc_temps(Node *n, int base) {   /* n's temporaries from `base` down; returns the depth reached */
	if (!n) return base;
	int b = base;
	if (n->kind == ND_RMW) { b += 24; n->offset = -b; }   /* &lvalue + old value + operand */
	if (n->kind == ND_CPAIR) { b += (n->type->size + 3) & ~3; n->offset = -b; }
	if (n->kind == ND_ASM && n->val) { b += 4 * n->val; n->offset = -b; }   /* one address per output */
	if (n->kind == ND_CALL && is_aggr(n->type) && strncmp(n->name, "__builtin_", 10)) {
		b += n->type->size < 4 ? 4 : (n->type->size + 3) & ~3; n->offset = -b;   /* >= 4: an r0 return is stored as a word */
	}
	int hw = b;
	#define PART(x) do { int h_ = alloc_temps((x), b); if (h_ > hw) hw = h_; } while (0)
	if (is_stmt_kind(n->kind) || n->kind == ND_STMTEXPR) {   /* statements: each part from the same base */
		PART(n->lhs); PART(n->rhs); PART(n->cond); PART(n->then); PART(n->els); PART(n->init); PART(n->inc);
		for (Node *c = n->body; c; c = c->next) PART(c);
		for (Node *a = n->args; a; a = a->next) PART(a);
		return hw;
	}
	#undef PART
	b = alloc_temps(n->lhs, b); b = alloc_temps(n->rhs, b); b = alloc_temps(n->cond, b);   /* an expression: they stack up */
	b = alloc_temps(n->then, b); b = alloc_temps(n->els, b); b = alloc_temps(n->init, b); b = alloc_temps(n->inc, b);
	for (Node *c = n->body; c; c = c->next) b = alloc_temps(c, b);
	for (Node *a = n->args; a; a = a->next) b = alloc_temps(a, b);
	return b;
}
/* Frame: [r11+8..] homed r0..r3 (when the function takes any argument words) contiguous with the caller's
 * stack args, so every param is at [r11, #8 + 4*word]; [r11] saved r11/lr; below r11 the locals + call temps. */
/* A naked function's statement: asm whose operands are all immediates (GCC: nothing may need a register or the
 * frame — there is none), possibly in blocks; anything else has no code without a prologue. */
static void naked_stmt(Node *s) {
	if (s->kind == ND_BLOCK) { for (Node *c = s->body; c; c = c->next) naked_stmt(c); return; }
	if (s->kind == ND_EXPRSTMT && !s->lhs) return;
	if (s->kind != ND_ASM) die("cc: naked function %s: only asm statements are allowed in its body", cur_gen_func);
	for (Node *a = s->args; a; a = a->next)
		if (!a->imm_sym && !(strpbrk(a->cons, "insIJKLMN") && !strpbrk(a->cons, "rlgmQoV0123456789")))
			die("cc: naked function %s: asm operand \"%s\" needs a register or memory (only immediates are allowed)", cur_gen_func, a->cons);
	gen_asm(s);
}
static void gen_func(Func *f) {
	ret_label = uniq(); cur_func_id = func_seq++; ngot = 0; cur_gen_func = f->name;
	cur_nfixed = f->nfixed_words; cur_ret = f->ret_type;
	cur_vfp = f->vfp; cur_core_home = f->vfp_save ? 72 : 8;
	int frame = f->frame, homed = f->nfixed_words || f->variadic || f->vfp_save;
	cur_alloca_slot = 0; nvla_marks = 0; cur_setjmp_save = 0;
	for (Node *s = f->body; s; s = s->next) alloc_fixed(s, &frame);
	int base = frame; for (Node *s = f->body; s; s = s->next) { int h = alloc_temps(s, base); if (h > frame) frame = h; }
	frame = (frame + 7) & ~7;
	if (f->attr.section[0]) fprintf(o, "\t.section %s,\"ax\",%%progbits\n", f->attr.section);   /* __attribute__((section)) (__init ...) */
	else if (function_sections) fprintf(o, "\t.section .text.%s,\"ax\",%%progbits\n", asm_name(f->name));   /* its own: .text.f */
	else fprintf(o, "\t.text\n");
	fprintf(o, "\t.p2align %d\n", log2i(f->attr.align > 4 ? f->attr.align : 4));
	const char *sym = asm_name(f->name);   /* its symbol: an asm label renames it */
	if (!f->is_static && vis_directive(f->attr.vis ? f->attr.vis : default_vis))   /* explicit, else -fvisibility= */
		fprintf(o, "\t%s %s\n", vis_directive(f->attr.vis ? f->attr.vis : default_vis), sym);
	if (f->attr.weak || name_is_weak(f->name)) fprintf(o, "\t.weak %s\n", sym);
	else if (!f->is_static) fprintf(o, "\t.global %s\n", sym);   /* `static` -> file-local symbol */
	fprintf(o, "\t.type %s, %%function\n%s:\n", sym, sym);
	/* -funwind-tables: the EHABI entry describes the prologue as GCC's does — the pushes (.save/.vsave), the frame
	 * pointer (.setfp: from then on the unwinder recovers sp from r11), the frame (.pad) */
	if (unwind_tables) fprintf(o, "\t.fnstart\n");
	if (f->attr.naked) {   /* no prologue, no epilogue, no return: the asm is the function */
		for (Node *s = f->body; s; s = s->next) naked_stmt(s);
		if (unwind_tables) fprintf(o, "\t.fnend\n");
		fprintf(o, "\t.size %s, .-%s\n", sym, sym); return;
	}
	if (homed) fprintf(o, "\tpush {r0, r1, r2, r3}\n%s", unwind_tables ? "\t.save {r0, r1, r2, r3}\n" : "");
	if (f->vfp_save) fprintf(o, "\tvpush {d0-d7}\n%s", unwind_tables ? "\t.vsave {d0-d7}\n" : "");   /* VFP argument registers, below the homed r0-r3 */
	fprintf(o, "\tpush {r11, lr}\n%s\tmov r11, sp\n%s", unwind_tables ? "\t.save {r11, lr}\n" : "", unwind_tables ? "\t.setfp r11, sp\n" : "");
	if (f->nested) fprintf(o, "\tstr ip, [sp, #-4]!\n");   /* the static chain: first slot, [r11, #-4] (before anything uses ip) */
	if (frame - 4 * f->nested) emit_addimm("sp", "sp", -(frame - 4 * f->nested));   /* ip is free here; frame may exceed the imm range */
	if (unwind_tables && frame) fprintf(o, "\t.pad #%d\n", frame);
	f->final_frame = frame; f->alloca_slot = cur_alloca_slot;   /* for a non-local goto into this function */
	if (cur_alloca_slot) { fprintf(o, "\tmov ip, sp\n"); fp_mem("str", "ip", cur_alloca_slot); }   /* alloca floor = the frame's bottom */
	if (nvla_marks) { fprintf(o, "\tmov ip, #0\n"); for (int i = 0; i < nvla_marks; i++) fp_mem("str", "ip", vla_marks[i]->offset); }   /* no VLA allocated yet */
	/* A longjmp may arrive from code that used r4-r10: the caller still expects them preserved (AAPCS). */
	if (cur_setjmp_save) for (int r = 4; r <= 10; r++) { char rn[4]; snprintf(rn, sizeof rn, "r%d", r); fp_mem("str", rn, cur_setjmp_save + 4 * (r - 4)); }
	for (Node *s = f->body; s; s = s->next) gen_stmt(s);
	fprintf(o, ".L%d:\n", ret_label);
	if (cur_setjmp_save) for (int r = 4; r <= 10; r++) { char rn[4]; snprintf(rn, sizeof rn, "r%d", r); fp_mem("ldr", rn, cur_setjmp_save + 4 * (r - 4)); }
	fprintf(o, "\tmov sp, r11\n\tpop {r11, lr}\n");            /* epilogue */
	if (f->vfp_save) fprintf(o, "\tadd sp, sp, #64\n");      /* discard the saved d0-d7 */
	if (homed) fprintf(o, "\tadd sp, sp, #16\n");            /* and the homed r0..r3 */
	fprintf(o, "\tbx lr\n");
	if (unwind_tables) fprintf(o, "\t.fnend\n");
	fprintf(o, "\t.size %s, .-%s\n", sym, sym);    /* the symbol's size (debuggers, backtraces, a provider's exports) */
	/* __attribute__((constructor/destructor)): a word pointing at it in .init_array / .fini_array, which the
	 * linker script collects between __init_array_start/_end for the C runtime to walk. A priority gets its own
	 * .init_array.NNNNN, which the script sorts by name. */
	for (int pass = 0; pass < 2; pass++) {
		int prio = pass ? f->attr.dtor : f->attr.ctor;
		if (!prio) continue;
		const char *arr = pass ? ".fini_array" : ".init_array";
		if (prio < 0) fprintf(o, "\t.section %s,\"aw\"\n", arr);
		else fprintf(o, "\t.section %s.%05d,\"aw\"\n", arr, prio);
		fprintf(o, "\t.align 2\n\t.word %s\n", sym);
	}
}

/* An initialized object's section, as GCC chooses it: a const (not volatile) object is read-only — .rodata, or
 * under -fPIC .data.rel.ro when its initializer holds addresses (the loader fixes those up; then the linker can
 * write-protect it); anything else .data. */
static const char *data_section(Gvar *g) {
	if (g->is_tls) return ".section .tdata,\"awT\",%progbits";   /* a thread-local object's initial image */
	int q = 0;   /* an array's qualifiers are its elements' (C11 6.7.3p9): `const T` with T an array typedef is read-only too */
	for (Type *t = g->type; ; t = t->base) { q |= t->quals; if (t->kind != TY_ARRAY) break; }
	if (!((q & 1) || g->const_obj) || (q & 2)) return ".data";   /* (a struct's const: on the object) */
	if (pic) for (Init *it = g->init; it; it = it->next) if (it->kind == INIT_SYM) return ".section .data.rel.ro,\"aw\",%progbits";
	return ".section .rodata";
}
/* Section, binding and alignment of one file-scope object: `deflt` is .data/.bss; an attribute section wins
 * (`.bss*` names are NOBITS). The alignment is the type's, raised by aligned(N). */
/* -fdata-sections: the object in its own section — the default one's name + "." + its symbol (.data.x, .bss.x,
 * .rodata.x, .tdata.x, .tbss.x, .data.rel.ro.x: GCC's), with that section's flags and type. */
static void own_section(Gvar *g, const char *deflt) {
	static const struct { const char *deflt, *name, *rest; } map[] = {
		{ ".data", ".data", "\"aw\"" }, { ".bss", ".bss", "\"aw\",%nobits" }, { ".section .rodata", ".rodata", "\"a\"" },
		{ ".section .tdata,\"awT\",%progbits", ".tdata", "\"awT\",%progbits" }, { ".section .tbss,\"awT\",%nobits", ".tbss", "\"awT\",%nobits" },
		{ ".section .data.rel.ro,\"aw\",%progbits", ".data.rel.ro", "\"aw\"" } };
	for (unsigned i = 0; i < sizeof map / sizeof *map; i++)
		if (!strcmp(deflt, map[i].deflt)) { fprintf(o, "\t.section %s.%s,%s\n", map[i].name, asm_name(g->name), map[i].rest); return; }
	die("cc: internal: no -fdata-sections name for section '%s'", deflt);
}
static void gvar_head(Gvar *g, const char *deflt) {
	if (g->attr.section[0]) {   /* GCC's directive: a BSS-named section %nobits, TLS "awT"; plain data no type at all — the assembler
		                         * types it by name (.note* NOTE, .init_array INIT_ARRAY) — read-only ("a") where it'd be .rodata */
		const char *nm = g->attr.section;
		if (g->is_tls) fprintf(o, "\t.section %s,\"awT\",%s\n", nm, strncmp(nm, ".tbss", 5) ? "%progbits" : "%nobits");
		else if (!strncmp(nm, ".bss", 4) || !strncmp(nm, ".sbss", 5)) fprintf(o, "\t.section %s,\"aw\",%%nobits\n", nm);
		else fprintf(o, "\t.section %s,\"%s\"\n", nm, !strcmp(deflt, ".section .rodata") ? "a" : "aw");
	}
	else if (data_sections) own_section(g, deflt);
	else fprintf(o, "\t%s\n", deflt);
	if (g->attr.weak || name_is_weak(g->name)) fprintf(o, "\t.weak %s\n", asm_name(g->name));
	else if (!g->is_static) fprintf(o, "\t.global %s\n", asm_name(g->name));
	fprintf(o, "\t.type %s, %%object\n", asm_name(g->name));      /* STT_OBJECT + a real .size -> exported size (copy relocs) */
	int al = align_of(g->type); if (g->attr.align > al) al = g->attr.align;
	if (al > 1) fprintf(o, "\t.p2align %d\n", log2i(al));
}
/* Emit the file-scope objects: string literals in .rodata, initialized globals in .data, zero-init in .bss;
 * an `extern` decl defines nothing — it's a reference the linker resolves against the real definition. */
static StrMap dce_funcs, dce_refs;   /* (the dead-code pass's, below: what's defined, what's used) */
static void gen_data(void) {
	for (Gvar *g = globals; g; g = g->next) if (g->is_topasm) fprintf(o, "%s\n", g->str);   /* file-scope asm: .weak/.set etc, verbatim */
	for (ProtoVis *p = proto_vis; p; p = p->next) {   /* a prototype's visibility, once the symbol is used (not defined here: its definition says it) */
		Func *f = strmap_get(&dce_funcs, p->name);
		if (f && !f->no_emit) continue;
		if (strmap_get(&dce_refs, p->name) && vis_directive(p->vis)) fprintf(o, "\t%s %s\n", vis_directive(p->vis), asm_name(p->name));
	}
	for (Gvar *g = globals; g; g = g->next) {            /* object visibility: its own, else -fvisibility= for a definition */
		if (g->is_str || g->is_topasm || g->is_static) continue;
		if (g->is_extern && !g->reachable) continue;   /* (GCC: a declaration's visibility only once it's used — `.hidden x`
		                                                 * alone would put an undefined x in the symbol table) */
		const char *v = vis_directive(g->attr.vis ? g->attr.vis : g->is_extern ? 0 : default_vis);
		if (v) fprintf(o, "\t%s %s\n", v, asm_name(g->name));
	}
	for (Gvar *g = globals; g; g = g->next) if (g->is_str && g->reachable) {
		if (!g->wide) {   /* its bytes, 64 to a line (an assembler line stays short), escaped: any byte value round-trips */
			fprintf(o, "\t.section .rodata\n%s:\n", g->name);
			for (int i = 0; i < g->slen; ) {
				fputs("\t.ascii \"", o);
				for (int k = 0; k < 64 && i < g->slen; k++, i++) {
					unsigned char c = (unsigned char)g->str[i];
					if (c == '"' || c == '\\') fprintf(o, "\\%c", c); else if (c >= 32 && c < 127) fputc(c, o); else fprintf(o, "\\%03o", c);
				}
				fputs("\"\n", o);
			}
			fputs("\t.byte 0\n", o);
			continue;
		}
		fprintf(o, "\t.section .rodata\n\t.p2align %d\n%s:\n", g->wide == 4 ? 2 : 1, g->name);   /* wide: one word/halfword per character */
		for (int i = 0; i <= g->slen; i++) fprintf(o, g->wide == 4 ? "\t.word %u\n" : "\t.hword %u\n", i < g->slen ? (g->wide == 4 ? g->wstr[i] : g->wstr[i] & 0xffff) : 0);
	}
	for (Gvar *g = globals; g; g = g->next) if (!g->is_str && g->init && g->reachable) {
		gvar_head(g, data_section(g));
		fprintf(o, "%s:\n", asm_name(g->name));
		for (Init *it = g->init; it; it = it->next) {
			if (it->kind == INIT_CONST) {
				if (it->size == 8) fprintf(o, "\t.word %ld\n\t.word %ld\n", it->val & 0xffffffff, (it->val >> 32) & 0xffffffff);   /* low, high */
				else fprintf(o, it->size == 1 ? "\t.byte %ld\n" : it->size == 2 ? "\t.hword %ld\n" : "\t.word %ld\n", it->val);
			}
			else if (it->kind == INIT_SYM) {   /* a word (a label difference may be a halfword) */
				const char *d = it->size == 2 ? ".hword" : ".word";
				if (it->val) fprintf(o, "\t%s %s+%ld\n", d, asm_name(it->sym), it->val); else fprintf(o, "\t%s %s\n", d, asm_name(it->sym)); }   /* val = byte addend for &sym.member / &sym[i] */
			else fprintf(o, "\t.space %d\n", it->size);
		}
		fprintf(o, "\t.size %s, . - %s\n", asm_name(g->name), asm_name(g->name));
	}
	for (Gvar *g = globals; g; g = g->next) if (!g->is_str && !g->is_topasm && !g->init && !g->is_extern && !g->attr.alias[0] && g->reachable) {
		gvar_head(g, g->is_tls ? ".section .tbss,\"awT\",%nobits" : ".bss");
		fprintf(o, "%s:\n\t.space %d\n", asm_name(g->name), g->type->size);
		fprintf(o, "\t.size %s, . - %s\n", asm_name(g->name), asm_name(g->name));
	}
}

/* Dead-code elimination: a kernel .c pulls in thousands of static inline functions from headers; GCC drops
 * the unused ones. We keep every non-static (exported) function + any function referenced by a global's
 * initializer as roots, then transitively keep whatever they reference (any node->name matching a defined
 * function — direct calls and function designators both surface the name), and drop the rest. */
/* Dead-code elimination, for functions AND objects, as GCC does: a kernel .c pulls in thousands of static inline
 * functions (and header statics, and the static locals of those inline functions) that it never uses; GCC emits none
 * of them. The roots are what the object must export — every non-static definition — plus __used and alias
 * targets; from them, whatever an emitted function's body or an emitted object's initializer names is emitted too, to
 * a fixpoint. (An unused static's initializer can name an undefined function: emitting it would break the link.) */
static StrMap dce_funcs, dce_vars;   /* name -> Func / Gvar (definitions) */
static Func *find_func(Func *prog, const char *name) {
	(void)prog; if (!name || !name[0]) return NULL;
	return strmap_get(&dce_funcs, name);
}
static int dce_changed;
static StrMap dce_refs;   /* every name emitted code or data refers to */
static void dce_name(const char *name) {   /* a reference by name: a function or an object */
	if (!name || !name[0]) return;
	if (!strmap_get(&dce_refs, name)) strmap_put(&dce_refs, strdup(name), (void *)1);
	Func *g = strmap_get(&dce_funcs, name); if (g && !g->reachable && !g->no_emit) { g->reachable = 1; dce_changed = 1; }
	Gvar *v = strmap_get(&dce_vars, name); if (v && !v->reachable) { v->reachable = 1; dce_changed = 1; }
}
static void dce_sym(const char *e) {       /* an initializer's symbol expression: `sym`, `a - b` */
	char w[SYMEXPR_MAX]; size_t k = 0;
	for (const char *p = e; ; p++) {
		if (*p && *p != ' ' && *p != '-' && *p != '+') { if (k < sizeof w - 1) w[k++] = *p; continue; }
		if (k) { w[k] = 0; dce_name(w); k = 0; }
		if (!*p) break;
	}
}
static void dce_mark(Func *prog, Node *n) {   /* everything n's subtree names */
	if (!n) return;
	if (n->kind != ND_VAR && n->kind != ND_LABEL && n->kind != ND_GOTO && n->kind != ND_LABELADDR && n->kind != ND_CASE)
		dce_name(n->name);   /* (a local, a label: not the function or object that happens to share its name) */
	dce_mark(prog, n->lhs); dce_mark(prog, n->rhs); dce_mark(prog, n->cond);
	dce_mark(prog, n->then); dce_mark(prog, n->els); dce_mark(prog, n->init); dce_mark(prog, n->inc);
	for (Node *c = n->body; c; c = c->next) dce_mark(prog, c);
	for (Node *a = n->args; a; a = a->next) dce_mark(prog, a);
}
static void dce(Func *prog) {
	strmap_clear(&dce_funcs); strmap_clear(&dce_vars); strmap_clear(&dce_refs);
	for (Func *f = prog; f; f = f->next) strmap_put(&dce_funcs, f->name, f);
	for (Gvar *g = globals; g; g = g->next) if (!g->is_topasm) strmap_put(&dce_vars, g->name, g);   /* (an extern: to know it's used) */
	for (Func *f = prog; f; f = f->next) f->reachable = f->no_emit || (f->is_static && !f->attr.used && !f->attr.ctor && !f->attr.dtor && !is_alias_target(f->name)) ? 0 : 1;
	for (Gvar *g = globals; g; g = g->next)
		g->reachable = g->is_topasm || (!g->is_extern && !g->is_str && (!g->is_static || g->attr.used || is_alias_target(g->name)));
	for (dce_changed = 1; dce_changed; ) {   /* reachable 1: newly found, 2: its references followed */
		dce_changed = 0;
		for (Func *f = prog; f; f = f->next) if (f->reachable == 1) { f->reachable = 2; dce_changed = 1; for (Node *s = f->body; s; s = s->next) dce_mark(prog, s); }
		for (Gvar *g = globals; g; g = g->next) if (g->reachable == 1) {
			g->reachable = 2; dce_changed = 1;
			if (g->attr.alias[0]) dce_name(g->attr.alias);
			for (Init *it = g->init; it; it = it->next) if (it->kind == INIT_SYM) dce_sym(it->sym);
		}
	}
}
void gen(Func *prog, const char *out) {
	o = fopen(out, "w"); if (!o) die("cc: cannot open %s", out);
	/* The build attributes, exactly as GCC writes them for these options: target, FPU (softvfp: soft float), then
	 * AAPCS-VFP args (hard float), FP denormal/exception/number model, 8-byte alignment needed+preserved, int-sized
	 * enums, optimization goals, unaligned access, wchar_t width. The linker checks some of them between objects. */
	if (opt_march) fprintf(o, "\t.arch %s\n", opt_march);
	else fprintf(o, "\t.cpu cortex-a7\n\t.arch armv7-a\n\t.arch_extension virt\n\t.arch_extension idiv\n\t.arch_extension sec\n\t.arch_extension mp\n");
	fprintf(o, "\t.fpu %s\n", soft_float ? "softvfp" : opt_mfpu);
	if (!soft_float) fprintf(o, "\t.eabi_attribute 28, 1\n");
	if (!unsafe_math) fprintf(o, "\t.eabi_attribute 20, 1\n\t.eabi_attribute 21, 1\n");
	fprintf(o, "\t.eabi_attribute 23, %d\n\t.eabi_attribute 24, 1\n\t.eabi_attribute 25, 1\n\t.eabi_attribute 26, 2\n", finite_math ? 1 : 3);
	fprintf(o, "\t.eabi_attribute 30, %d\n\t.eabi_attribute 34, %d\n\t.eabi_attribute 18, %d\n",
	        opt_olevel == '0' ? 6 : opt_olevel == '1' || opt_olevel == 'g' ? 1 : opt_olevel == 's' || opt_olevel == 'z' ? 4 : 2, !no_unaligned, short_wchar ? 2 : 4);
	dce(prog); gen_prog = prog;
	for (Func *f = prog; f; f = f->next) if (f->reachable && !f->no_emit) gen_func(f);   /* (an inline-only definition's calls are external) */
	gen_data();
	fprintf(o, "\t.section .note.GNU-stack,\"%s\",%%progbits\n", exec_stack ? "x" : "");   /* an executable stack only for trampolines (as GCC marks it) */
	fclose(o);
}
