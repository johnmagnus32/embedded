/*
 * arm.c — the MACHINE-DEPENDENT backend for ARMv7-A (ARM/A32 encoding), the gas `config/tc-arm.c`
 * equivalent. THE ONLY architecture-specific file: it owns instruction syntax + encoding, register and
 * immediate parsing, the relocation types, and the ELF machine id. It reaches the symbol/section/reloc
 * tables + byte buffers ONLY through the front-end services in as.h (emit32/patch32/here/…), so a new
 * architecture is a sibling of this file with the front-end + obj backend unchanged.
 *
 * Current instruction set (grows one at a time toward the ~85 mnemonics gcc emits): mov (reg),
 * bic (#imm), bl, b. One target: ARMv7-A, ARM mode, little-endian.
 */
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "as.h"

/* ELF identity this backend targets (read by the obj backend when it writes the header). */
const u16 md_e_machine = EM_ARM;
const u32 md_e_flags   = 0x05000000;  /* EF_ARM_EABI_VER5 */

/* ARM relocation type codes (arch-private; the front-end stores them opaquely, the obj backend writes them). */
#define R_ARM_CALL   28   /* bl/blx to a symbol — imm24, addend held in-place */
#define R_ARM_JUMP24 29   /* b to a symbol */

/* The current instruction's tokens (set by md_assemble; the enc_* helpers read them, like tc-arm.c). */
static char **toks; static int ntok;

/* ------------------------------------------------------------------ operand parsing --------------- */
static int reg(const char *t) {   /* r0..r15 + sp/lr/pc/fp/ip/sl aliases; -1 if not a register */
	if (!t) return -1;
	if (!strcmp(t, "sp")) return 13;
	if (!strcmp(t, "lr")) return 14;
	if (!strcmp(t, "pc")) return 15;
	if (!strcmp(t, "fp")) return 11;
	if (!strcmp(t, "ip")) return 12;
	if (!strcmp(t, "sl")) return 10;
	if ((t[0] == 'r' || t[0] == 'R') && isdigit((unsigned char)t[1])) { int n = atoi(t + 1); if (n >= 0 && n <= 15) return n; }
	return -1;
}
static u32 imm(const char *t) {   /* #<num> immediate (dec / 0x hex / negative) */
	if (!t || t[0] != '#') die("expected #immediate, got '%s'", t ? t : "(nil)");
	return (u32)strtol(t + 1, NULL, 0);
}
/* ARM modified-immediate: encode v as (rot<<8)|imm8 where v == ror(imm8, 2*rot). Recover imm8 for a
 * candidate rot as rol(v, 2*rot); the smallest rot whose imm8 fits in 8 bits wins (matches GNU as). */
static u32 modimm(u32 v) {
	for (int rot = 0; rot < 16; rot++) {
		u32 s = (2 * rot) & 31;
		u32 imm8 = (v << s) | (v >> ((32 - s) & 31));   /* rol(v, 2*rot); &31 avoids UB shift at rot 0 */
		if (imm8 <= 0xff) return (rot << 8) | imm8;
	}
	die("immediate #%u not encodable as an ARM modified-immediate", v); return 0;
}

/* ------------------------------------------------------------------ instruction encoders ---------- */
static u32 need_reg(int i) {   /* operand i must be a register */
	int r = (i < ntok) ? reg(toks[i]) : -1;
	if (r < 0) die("%s: expected a register at operand %d", toks[0], i);
	return (u32)r;
}

/* ---- data-processing family (and/eor/sub/rsb/add/adc/sbc/rsc/tst/teq/cmp/cmn/orr/mov/bic/mvn) ------
 * ONE encoder for all 16: ARM DP format cond(4) 00 I(1) opcode(4) S(1) Rn(4) Rd(4) operand2(12). They
 * differ only by the 4-bit opcode + the operand FORM. operand2 is #imm (I=1, modimm) or a plain
 * register Rm (I=0). Shifted-register operand2 ("Rm, lsl #n") is a later increment. */
enum { DP_3OP, DP_MOV, DP_CMP };   /* Rd,Rn,op2 | Rd,op2 | Rn,op2 (flags forced) */
static const struct { const char *name; u32 opc; int form; } dp_tab[] = {
	{"and",0,DP_3OP},{"eor",1,DP_3OP},{"sub",2,DP_3OP},{"rsb",3,DP_3OP},
	{"add",4,DP_3OP},{"adc",5,DP_3OP},{"sbc",6,DP_3OP},{"rsc",7,DP_3OP},
	{"tst",8,DP_CMP},{"teq",9,DP_CMP},{"cmp",10,DP_CMP},{"cmn",11,DP_CMP},
	{"orr",12,DP_3OP},{"mov",13,DP_MOV},{"bic",14,DP_3OP},{"mvn",15,DP_MOV},
};
static const struct { const char *name; u32 code; } cc_tab[] = {
	{"eq",0},{"ne",1},{"cs",2},{"hs",2},{"cc",3},{"lo",3},{"mi",4},{"pl",5},
	{"vs",6},{"vc",7},{"hi",8},{"ls",9},{"ge",10},{"lt",11},{"gt",12},{"le",13},{"al",14},
};
static int lookup_cc(const char *s, u32 *code) {
	for (unsigned i = 0; i < sizeof cc_tab / sizeof *cc_tab; i++)
		if (!strcmp(s, cc_tab[i].name)) { *code = cc_tab[i].code; return 1; }
	return 0;
}

/* operand2 at token index opidx: "#imm" -> I=1 + modimm; a bare register -> I=0 + Rm. */
static u32 operand2(int opidx, u32 *I) {
	if (toks[opidx][0] == '#') { *I = 1; return modimm(imm(toks[opidx])); }
	int rm = reg(toks[opidx]); if (rm < 0) die("%s: bad operand2 '%s'", toks[0], toks[opidx]);
	if (ntok > opidx + 1) die("%s: shifted-register operand2 not supported yet", toks[0]);
	*I = 0; return (u32)rm;
}

static void enc_dp(u32 opc, int form, u32 cond, int s) {
	u32 rd = 0, rn = 0, I; int opidx;
	if (form == DP_MOV)      { rd = need_reg(1);                    opidx = 2; }        /* mov/mvn Rd, op2 */
	else if (form == DP_CMP) { rn = need_reg(1); s = 1;            opidx = 2; }        /* cmp/… Rn, op2 (S forced) */
	else                     { rd = need_reg(1); rn = need_reg(2); opidx = 3; }        /* add/… Rd, Rn, op2 */
	u32 op2 = operand2(opidx, &I);
	emit32((cond << 28) | (I << 25) | (opc << 21) | ((u32)s << 20) | (rn << 16) | (rd << 12) | op2);
}
static void enc_branch(int is_bl) {   /* b/bl <label> */
	if (ntok < 2) die("%s: missing target", toks[0]);
	u32 base = is_bl ? 0xeb000000u : 0xea000000u;
	u32 off = here(); emit32(base);        /* placeholder; patched below or by md_apply_fix */
	const char *name = toks[1];
	if (isdigit((unsigned char)name[0]) && (name[1] == 'b' || name[1] == 'f') && name[2] == 0) {
		int n = name[0] - '0';
		if (name[1] == 'b') {              /* backward local label: resolve now */
			if (!local_defined(n)) die("backward ref to undefined local label %db", n);
			int32_t rel = (int32_t)local_value(n) - (int32_t)(off + 8);
			patch32(cursec, off, base | ((rel >> 2) & 0xffffff));
		} else {                            /* forward local label: front-end resolves via md_apply_fix */
			add_fixup(cursec, off, n);
		}
		return;
	}
	/* named symbol -> external ref -> relocation (imm24 = -2 => addend -8, per ARM REL). */
	patch32(cursec, off, base | 0xfffffe);
	add_reloc(cursec, off, sym_intern(name), is_bl ? R_ARM_CALL : R_ARM_JUMP24);
}

/* Parse a { … } register list (operand tokens toks[1..]) into a 16-bit mask. Handles ranges (r4-r7)
 * and aliases (sp/lr/pc/fp/…); the '{' and '}' are stripped wherever the tokenizer left them. */
static u32 reglist(void) {
	u32 mask = 0;
	for (int i = 1; i < ntok; i++) {
		char t[32]; size_t k = 0;
		for (const char *p = toks[i]; *p && k < sizeof t - 1; p++) if (*p != '{' && *p != '}') t[k++] = *p;
		t[k] = 0;
		if (!t[0]) continue;                          /* a lone '{' or '}' token */
		char *dash = strchr(t, '-');
		if (dash) {                                    /* range rA-rB */
			*dash = 0; int a = reg(t), b = reg(dash + 1);
			if (a < 0 || b < 0 || a > b) die("bad register range '%s' in list", toks[i]);
			for (int r = a; r <= b; r++) mask |= 1u << r;
		} else {
			int r = reg(t); if (r < 0) die("bad register '%s' in list", t);
			mask |= 1u << r;
		}
	}
	return mask;
}
static void enc_push(void) { emit32(0xe92d0000u | reglist()); }   /* STMDB sp!, {list} */
static void enc_pop(void)  { emit32(0xe8bd0000u | reglist()); }   /* LDMIA sp!, {list} */

/* ------------------------------------------------------------------ md hooks ---------------------- */
void md_assemble(char **t, int n) {
	toks = t; ntok = n;
	const char *m = toks[0];
	if (!strcmp(m, "b"))    { enc_branch(0); return; }
	if (!strcmp(m, "bl"))   { enc_branch(1); return; }
	if (!strcmp(m, "push")) { enc_push();    return; }
	if (!strcmp(m, "pop"))  { enc_pop();     return; }
	/* data-processing: a 3-char base (add/mov/cmp/…) + optional {s}{cond} suffix (UAL order). */
	if (strlen(m) >= 3)
		for (unsigned i = 0; i < sizeof dp_tab / sizeof *dp_tab; i++)
			if (!strncmp(m, dp_tab[i].name, 3)) {
				const char *suf = m + 3; u32 cond = 14; int s = 0;   /* default AL, S=0 */
				if (*suf == 's') { s = 1; suf++; }
				if (*suf && !lookup_cc(suf, &cond)) die("%s: bad condition/suffix", m);
				enc_dp(dp_tab[i].opc, dp_tab[i].form, cond, s);
				return;
			}
	die("unknown mnemonic '%s' (not in the ARM backend's instruction set yet)", m);
}

void md_apply_fix(const Fixup *f) {   /* resolve a forward local branch: OR the pc-relative offset in */
	u32 base = read32(f->sec, f->off);
	int32_t rel = (int32_t)local_value(f->local_num) - (int32_t)(f->off + 8);
	patch32(f->sec, f->off, base | ((rel >> 2) & 0xffffff));
}

int md_directive(char **t, int n) {   /* accept + ignore benign ARM/ABI metadata pseudo-ops */
	(void)n; const char *d = t[0];
	if (strstr(d, "arch") || strstr(d, "cpu") || strstr(d, "fpu") || strstr(d, "syntax")
	    || strstr(d, "eabi_attribute") || strstr(d, "fnstart") || strstr(d, "fnend") || strstr(d, "cantunwind")
	    || strstr(d, "save") || strstr(d, "file") || strstr(d, "ident") || strstr(d, "thumb") || strstr(d, "arm"))
		return 1;
	return 0;
}
