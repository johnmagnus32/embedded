/*
 * gen.c — the codegen backend: walk the AST and emit ARMv7-A/A32 assembly text (assembled later by our
 * as). A simple STACK MACHINE, no optimization: every expression leaves its result in r0; a binary op
 * evaluates its left operand into r0, pushes it, evaluates the right into r0, then pops the left into r1
 * and combines. Functions follow AAPCS: args arrive in r0..r3, the result returns in r0, r11 is the frame
 * pointer. Locals live at [r11, #-4*(slot+1)]; the frame is set up in the prologue and torn down at the
 * single return label the body branches to.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cc.h"

static FILE *o;
static int label_id;                 /* source of unique .L labels */
static int ret_label;                /* the current function's return label id */
static int brk_lbl, cont_lbl;        /* innermost loop's break/continue targets (0 = not in a loop) */
static int uniq(void) { return ++label_id; }   /* 1-based, so 0 is a valid "none" sentinel */

/* Per-function literal pool: ARM can't load a 32-bit symbol address in one instruction, so a global's
 * address is fetched pc-relative from a `.word <sym>` we drop just past the function's code. */
static int cur_func_id, func_seq;
static int cur_nfixed, cur_variadic;   /* current function's fixed-param count + whether it's variadic */
static Type *cur_ret;                  /* current function's return type (so `return e` widens to 64-bit) */
static char pool[64][64]; static int npool;
static int ngot;   /* -fPIC: per-function counter for GOT-access labels (.LGOT/.LGA) */

/* Per-function C-label -> asm-label-id map (goto/label; forward references get an id on first sight). */
static struct { char name[64]; int id; } clabels[128]; static int nclabels;
static int clabel_id(const char *name) {
	for (int i = 0; i < nclabels; i++) if (!strcmp(clabels[i].name, name)) return clabels[i].id;
	int id = uniq(); if (nclabels < 128) { strncpy(clabels[nclabels].name, name, 63); clabels[nclabels].id = id; nclabels++; }
	return id;
}

static void gen_expr(Node *n);
static void gen_stmt(Node *n);
static void gen_addr(Node *n);
static void gen_binary64(Node *n);

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
static int op_uns(Node *n) { return usual_arith(n->lhs->type, n->rhs->type)->is_unsigned; }

/* A 64-bit value lives in the register PAIR r0(low):r1(high); everything else lives in r0. */
static int is64(Type *t) { return t && t->size == 8; }

/* AAPCS base-standard argument placement. is64[i] marks 8-byte args. Fills onstk[i] and word[i] — a core
 * register index (0..3) when onstk[i]==0, else a WORD offset into the outgoing stack area. 64-bit args are
 * even-aligned (may skip a register and/or pad the stack) and never split. Returns the stack size in words. */
int aapcs_layout(const int *is64a, int n, int *onstk, int *word) {
	int ncrn = 0, nsaa = 0;
	for (int i = 0; i < n; i++) {
		int w = is64a[i] ? 2 : 1;
		if (is64a[i]) ncrn = (ncrn + 1) & ~1;             /* 64-bit: round the core reg up to even */
		if (ncrn <= 4 - w) { onstk[i] = 0; word[i] = ncrn; ncrn += w; }
		else { ncrn = 4; if (is64a[i]) nsaa = (nsaa + 1) & ~1; onstk[i] = 1; word[i] = nsaa; nsaa += w; }
	}
	return nsaa;
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
/* (type) cast on the value in r0: narrowing to char/short truncates + re-extends per the target's sign;
 * widening to int/pointer is a no-op (a narrow load already extended). Non-scalar targets: nothing to do. */
static void gen_cast(Type *ty) {
	if (ty->kind == TY_PTR || ty->kind == TY_ARRAY || ty->kind == TY_STRUCT) return;
	if      (ty->size == 1) fprintf(o, ty->is_unsigned ? "\tuxtb r0, r0\n" : "\tsxtb r0, r0\n");
	else if (ty->size == 2) fprintf(o, ty->is_unsigned ? "\tuxth r0, r0\n" : "\tsxth r0, r0\n");
}

/* Put the ADDRESS of an lvalue in r0. A variable's address is fp+offset; *p's address is p's value. */
static void gen_addr(Node *n) {
	switch (n->kind) {
	case ND_VAR:   /* fp-relative: locals are below fp (negative), stack params above it (positive) */
		if (n->offset < 0) fprintf(o, "\tsub r0, r11, #%d\n", -n->offset);
		else               fprintf(o, "\tadd r0, r11, #%d\n",  n->offset);
		return;
	case ND_DEREF: gen_expr(n->lhs); return;                                 /* the pointer value IS the address */
	case ND_MEMBER: gen_addr(n->lhs); if (n->offset) fprintf(o, "\tadd r0, r0, #%d\n", n->offset); return;
	case ND_GVAR: {
		if (pic) {
			/* PIC: r0 = &sym via the GOT. The `add` sits exactly 8 bytes before the inline literal so
			 * its pc equals the literal's address (P) — the R_ARM_GOT_PREL is then just GOT(sym)-P.
			 * A branch skips the literal so it isn't executed. No absolute address touches .text. */
			int g = ngot++;
			fprintf(o,
			    "\tldr r0, .LGOT%d_%d\n"        /* r0 = GOT(sym) - P  (P = address of the .word below) */
			    "\tadd r0, pc, r0\n"            /* r0 = &GOT[sym]     (pc here == P)                    */
			    "\tb .LGA%d_%d\n"               /* skip the inline literal                              */
			    ".LGOT%d_%d:\n\t.word %s(GOT)\n"
			    ".LGA%d_%d:\n"
			    "\tldr r0, [r0]\n",             /* r0 = GOT[sym] = &sym                                 */
			    cur_func_id, g, cur_func_id, g, cur_func_id, g, n->name, cur_func_id, g);
			return;
		}
		/* non-PIC: address via the per-function literal pool (`ldr r0,.LCPIk` + `.word sym` past the code) */
		if (npool >= 64) die("cc: too many pooled addresses in one function");
		int k = npool++; strncpy(pool[k], n->name, 63);
		fprintf(o, "\tldr r0, .LCPI%d_%d\n", cur_func_id, k);
		return;
	}
	default: die("cc: not an lvalue");
	}
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
static void gen_binary64(Node *n) {
	int u = op_uns(n);
	gen_expr_w(n->lhs, 1); fprintf(o, "\tpush {r0, r1}\n");
	gen_expr_w(n->rhs, 1); fprintf(o, "\tmov r2, r0\n\tmov r3, r1\n\tpop {r0, r1}\n");   /* rhs->r2:r3, lhs->r0:r1 */
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
	case ND_DIV:    fprintf(o, u ? "\tbl __udivdi3\n" : "\tbl __divdi3\n"); return; /* n=r0:r1 d=r2:r3 -> q=r0:r1 */
	case ND_MOD:    fprintf(o, u ? "\tbl __umoddi3\n" : "\tbl __moddi3\n"); return; /*                 -> r=r0:r1 */
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

/* Bitfield read: r0 = &storage-unit on entry -> r0 = the field value. Two shifts isolate the field —
 * left so its top bit reaches bit 31, then right (asr signed / lsr unsigned) down to bit 0. Unit <= 32 bits. */
static void gen_bitfield_load(Node *n) {
	int sz = n->type->size, lsh = 32 - n->bit_offset - n->bit_width, rsh = 32 - n->bit_width;
	fprintf(o, sz == 1 ? "\tldrb r0, [r0]\n" : sz == 2 ? "\tldrh r0, [r0]\n" : "\tldr r0, [r0]\n");
	if (lsh) fprintf(o, "\tlsl r0, r0, #%d\n", lsh);
	if (rsh) fprintf(o, n->type->is_unsigned ? "\tlsr r0, r0, #%d\n" : "\tasr r0, r0, #%d\n", rsh);
}
/* Bitfield write (read-modify-write): store rhs into lhs's field, leaving the storage unit's other bits.
 * &unit -> r1, value -> r0; clear the field bits (bic) and OR the masked, shifted value back in. */
static void gen_bitfield_store(Node *n) {
	Node *lhs = n->lhs;
	int sz = lhs->type->size, bo = lhs->bit_offset, bw = lhs->bit_width;
	long mask = (bw >= 32) ? 0xffffffffL : ((1L << bw) - 1);
	gen_addr(lhs); fprintf(o, "\tpush {r0}\n");
	gen_expr(n->rhs); fprintf(o, "\tpop {r1}\n");                 /* r0 = value, r1 = &unit */
	fprintf(o, sz == 1 ? "\tldrb r2, [r1]\n" : sz == 2 ? "\tldrh r2, [r1]\n" : "\tldr r2, [r1]\n");
	load_imm("r3", mask); fprintf(o, "\tand r0, r0, r3\n");      /* value &= fieldmask */
	if (bo) fprintf(o, "\tlsl r0, r0, #%d\n\tlsl r3, r3, #%d\n", bo, bo);   /* shift value + mask into place */
	fprintf(o, "\tbic r2, r2, r3\n\torr r2, r2, r0\n");          /* clear field, OR the new bits in */
	fprintf(o, sz == 1 ? "\tstrb r2, [r1]\n" : sz == 2 ? "\tstrh r2, [r1]\n" : "\tstr r2, [r1]\n");
}

static void gen_expr(Node *n) {
	switch (n->kind) {
	case ND_NUM:                                                 /* 64-bit literal fills the pair r0:r1 */
		if (is64(n->type)) { load_imm("r0", n->val & 0xffffffff); load_imm("r1", (n->val >> 32) & 0xffffffff); }
		else load_imm("r0", n->val);
		return;
	case ND_MEMBER:
		gen_addr(n);
		if (n->bit_width) { gen_bitfield_load(n); return; }              /* bitfield: extract from its unit */
		if (n->type->kind != TY_ARRAY) load(n->type);
		return;
	case ND_VAR: case ND_GVAR:                             /* address -> r0; scalars then load, arrays decay */
		gen_addr(n); if (n->type->kind != TY_ARRAY) load(n->type); return;
	case ND_ADDR: gen_addr(n->lhs); return;                 /* &lvalue -> the address itself */
	case ND_DEREF: gen_expr(n->lhs); load(n->type); return; /* pointer -> r0, then load the pointee by width */
	case ND_ASSIGN:
		if (n->lhs->kind == ND_MEMBER && n->lhs->bit_width) { gen_bitfield_store(n); return; }   /* bitfield RMW */
		gen_addr(n->lhs); fprintf(o, "\tpush {r0}\n");      /* destination address */
		gen_expr_w(n->rhs, is64(n->lhs->type));             /* value in r0(:r1), widened to the dest width */
		if (is64(n->lhs->type)) { fprintf(o, "\tpop {r2}\n"); store(n->lhs->type); }   /* addr r2; str r0:r1 */
		else { fprintf(o, "\tpop {r1}\n"); store(n->lhs->type); }                      /* addr r1; store by width */
		return;                                             /* r0(:r1) keeps the value (assignment result) */
	case ND_CAST:   gen_expr(n->lhs);
		if (is64(n->type)) { if (!is64(n->lhs->type)) extend64(n->lhs->type); }   /* widen 32->64 (sign/zero) */
		else gen_cast(n->type);                                                   /* 64->32 keeps r0 low word; then narrow to char/short */
		return;
	case ND_COMMA:  gen_expr(n->lhs); gen_expr(n->rhs); return;   /* evaluate left (discard), then right */
	case ND_STMTEXPR:                                            /* ({...}): run the block; the last expr leaves its value in r0(:r1) */
		for (Node *s = n->body; s; s = s->next) gen_stmt(s);
		return;
	case ND_VA_START:                                            /* ap = &(first variadic arg) */
		gen_addr(n->lhs); fprintf(o, "\tadd r1, r11, #%d\n\tstr r1, [r0]\n", 8 + 4 * cur_nfixed);
		return;
	case ND_VA_ARG:                                              /* fetch *ap, advance ap by the arg width */
		gen_addr(n->lhs);
		if (is64(n->type))   /* 64-bit: 8-align ap (AAPCS even-word), then r0=low@[ap], r1=high@[ap+4]; ap += 8 */
			fprintf(o, "\tldr r2, [r0]\n\tadd r2, r2, #7\n\tbic r2, r2, #7\n\tldr r1, [r2, #4]\n\tadd r3, r2, #8\n\tstr r3, [r0]\n\tldr r0, [r2]\n");
		else
			fprintf(o, "\tldr r1, [r0]\n\tadd r2, r1, #4\n\tstr r2, [r0]\n\tldr r0, [r1]\n");
		return;
	case ND_NEG:    gen_expr_w(n->lhs, is64(n->type));
		if (is64(n->type)) fprintf(o, "\trsbs r0, r0, #0\n\trsc r1, r1, #0\n");   /* 0 - value (64-bit) */
		else               fprintf(o, "\trsb r0, r0, #0\n");
		return;
	case ND_BITNOT: gen_expr_w(n->lhs, is64(n->type));
		if (is64(n->type)) fprintf(o, "\tmvn r0, r0\n\tmvn r1, r1\n");
		else               fprintf(o, "\tmvn r0, r0\n");
		return;
	case ND_NOT:    gen_expr(n->lhs); fprintf(o, "\tcmp r0, #0\n\tmov r0, #0\n\tmoveq r0, #1\n"); return;
	case ND_AND: {                                          /* a && b — short-circuit */
		int f = uniq(), e = uniq();
		gen_expr(n->lhs); fprintf(o, "\tcmp r0, #0\n\tbeq .L%d\n", f);
		gen_expr(n->rhs); fprintf(o, "\tcmp r0, #0\n\tbeq .L%d\n", f);
		fprintf(o, "\tmov r0, #1\n\tb .L%d\n.L%d:\n\tmov r0, #0\n.L%d:\n", e, f, e);
		return;
	}
	case ND_OR: {                                           /* a || b — short-circuit */
		int t = uniq(), e = uniq();
		gen_expr(n->lhs); fprintf(o, "\tcmp r0, #0\n\tbne .L%d\n", t);
		gen_expr(n->rhs); fprintf(o, "\tcmp r0, #0\n\tbne .L%d\n", t);
		fprintf(o, "\tmov r0, #0\n\tb .L%d\n.L%d:\n\tmov r0, #1\n.L%d:\n", e, t, e);
		return;
	}
	case ND_COND: {                                         /* cond ? then : els — both arms widened to the result width */
		int els = uniq(), end = uniq(), w = is64(n->type);
		gen_expr(n->cond); fprintf(o, "\tcmp r0, #0\n\tbeq .L%d\n", els);
		gen_expr_w(n->then, w); fprintf(o, "\tb .L%d\n.L%d:\n", end, els);
		gen_expr_w(n->els, w);  fprintf(o, ".L%d:\n", end);
		return;
	}
	case ND_CALL: {
		Node *av[16]; int nargs = 0; for (Node *a = n->args; a; a = a->next) { if (nargs >= 16) die("cc: too many args"); av[nargs++] = a; }
		int is64a[16], onstk[16], word[16];
		const char *callee = n->lhs ? 0 : n->name;                 /* only a direct call has a known signature */
		for (int i = 0; i < nargs; i++) {                          /* a 64-bit param is placed 64-bit even if the arg is narrower */
			Type *pt = callee ? func_param_type(callee, i) : 0;
			is64a[i] = pt ? is64(pt) : is64(av[i]->type);
		}
		int nstk = aapcs_layout(is64a, nargs, onstk, word);        /* AAPCS placement: reg index or stack word */
		int regwords = 0; for (int i = 0; i < nargs; i++) if (!onstk[i]) regwords += is64a[i] ? 2 : 1;
		int cw = n->lhs ? 1 : 0;                                    /* indirect: 1 staging word for the callee ptr */
		/* Reserve one area from sp: [0, nstk) = outgoing stack args; [nstk, +regwords) = register-arg staging;
		 * [.. ] = callee-ptr staging. Everything is addressed off sp, so nested-call arg evaluation (which
		 * moves sp and restores it) never disturbs already-placed args. Pad so sp stays 8-aligned at the call. */
		int stageb = nstk * 4, total = nstk + regwords + cw, resv = total * 4 + ((total * 4 & 7) ? 4 : 0);
		if (resv) fprintf(o, "\tsub sp, sp, #%d\n", resv);
		int stageword[16], si = 0;
		for (int i = 0; i < nargs; i++) {
			gen_expr_w(av[i], is64a[i]);   /* widen a narrow arg to a 64-bit param (sign/zero) */
			if (onstk[i]) { fprintf(o, "\tstr r0, [sp, #%d]\n", word[i] * 4); if (is64a[i]) fprintf(o, "\tstr r1, [sp, #%d]\n", word[i] * 4 + 4); }
			else { stageword[i] = si; fprintf(o, "\tstr r0, [sp, #%d]\n", stageb + si * 4); si++; if (is64a[i]) { fprintf(o, "\tstr r1, [sp, #%d]\n", stageb + si * 4); si++; } }
		}
		if (n->lhs) { gen_expr(n->lhs); fprintf(o, "\tstr r0, [sp, #%d]\n", stageb + regwords * 4); }   /* callee ptr */
		for (int i = 0; i < nargs; i++) if (!onstk[i]) {           /* load staged register args into r0..r3 */
			fprintf(o, "\tldr r%d, [sp, #%d]\n", word[i], stageb + stageword[i] * 4);
			if (is64a[i]) fprintf(o, "\tldr r%d, [sp, #%d]\n", word[i] + 1, stageb + (stageword[i] + 1) * 4);
		}
		if (n->lhs) fprintf(o, "\tldr r12, [sp, #%d]\n\tblx r12\n", stageb + regwords * 4);
		else        fprintf(o, "\tbl %s\n", n->name);              /* result in r0(:r1) */
		if (resv) fprintf(o, "\tadd sp, sp, #%d\n", resv);
		return;
	}
	default: break;
	}

	/* 64-bit operand(s) -> the register-pair path. A comparison yields a 32-bit bool but reads 64-bit
	 * operands, so decide by the operands; every other op's result width is exactly n->type (add_type set
	 * shifts to promote(lhs), arithmetic to usual_arith), so decide by that. */
	int is_cmp = n->kind==ND_EQ||n->kind==ND_NE||n->kind==ND_LT||n->kind==ND_LE||n->kind==ND_GT||n->kind==ND_GE;
	if (is_cmp ? (is64(n->lhs->type) || is64(n->rhs->type)) : is64(n->type)) { gen_binary64(n); return; }

	/* binary operators: left -> r0 (saved), right -> r1, combine into r0 */
	gen_expr(n->lhs); fprintf(o, "\tpush {r0}\n");
	gen_expr(n->rhs); fprintf(o, "\tmov r1, r0\n\tpop {r0}\n");
	switch (n->kind) {
	case ND_ADD:    fprintf(o, "\tadd r0, r0, r1\n"); break;
	case ND_SUB:    fprintf(o, "\tsub r0, r0, r1\n"); break;
	case ND_MUL:    fprintf(o, "\tmul r0, r0, r1\n"); break;
	case ND_DIV:    fprintf(o, op_uns(n) ? "\tudiv r0, r0, r1\n" : "\tsdiv r0, r0, r1\n"); break;
	case ND_MOD:    fprintf(o, op_uns(n) ? "\tudiv r2, r0, r1\n\tmls r0, r2, r1, r0\n"      /* r0 = r0 - (r0/r1)*r1 */
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
	if (s[0] == 'r' && s[1]) { int n = atoi(s + 1); if (n >= 0 && n <= 15) return n; }
	return -1;
}
static const char *asm_regname(int n) {
	static char b[4];
	if (n == 13) return "sp";
	if (n == 14) return "lr";
	if (n == 15) return "pc";
	snprintf(b, sizeof b, "r%d", n);
	return b;
}
static int asm_is_input(const char *c)  { return !strchr(c, '=') || strchr(c, '+'); }   /* "r"/"+r"/"i" read */
/* Emit the template, substituting %0..%9 with each operand's register (or immediate) and turning \n/\t
 * escapes into real newlines/tabs so our as sees one instruction per line. %% -> %, %= (unique id) dropped,
 * a leading modifier letter (%w0/%c0) is ignored. */
static void emit_asm_template(const char *t, char subst[][24], int nops) {
	fprintf(o, "\t");
	for (const char *p = t; *p; ) {
		if (*p == '%') {
			p++;
			if (*p == '%') { fputc('%', o); p++; }
			else if (*p == '=') { p++; }
			else {
				if (*p && !(*p >= '0' && *p <= '9')) p++;   /* skip a modifier letter (%w0/%c0/…) */
				if (*p >= '0' && *p <= '9') { int i = *p - '0'; p++; if (i < nops) fputs(subst[i], o); }
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
static void gen_asm(Node *n) {
	Node *ops[16]; int nops = 0;
	for (Node *a = n->args; a; a = a->next) { if (nops >= 16) die("cc: too many asm operands"); ops[nops++] = a; }
	int nouts = n->val, regof[16], used = 0; char subst[16][24];
	for (int i = 0; i < nops; i++) {                       /* immediates + pinned registers */
		if (strchr(ops[i]->cons, 'i')) { regof[i] = -2; snprintf(subst[i], 24, "%ld", ops[i]->val); continue; }
		int rn = asm_regnum(ops[i]->reg); regof[i] = rn; if (rn >= 0) used |= 1 << rn;
	}
	for (int i = 0; i < nops; i++) if (regof[i] == -1) {   /* allocate the unpinned ones (avoid r11/sp/lr/pc) */
		int rn = -1;
		for (int c = 0; c <= 12; c++) { if (c == 11) continue; if (!(used & (1 << c))) { rn = c; break; } }
		if (rn < 0) die("cc: out of registers for asm operands");
		regof[i] = rn; used |= 1 << rn;
	}
	for (int i = 0; i < nops; i++) if (regof[i] >= 0) snprintf(subst[i], 24, "%s", asm_regname(regof[i]));
	for (int i = 0; i < nops; i++) if (regof[i] >= 0 && asm_is_input(ops[i]->cons)) { gen_expr(ops[i]); fprintf(o, "\tpush {r0}\n"); }
	for (int i = nops - 1; i >= 0; i--) if (regof[i] >= 0 && asm_is_input(ops[i]->cons)) fprintf(o, "\tpop {%s}\n", asm_regname(regof[i]));
	emit_asm_template(n->asm_tmpl, subst, nops);
	for (int i = 0; i < nops; i++) if (i < nouts && regof[i] >= 0) fprintf(o, "\tpush {%s}\n", asm_regname(regof[i]));
	for (int i = nops - 1; i >= 0; i--) if (i < nouts && regof[i] >= 0) { gen_addr(ops[i]); fprintf(o, "\tmov r1, r0\n\tpop {r0}\n"); store(ops[i]->type); }
}

static void gen_stmt(Node *n) {
	switch (n->kind) {
	case ND_RETURN:   if (n->lhs) gen_expr_w(n->lhs, is64(cur_ret)); fprintf(o, "\tb .L%d\n", ret_label); return;   /* widen to the return type; lhs NULL for `return;` */
	case ND_EXPRSTMT: gen_expr(n->lhs); return;
	case ND_BLOCK:    for (Node *s = n->body; s; s = s->next) gen_stmt(s); return;
	case ND_IF: {
		int els = uniq(), end = uniq();
		gen_expr(n->cond); fprintf(o, "\tcmp r0, #0\n\tbeq .L%d\n", els);
		gen_stmt(n->then); fprintf(o, "\tb .L%d\n.L%d:\n", end, els);
		if (n->els) gen_stmt(n->els);
		fprintf(o, ".L%d:\n", end);
		return;
	}
	case ND_WHILE: {
		int begin = uniq(), end = uniq(), sb = brk_lbl, sc = cont_lbl;
		brk_lbl = end; cont_lbl = begin;                    /* continue -> re-test, break -> exit */
		fprintf(o, ".L%d:\n", begin);
		gen_expr(n->cond); fprintf(o, "\tcmp r0, #0\n\tbeq .L%d\n", end);
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
		gen_expr(n->cond); fprintf(o, "\tcmp r0, #0\n\tbne .L%d\n.L%d:\n", begin, end);
		brk_lbl = sb; cont_lbl = sc;
		return;
	}
	case ND_FOR: {
		int begin = uniq(), end = uniq(), cont = uniq(), sb = brk_lbl, sc = cont_lbl;
		brk_lbl = end; cont_lbl = cont;                     /* continue -> the inc step, break -> exit */
		if (n->init) gen_stmt(n->init);
		fprintf(o, ".L%d:\n", begin);
		if (n->cond) { gen_expr(n->cond); fprintf(o, "\tcmp r0, #0\n\tbeq .L%d\n", end); }
		gen_stmt(n->body);
		fprintf(o, ".L%d:\n", cont);
		if (n->inc) gen_expr(n->inc);
		fprintf(o, "\tb .L%d\n.L%d:\n", begin, end);
		brk_lbl = sb; cont_lbl = sc;
		return;
	}
	case ND_SWITCH: {                                       /* eval, compare-chain to each case, then body */
		int end = uniq(), sb = brk_lbl; brk_lbl = end;
		gen_expr(n->cond);                                  /* switch value -> r0 */
		int def = 0;
		for (Node *c = n->case_list; c; c = c->case_next) {
			c->offset = uniq();                             /* the label this case jumps to */
			if (c->is_default) { def = c->offset; continue; }
			if (c->is_range) {                                  /* case lo ... hi: match the inclusive range */
				int skip = uniq();
				load_imm("r1", c->val);  fprintf(o, "\tcmp r0, r1\n\tblt .L%d\n", skip);
				load_imm("r1", c->val2); fprintf(o, "\tcmp r0, r1\n\tbgt .L%d\n", skip);
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
	case ND_ASM: gen_asm(n); return;   /* %N-substituted template + constraint-driven operand load/store */
	case ND_GOTO:  fprintf(o, "\tb .L%d\n", clabel_id(n->name)); return;
	case ND_LABEL: fprintf(o, ".L%d:\n", clabel_id(n->name)); return;
	case ND_BREAK:    if (!brk_lbl)  die("cc: break outside a loop");    fprintf(o, "\tb .L%d\n", brk_lbl);  return;
	case ND_CONTINUE: if (!cont_lbl) die("cc: continue outside a loop"); fprintf(o, "\tb .L%d\n", cont_lbl); return;
	default: gen_expr(n); return;   /* a bare declaration compiles to an empty ND_BLOCK; other exprs run */
	}
}

static void gen_func(Func *f) {
	ret_label = uniq(); cur_func_id = func_seq++; npool = 0; nclabels = 0; ngot = 0;
	cur_nfixed = f->nfixed_words; cur_variadic = f->variadic; cur_ret = f->ret_type;
	if (!f->is_static) fprintf(o, "\t.global %s\n", f->name);   /* `static` -> file-local symbol */
	fprintf(o, "\t.type %s, %%function\n%s:\n", f->name, f->name);
	if (f->variadic) fprintf(o, "\tpush {r0, r1, r2, r3}\n");   /* save area: args become contiguous at [r11,#8+4i] */
	fprintf(o, "\tpush {r11, lr}\n\tmov r11, sp\n");
	if (f->frame) fprintf(o, "\tsub sp, sp, #%d\n", f->frame);
	if (!f->variadic) for (int w = 0; w < f->arg_regs; w++) if (f->arg_off[w]) fprintf(o, "\tstr r%d, [r11, #%d]\n", w, f->arg_off[w]);   /* spill incoming r0..r3 to param slots (word-based) */
	for (Node *s = f->body; s; s = s->next) gen_stmt(s);
	fprintf(o, ".L%d:\n\tmov sp, r11\n\tpop {r11, lr}\n", ret_label);            /* epilogue */
	if (f->variadic) fprintf(o, "\tadd sp, sp, #16\n");        /* discard the r0..r3 save area */
	fprintf(o, "\tbx lr\n");
	if (npool) { fprintf(o, "\t.align 2\n");                                     /* address pool, past the code */
		for (int k = 0; k < npool; k++) fprintf(o, ".LCPI%d_%d:\n\t.word %s\n", cur_func_id, k, pool[k]); }
}

/* Emit the file-scope objects: string literals in .rodata, initialized globals in .data, zero-init in .bss;
 * an `extern` decl defines nothing — it's a reference the linker resolves against the real definition. */
static void gen_data(void) {
	for (Gvar *g = globals; g; g = g->next) if (g->is_str) {
		fprintf(o, "\t.section .rodata\n%s:\n\t.asciz \"%s\"\n", g->name, g->str);
	}
	for (Gvar *g = globals; g; g = g->next) if (!g->is_str && g->init) {
		fprintf(o, "\t.data\n");
		if (!g->is_static) fprintf(o, "\t.global %s\n", g->name);
		fprintf(o, "\t.type %s, %%object\n", g->name);      /* STT_OBJECT + a real .size -> exported size (copy relocs) */
		if (align_of(g->type) >= 4) fprintf(o, "\t.align 2\n"); else if (align_of(g->type) == 2) fprintf(o, "\t.align 1\n");
		fprintf(o, "%s:\n", g->name);
		for (Init *it = g->init; it; it = it->next) {
			if (it->kind == INIT_CONST) {
				if (it->size == 8) fprintf(o, "\t.word %ld\n\t.word %ld\n", it->val & 0xffffffff, (it->val >> 32) & 0xffffffff);   /* low, high */
				else fprintf(o, it->size == 1 ? "\t.byte %ld\n" : it->size == 2 ? "\t.hword %ld\n" : "\t.word %ld\n", it->val);
			}
			else if (it->kind == INIT_SYM) fprintf(o, "\t.word %s\n", it->sym);
			else fprintf(o, "\t.space %d\n", it->size);
		}
		fprintf(o, "\t.size %s, . - %s\n", g->name, g->name);
	}
	for (Gvar *g = globals; g; g = g->next) if (!g->is_str && !g->init && !g->is_extern) {
		fprintf(o, "\t.bss\n");
		if (!g->is_static) fprintf(o, "\t.global %s\n", g->name);
		fprintf(o, "\t.type %s, %%object\n", g->name);
		if (align_of(g->type) >= 4) fprintf(o, "\t.align 2\n"); else if (align_of(g->type) == 2) fprintf(o, "\t.align 1\n");
		fprintf(o, "%s:\n\t.space %d\n", g->name, g->type->size);
		fprintf(o, "\t.size %s, . - %s\n", g->name, g->name);
	}
}

void gen(Func *prog, const char *out) {
	o = fopen(out, "w"); if (!o) die("cc: cannot open %s", out);
	fprintf(o, "\t.text\n");
	for (Func *f = prog; f; f = f->next) gen_func(f);
	gen_data();
	fclose(o);
}
