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
#include "cc.h"

static FILE *o;
static int label_id;                 /* source of unique .L labels */
static int ret_label;                /* the current function's return label id */
static int uniq(void) { return label_id++; }

static void gen_expr(Node *n);
static void gen_stmt(Node *n);
static void gen_addr(Node *n);

/* Materialize a 32-bit constant into r0 with movw (+movt for the high half) — no literal pool needed. */
static void load_imm(const char *reg, long v) {
	unsigned u = (unsigned)v;
	fprintf(o, "\tmovw %s, #%u\n", reg, u & 0xffff);
	if (u >> 16) fprintf(o, "\tmovt %s, #%u\n", reg, (u >> 16) & 0xffff);
}

/* Emit `cmp r0,r1` then set r0 to 0/1 by the signed condition — the shape of every comparison operator. */
static void gen_setcc(const char *cc) {
	fprintf(o, "\tcmp r0, r1\n\tmov r0, #0\n\tmov%s r0, #1\n", cc);
}

/* Load/store through an address by WIDTH: char is one byte (ldrb/strb, zero-extended), int/pointer four. */
static void load(Type *ty)  { fprintf(o, ty->size == 1 ? "\tldrb r0, [r0]\n" : "\tldr r0, [r0]\n"); }   /* r0=addr -> r0=value */
static void store(Type *ty) { fprintf(o, ty->size == 1 ? "\tstrb r0, [r1]\n" : "\tstr r0, [r1]\n"); }   /* r1=addr, r0=value */

/* Put the ADDRESS of an lvalue in r0. A variable's address is fp+offset; *p's address is p's value. */
static void gen_addr(Node *n) {
	switch (n->kind) {
	case ND_VAR:   fprintf(o, "\tsub r0, r11, #%d\n", -n->offset); return;   /* offset is negative */
	case ND_DEREF: gen_expr(n->lhs); return;                                 /* the pointer value IS the address */
	default: die("cc: not an lvalue");
	}
}

static void gen_expr(Node *n) {
	switch (n->kind) {
	case ND_NUM:  load_imm("r0", n->val); return;
	case ND_VAR:  gen_addr(n); load(n->type); return;       /* address -> r0, then load its value by width */
	case ND_ADDR: gen_addr(n->lhs); return;                 /* &lvalue -> the address itself */
	case ND_DEREF: gen_expr(n->lhs); load(n->type); return; /* pointer -> r0, then load the pointee by width */
	case ND_ASSIGN:
		gen_addr(n->lhs); fprintf(o, "\tpush {r0}\n");      /* destination address */
		gen_expr(n->rhs); fprintf(o, "\tpop {r1}\n");       /* value in r0, address in r1 */
		store(n->lhs->type);                                /* store by width; r0 keeps the value (assignment result) */
		return;
	case ND_NEG:    gen_expr(n->lhs); fprintf(o, "\trsb r0, r0, #0\n"); return;
	case ND_BITNOT: gen_expr(n->lhs); fprintf(o, "\tmvn r0, r0\n"); return;
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
	case ND_CALL: {
		int nargs = 0; for (Node *a = n->args; a; a = a->next) nargs++;
		if (nargs > 4) die("cc: >4 arguments not supported yet (call to %s)", n->name);
		for (Node *a = n->args; a; a = a->next) { gen_expr(a); fprintf(o, "\tpush {r0}\n"); }   /* eval L->R, stack them */
		for (int i = nargs - 1; i >= 0; i--) fprintf(o, "\tpop {r%d}\n", i);                    /* into r0..r(n-1) */
		fprintf(o, "\tbl %s\n", n->name);                                                       /* result in r0 */
		return;
	}
	default: break;
	}

	/* binary operators: left -> r0 (saved), right -> r1, combine into r0 */
	gen_expr(n->lhs); fprintf(o, "\tpush {r0}\n");
	gen_expr(n->rhs); fprintf(o, "\tmov r1, r0\n\tpop {r0}\n");
	switch (n->kind) {
	case ND_ADD:    fprintf(o, "\tadd r0, r0, r1\n"); break;
	case ND_SUB:    fprintf(o, "\tsub r0, r0, r1\n"); break;
	case ND_MUL:    fprintf(o, "\tmul r0, r0, r1\n"); break;
	case ND_DIV:    fprintf(o, "\tsdiv r0, r0, r1\n"); break;
	case ND_MOD:    fprintf(o, "\tsdiv r2, r0, r1\n\tmls r0, r2, r1, r0\n"); break;   /* r0 = r0 - (r0/r1)*r1 */
	case ND_BITAND: fprintf(o, "\tand r0, r0, r1\n"); break;
	case ND_BITOR:  fprintf(o, "\torr r0, r0, r1\n"); break;
	case ND_BITXOR: fprintf(o, "\teor r0, r0, r1\n"); break;
	case ND_SHL:    fprintf(o, "\tlsl r0, r0, r1\n"); break;
	case ND_SHR:    fprintf(o, "\tasr r0, r0, r1\n"); break;                          /* arithmetic (signed int) */
	case ND_EQ: gen_setcc("eq"); break;
	case ND_NE: gen_setcc("ne"); break;
	case ND_LT: gen_setcc("lt"); break;
	case ND_LE: gen_setcc("le"); break;
	case ND_GT: gen_setcc("gt"); break;
	case ND_GE: gen_setcc("ge"); break;
	default: die("cc: unhandled expr node %d", n->kind);
	}
}

static void gen_stmt(Node *n) {
	switch (n->kind) {
	case ND_RETURN:   gen_expr(n->lhs); fprintf(o, "\tb .L%d\n", ret_label); return;
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
		int begin = uniq(), end = uniq();
		fprintf(o, ".L%d:\n", begin);
		gen_expr(n->cond); fprintf(o, "\tcmp r0, #0\n\tbeq .L%d\n", end);
		gen_stmt(n->body); fprintf(o, "\tb .L%d\n.L%d:\n", begin, end);
		return;
	}
	default: gen_expr(n); return;   /* a bare declaration compiles to an empty ND_BLOCK; other exprs run */
	}
}

static void gen_func(Func *f) {
	ret_label = uniq();
	fprintf(o, "\t.global %s\n\t.type %s, %%function\n%s:\n", f->name, f->name, f->name);
	fprintf(o, "\tpush {r11, lr}\n\tmov r11, sp\n");
	if (f->frame) fprintf(o, "\tsub sp, sp, #%d\n", f->frame);
	for (int i = 0; i < f->nparams; i++) fprintf(o, "\tstr r%d, [r11, #%d]\n", i, -4 * (i + 1));   /* spill params */
	for (Node *s = f->body; s; s = s->next) gen_stmt(s);
	fprintf(o, ".L%d:\n\tmov sp, r11\n\tpop {r11, lr}\n\tbx lr\n", ret_label);   /* fall-through return */
}

void gen(Func *prog, const char *out) {
	o = fopen(out, "w"); if (!o) die("cc: cannot open %s", out);
	fprintf(o, "\t.text\n");
	for (Func *f = prog; f; f = f->next) gen_func(f);
	fclose(o);
}
