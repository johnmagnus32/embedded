/* thumb.c — the Thumb instruction set (T32): its 16-bit encodings, in UAL (.syntax unified), as GCC and the kernel
 * write them. GAS picks the 16-bit form whenever the operands fit it — a constant immediate relaxes to the narrow
 * form iff it fits the field, alignment included (tc-arm.c relax_immediate) — and the flags decide which forms
 * exist: outside an IT block the narrow data-processing instructions SET the flags, so `adds r0, r1, r2` is 16-bit
 * but `add r0, r1, r2` needs Thumb-2's 32-bit add.w. Everything outside that — the 32-bit Thumb-2 encodings, IT
 * blocks, label and literal-pool operands, divided (pre-UAL) syntax — is rejected by name, never mis-encoded. */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "arm-int.h"

static const char *mn;      /* the mnemonic, width suffix stripped */
static int narrow_only;     /* `.n`: the 16-bit form or an error */

__attribute__((noreturn)) static void wide(void) {
	if (narrow_only) die("%s.n: cannot honor width suffix (no 16-bit encoding for these operands)", mn);
	die("%s: these operands need a 32-bit Thumb-2 encoding (not supported yet)", mn);
}
static int nops(void) { return arm_ntok() - 1; }
static const char *op(int i) { const char *t = arm_tok(i); if (!t) die("%s: missing operand %d", mn, i); return t; }
static int is_reg(int i) { return i < arm_ntok() && arm_reg(arm_tok(i)) >= 0; }
static int R(int i) {
	int r = arm_reg(op(i));
	if (r < 0) die("%s: expected a register at operand %d, got '%s'", mn, i, op(i));
	return r;
}
static int lo(int r) { return r >= 0 && r < 8; }
static int is_imm(int i) {   /* `#expr`, or (UAL) a bare expression */
	if (i >= arm_ntok()) return 0;
	const char *t = arm_tok(i);
	return t[0] == '#' || (t[0] != '[' && t[0] != '{' && arm_reg(t) < 0 && !isalpha((unsigned char)t[0]) ? 1 : t[0] == '#');
}
static long IMM(int i) {
	const char *t = op(i);
	if (t[0] == '#') return arm_imm(t);
	char buf[512]; snprintf(buf, sizeof buf, "#%s", t); return arm_imm(buf);
}
static void emit_t(u32 h) { emit16(h); }

/* ---- data processing -------------------------------------------------------------------------------------- */
/* The two-register ALU group: 0100 00 op Rm Rdn, all flag-setting */
enum { A_AND, A_EOR, A_LSL, A_LSR, A_ASR, A_ADC, A_SBC, A_ROR, A_TST, A_RSB, A_CMP, A_CMN, A_ORR, A_MUL, A_BIC, A_MVN };
static void alu16(int opc, int rdn, int rm) {
	if (!lo(rdn) || !lo(rm)) wide();
	emit_t(0x4000u | (u32)opc << 6 | (u32)rm << 3 | (u32)rdn);
}
/* Rd, Rm | Rd, Rn, Rm with Rd == Rn (or, commutative, Rd == Rm): the narrow form's Rdn and Rm */
static void alu_2or3(int opc, int commutative) {
	if (nops() == 2) { alu16(opc, R(1), R(2)); return; }
	if (nops() != 3) die("%s: expected 2 or 3 operands", mn);
	int rd = R(1), rn = R(2), rm = R(3);
	if (rd == rn) alu16(opc, rd, rm);
	else if (commutative && rd == rm) alu16(opc, rd, rn);
	else wide();
}
static void addsub_flags(int add) {   /* adds/subs: Rd, Rn, Rm | Rd, Rm | Rd, Rn, #imm | Rd, #imm */
	int rd = R(1), three = nops() >= 3, rn = three ? R(2) : rd, last = three ? 3 : 2;
	if (nops() > last) wide();                   /* a shifted register */
	if (is_reg(last)) {
		int rm = R(last);
		if (!lo(rd) || !lo(rn) || !lo(rm)) wide();
		emit_t((add ? 0x1800u : 0x1a00u) | (u32)rm << 6 | (u32)rn << 3 | (u32)rd); return;
	}
	long v = IMM(last);
	if (!lo(rd) || !lo(rn)) wide();
	if (rd == rn) { if (v < 0 || v > 255) wide(); emit_t((add ? 0x3000u : 0x3800u) | (u32)rd << 8 | (u32)v); return; }   /* relax_addsub: */
	if (v < 0 || v > 7) wide();                                                                                             /* Rd == Rn: imm8 */
	emit_t((add ? 0x1c00u : 0x1e00u) | (u32)v << 6 | (u32)rn << 3 | (u32)rd);
}
static void addsub_noflags(int add) {   /* add/sub without S: the SP/PC-relative forms and ADD (high registers) */
	int rd = R(1), three = nops() >= 3, rn = three ? R(2) : rd, last = three ? 3 : 2;
	if (nops() > last) wide();
	if (is_reg(last)) {
		int rm = R(last);
		if (!add) wide();
		if (rd != rn && rd != rm) wide();
		int other = rd == rn ? rm : rn;
		emit_t(0x4400u | (u32)(rd & 8) << 4 | (u32)other << 3 | (u32)(rd & 7)); return;
	}
	long v = IMM(last);
	if (rd == 13 && rn == 13) { if (v < 0 || v > 508 || (v & 3)) wide(); emit_t((add ? 0xb000u : 0xb080u) | (u32)v >> 2); return; }
	if (add && lo(rd) && (rn == 13 || rn == 15)) {
		if (v < 0 || v > 1020 || (v & 3)) wide();
		emit_t((rn == 13 ? 0xa800u : 0xa000u) | (u32)rd << 8 | (u32)v >> 2); return;
	}
	wide();
}
static int shift_kind(const char *t) {
	static const char *const k[] = { "lsl", "lsr", "asr", "ror", NULL };
	for (int i = 0; k[i]; i++) if (!strcasecmp(t, k[i])) return i;
	return -1;
}
static void shift_imm(int kind, int rd, int rm, long v) {   /* lsls/lsrs/asrs Rd, Rm, #imm5 (lsr/asr #32 is 0) */
	if (!lo(rd) || !lo(rm)) wide();
	if (v == 0 && kind != 3) kind = 0;                       /* a shift by 0 is lsls #0 (movs Rd, Rm), as GAS writes it */
	if (kind == 3) wide();                                   /* rors #imm: 32-bit only */
	if (kind == 0 ? (v < 0 || v > 31) : (v < 1 || v > 32)) wide();
	emit_t((u32)kind << 11 | ((u32)v & 31) << 6 | (u32)rm << 3 | (u32)rd);
}
static const int shift_alu[4] = { A_LSL, A_LSR, A_ASR, A_ROR };
static void shifts(int kind) {   /* lsls/lsrs/asrs/rors: Rd, Rm, #n | Rd, #n | Rd, Rs | Rd, Rd, Rs */
	int rd = R(1);
	if (nops() == 2) { if (is_imm(2)) shift_imm(kind, rd, rd, IMM(2)); else alu16(shift_alu[kind], rd, R(2)); return; }
	if (nops() != 3) die("%s: expected 2 or 3 operands", mn);
	int rm = R(2);
	if (is_imm(3)) { shift_imm(kind, rd, rm, IMM(3)); return; }
	if (rd != rm) wide();
	alu16(shift_alu[kind], rd, R(3));
}
static void movs(void) {   /* movs Rd, #imm8 | Rd, Rm (lsls #0) | Rd, Rm, <shift> #n | Rd, Rd, <shift> Rs */
	int rd = R(1);
	if (is_imm(2)) { long v = IMM(2); if (!lo(rd) || v < 0 || v > 255 || nops() > 2) wide(); emit_t(0x2000u | (u32)rd << 8 | (u32)v); return; }
	int rm = R(2);
	if (nops() == 2) { if (!lo(rd) || !lo(rm)) wide(); emit_t((u32)rm << 3 | (u32)rd); return; }
	int k = shift_kind(op(3));
	if (k < 0) die("%s: bad shift '%s'", mn, op(3));
	if (nops() != 4) die("%s: expected Rd, Rm, <shift> <amount>", mn);
	if (is_imm(4)) { shift_imm(k, rd, rm, IMM(4)); return; }
	if (rd != rm) wide();
	alu16(shift_alu[k], rd, R(4));
}
static void mov(void) {   /* mov Rd, Rm: MOV (register) T1, any registers, flags untouched */
	int rd = R(1);
	if (is_imm(2) || nops() > 2) wide();
	int rm = R(2);
	emit_t(0x4600u | (u32)(rd & 8) << 4 | (u32)rm << 3 | (u32)(rd & 7));
}
static void cmp(void) {
	int rn = R(1);
	if (nops() != 2) wide();
	if (is_imm(2)) { long v = IMM(2); if (!lo(rn) || v < 0 || v > 255) wide(); emit_t(0x2800u | (u32)rn << 8 | (u32)v); return; }
	int rm = R(2);
	if (rn == 15 || rm == 15) die("%s: r15 not allowed here", mn);
	if (lo(rn) && lo(rm)) emit_t(0x4280u | (u32)rm << 3 | (u32)rn);
	else emit_t(0x4500u | (u32)(rn & 8) << 4 | (u32)rm << 3 | (u32)(rn & 7));
}
static void cmn_tst(int opc) { if (nops() != 2 || is_imm(2)) wide(); alu16(opc, R(1), R(2)); }
static void muls(void) {   /* muls Rd, Rn, Rd | Rd, Rd, Rm | Rd, Rm */
	int rd = R(1);
	if (nops() == 2) { alu16(A_MUL, rd, R(2)); return; }
	int rn = R(2), rm = R(3);
	if (rd == rm) alu16(A_MUL, rd, rn); else if (rd == rn) alu16(A_MUL, rd, rm); else wide();
}
static void negs(int rsb) {   /* negs Rd, Rm | rsbs Rd, Rn, #0 */
	int rd = R(1), rm = R(2);
	if (rsb) { if (nops() != 3 || !is_imm(3) || IMM(3) != 0) wide(); }
	else if (nops() != 2) die("%s: expected Rd, Rm", mn);
	alu16(A_RSB, rd, rm);
}

/* ---- loads and stores ------------------------------------------------------------------------------------- */
enum { W_WORD, W_BYTE, W_HALF, W_SBYTE, W_SHALF };
static void ldst(int load, int width) {
	int rt = R(1);
	const char *a1 = op(2);
	if (a1[0] == '=') die("%s: literal-pool operands (=value) in Thumb: not supported yet", mn);
	if (a1[0] != '[') die("%s: PC-relative label operands in Thumb: not supported yet", mn);
	ArmAddr a = arm_addr(2);
	if (!lo(rt) || !a.P || a.W || a.shift || a.grp) wide();
	if (a.isreg) {
		static const u32 st[] = { 0x5000, 0x5400, 0x5200, 0, 0 }, ld[] = { 0x5800, 0x5c00, 0x5a00, 0x5600, 0x5e00 };
		u32 base = load ? ld[width] : st[width];
		if (!a.U || !lo(a.rn) || !lo(a.rm) || !base) wide();
		emit_t(base | (u32)a.rm << 6 | (u32)a.rn << 3 | (u32)rt); return;
	}
	long v = a.imm;
	if (!a.U) wide();
	switch (width) {
	case W_WORD:
		if (lo(a.rn) && !(v & 3) && v <= 124) { emit_t((load ? 0x6800u : 0x6000u) | (u32)(v >> 2) << 6 | (u32)a.rn << 3 | (u32)rt); return; }
		if (a.rn == 13 && !(v & 3) && v <= 1020) { emit_t((load ? 0x9800u : 0x9000u) | (u32)rt << 8 | (u32)(v >> 2)); return; }
		if (a.rn == 15 && load && !(v & 3) && v <= 1020) { emit_t(0x4800u | (u32)rt << 8 | (u32)(v >> 2)); return; }
		wide();
	case W_BYTE:
		if (lo(a.rn) && v <= 31) { emit_t((load ? 0x7800u : 0x7000u) | (u32)v << 6 | (u32)a.rn << 3 | (u32)rt); return; }
		wide();
	case W_HALF:
		if (lo(a.rn) && !(v & 1) && v <= 62) { emit_t((load ? 0x8800u : 0x8000u) | (u32)(v >> 1) << 6 | (u32)a.rn << 3 | (u32)rt); return; }
		wide();
	default: wide();
	}
}
static void push_pop(int pop, u32 list) {   /* push {r0-r7, lr} / pop {r0-r7, pc} */
	u32 extra = pop ? 1u << 15 : 1u << 14;
	if (list & ~(0xffu | extra)) wide();
	emit_t((pop ? 0xbc00u : 0xb400u) | (list & extra ? 0x100u : 0) | (list & 0xffu));
}
static void ldm_stm(int load, int db) {   /* ldmia/stmia Rn{!}, {list}; with sp! they're pop/push */
	const char *t = op(1); char rb[32]; size_t L = strlen(t); int wb = L && t[L - 1] == '!';
	if (L >= sizeof rb) die("%s: bad base register '%s'", mn, t);
	memcpy(rb, t, L + 1); if (wb) rb[L - 1] = 0;
	int rn = arm_reg(rb); if (rn < 0) die("%s: expected a base register, got '%s'", mn, t);
	int caret; u32 list = arm_reglist(2, &caret);
	if (caret) die("%s: `^` is not allowed in Thumb", mn);
	if (db || (list & ~0xffu)) wide();   /* GAS do_t_ldmstm: only ia, only low-register lists narrow (stmdb sp! is push.w) */
	int single = !(list & (list - 1)); u32 mask = 1u << rn;
	if (lo(rn)) {
		if (load ? wb == !(list & mask) : wb) { emit_t((load ? 0xc800u : 0xc000u) | (u32)rn << 8 | list); return; }   /* ldm: writeback
		                                                                                                           * iff Rn isn't loaded */
		if (!single) wide();
		if ((list & mask) && wb) die("%s: having the base register in the register list when using write back is UNPREDICTABLE", mn);
		emit_t((load ? 0x6800u : 0x6000u) | (u32)rn << 3 | (u32)__builtin_ctz(list)); return;   /* one register: ldr/str Rt, [Rn] */
	}
	if (rn == 13 && wb) {
		/* GAS writes `stmia sp!, {..}` as PUSH — which is STMDB: a different store. Refuse rather than copy that. */
		if (!load) die("%s sp!, {...}: GAS encodes this as push (stmdb), not the store written; use push or stmdb", mn);
		emit_t(0xbc00u | list); return;                    /* ldmia sp! = pop */
	}
	if (rn == 13 && single) { emit_t((load ? 0x9800u : 0x9000u) | (u32)__builtin_ctz(list) << 8); return; }   /* ldr/str Rt, [sp] */
	wide();
}

/* ---- the rest --------------------------------------------------------------------------------------------- */
static long imm8_opt(int i, long lo_, long hi) {   /* an optional #imm (bkpt, udf) */
	if (i >= arm_ntok()) return 0;
	long v = IMM(i); if (v < lo_ || v > hi) wide(); return v;
}
static void two_lo(u32 base) {   /* sxth/uxtb (may say `, ror #0`), rev/rev16/revsh: Rd, Rm, low registers */
	int rd = R(1), rm = R(2);
	if (nops() > 2) {
		if ((base & 0xff00) != 0xb200) die("%s: garbage after Rd, Rm", mn);
		if (nops() == 4 && !strcasecmp(op(3), "ror") && IMM(4) == 0) {} else wide();
	}
	if (!lo(rd) || !lo(rm)) wide();
	emit_t(base | (u32)rm << 3 | (u32)rd);
}
static void cps(int disable) {
	if (nops() != 1) wide();   /* with a mode: 32-bit */
	u32 f = 0;
	for (const char *p = op(1); *p; p++)
		switch (tolower((unsigned char)*p)) { case 'a': f |= 4; break; case 'i': f |= 2; break; case 'f': f |= 1; break;
		default: die("%s: bad interrupt flags '%s'", mn, op(1)); }
	if (!f) die("%s: no interrupt flags", mn);
	emit_t(0xb660u | (disable ? 0x10u : 0) | f);
}

typedef struct { const char *m; void (*f)(int); int arg; } Op;
static void f_addsub_s(int a) { addsub_flags(a); }
static void f_addsub(int a) { addsub_noflags(a); }
static void f_alu(int a) { alu_2or3(a & 0xff, a >> 8); }
static void f_shift(int k) { shifts(k); }
static void f_movs(int u) { (void)u; movs(); }
static void f_mov(int u) { (void)u; mov(); }
static void f_cmp(int u) { (void)u; cmp(); }
static void f_cmntst(int o) { cmn_tst(o); }
static void f_mvns(int u) { (void)u; if (nops() != 2 || is_imm(2)) wide(); alu16(A_MVN, R(1), R(2)); }
static void f_muls(int u) { (void)u; muls(); }
static void f_negs(int r) { negs(r); }
static void f_ld(int w) { ldst(1, w); }
static void f_st(int w) { ldst(0, w); }
static void f_push(int pop) { int c; u32 l = arm_reglist(1, &c); if (c) die("%s: `^` not allowed", mn); push_pop(pop, l); }
static void f_ldm(int db) { ldm_stm(1, db); }
static void f_stm(int db) { ldm_stm(0, db); }
static void f_bx(int link) { int r = R(1); if (nops() != 1) die("%s: expected one register", mn); if (link && r == 15) die("blx: pc not allowed"); emit_t((link ? 0x4780u : 0x4700u) | (u32)r << 3); }
static void f_svc(int u) { (void)u; long v = IMM(1); if (v < 0 || v > 255) die("%s: immediate %ld out of range (0-255)", mn, v); emit_t(0xdf00u | (u32)v); }
static void f_bkpt(int u) { (void)u; emit_t(0xbe00u | (u32)imm8_opt(1, 0, 255)); }
static void f_udf(int u) { (void)u; emit_t(0xde00u | (u32)imm8_opt(1, 0, 255)); }
static void f_hint(int h) { if (nops()) die("%s: takes no operands", mn); emit_t(0xbf00u | (u32)h << 4); }
static void f_two(int base) { two_lo((u32)base); }
static void f_cps(int d) { cps(d); }
static void f_setend(int u) { (void)u; const char *e = op(1); if (!strcasecmp(e, "be")) emit_t(0xb658); else if (!strcasecmp(e, "le")) emit_t(0xb650); else die("setend: expected be or le"); }

static const Op ops[] = {
	{ "adds", f_addsub_s, 1 }, { "subs", f_addsub_s, 0 }, { "add", f_addsub, 1 }, { "sub", f_addsub, 0 },
	{ "ands", f_alu, A_AND | 0x100 }, { "eors", f_alu, A_EOR | 0x100 }, { "adcs", f_alu, A_ADC | 0x100 }, { "orrs", f_alu, A_ORR | 0x100 },
	{ "sbcs", f_alu, A_SBC }, { "bics", f_alu, A_BIC },
	{ "lsls", f_shift, 0 }, { "lsrs", f_shift, 1 }, { "asrs", f_shift, 2 }, { "rors", f_shift, 3 },
	{ "movs", f_movs, 0 }, { "mov", f_mov, 0 }, { "cmp", f_cmp, 0 }, { "cmn", f_cmntst, A_CMN }, { "tst", f_cmntst, A_TST },
	{ "mvns", f_mvns, 0 }, { "muls", f_muls, 0 }, { "negs", f_negs, 0 }, { "rsbs", f_negs, 1 },
	{ "ldr", f_ld, W_WORD }, { "ldrb", f_ld, W_BYTE }, { "ldrh", f_ld, W_HALF }, { "ldrsb", f_ld, W_SBYTE }, { "ldrsh", f_ld, W_SHALF },
	{ "str", f_st, W_WORD }, { "strb", f_st, W_BYTE }, { "strh", f_st, W_HALF },
	{ "push", f_push, 0 }, { "pop", f_push, 1 },
	{ "ldm", f_ldm, 0 }, { "ldmia", f_ldm, 0 }, { "ldmfd", f_ldm, 0 }, { "ldmdb", f_ldm, 1 }, { "ldmea", f_ldm, 1 },
	{ "stm", f_stm, 0 }, { "stmia", f_stm, 0 }, { "stmea", f_stm, 0 }, { "stmdb", f_stm, 1 }, { "stmfd", f_stm, 1 },
	{ "bx", f_bx, 0 }, { "blx", f_bx, 1 },
	{ "svc", f_svc, 0 }, { "swi", f_svc, 0 }, { "bkpt", f_bkpt, 0 }, { "udf", f_udf, 0 },
	{ "nop", f_hint, 0 }, { "yield", f_hint, 1 }, { "wfe", f_hint, 2 }, { "wfi", f_hint, 3 }, { "sev", f_hint, 4 },
	{ "sxth", f_two, 0xb200 }, { "sxtb", f_two, 0xb240 }, { "uxth", f_two, 0xb280 }, { "uxtb", f_two, 0xb2c0 },
	{ "rev", f_two, 0xba00 }, { "rev16", f_two, 0xba40 }, { "revsh", f_two, 0xbac0 },
	{ "cpsie", f_cps, 0 }, { "cpsid", f_cps, 1 }, { "setend", f_setend, 0 },
	{ NULL, NULL, 0 }
};

void thumb_assemble(void) {
	static char m[32];
	const char *t0 = arm_tok(0); size_t L = strlen(t0);
	if (L >= sizeof m) die("unknown Thumb instruction '%s'", t0);
	memcpy(m, t0, L + 1); narrow_only = 0; mn = m;
	if (L > 2 && m[L - 2] == '.' && m[L - 1] == 'w') { m[L - 2] = 0; die("%s.w: 32-bit Thumb-2 encodings are not supported yet", m); }
	if (L > 2 && m[L - 2] == '.' && m[L - 1] == 'n') { m[L - 2] = 0; narrow_only = 1; }
	if (!arm_unified()) die("%s: Thumb code in divided (pre-UAL) syntax is not supported yet — use .syntax unified", m);
	for (const Op *o = ops; o->m; o++) if (!strcmp(o->m, m)) { o->f(o->arg); return; }
	if (m[0] == 'b' || !strncmp(m, "cb", 2)) die("%s: Thumb branches are not supported yet", m);
	if (m[0] == 'i' && m[1] == 't') die("%s: IT blocks are not supported yet", m);
	if (!strcmp(m, "adr")) die("adr: Thumb label operands are not supported yet");
	u32 cc; size_t n = strlen(m);
	for (const Op *o = ops; o->m; o++) {
		size_t k = strlen(o->m);
		if (n == k + 2 && !strncmp(m, o->m, k) && arm_cond(m + k, &cc)) die("%s: a conditional Thumb instruction needs an IT block (not supported yet)", m);
	}
	die("%s: not a supported Thumb instruction (32-bit Thumb-2 encodings are not implemented yet)", m);
}
