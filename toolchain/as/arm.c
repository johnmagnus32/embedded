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
#include <stdio.h>
#include "as.h"

/* ELF identity this backend targets (read by the obj backend when it writes the header). */
const u16 md_e_machine = EM_ARM;
const u32 md_e_flags   = 0x05000000;  /* EF_ARM_EABI_VER5 */

/* ARM relocation type codes (arch-private; the front-end stores them opaquely, the obj backend writes them). */
#define R_ARM_ABS32  2    /* .word <symbol> — 32-bit absolute */
#define R_ARM_CALL   28   /* bl/blx to a symbol — imm24, addend held in-place */
#define R_ARM_JUMP24 29   /* b to a symbol */
#define R_ARM_GOT_PREL 96 /* .word <symbol>(GOT) — PC-relative offset to the symbol's GOT slot (PIC) */
const u32 md_r_abs32    = R_ARM_ABS32;    /* the front-end uses this for `.word <symbol>`       */
const u32 md_r_got_prel = R_ARM_GOT_PREL; /* the front-end uses this for `.word <symbol>(GOT)`  */

/* Pc-relative literal loads (`ldr Rd, .Llabel[+/-N]`) — the target pool label usually sits AFTER the
 * code, so we emit `ldr Rd, [pc,#0]` and fix the 12-bit offset in md_finish once all labels are known. */
static struct { int sec; u32 off; char sym[64]; long addend; } ldrlit[256]; static int nldrlit;
/* Named-symbol branches (b/bl <sym>): deferred to md_finish so we can RESOLVE ones defined in the same
 * section (like GNU as does for local labels) and only RELOCATE truly external/cross-section ones. */
static struct { int sec; u32 off; char sym[64]; int is_bl; } brfix[1024]; static int nbrfix;

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

static int shift_type(const char *s) {   /* lsl=0 lsr=1 asr=2 ror=3; -1 if not a shift */
	if (!strcmp(s, "lsl") || !strcmp(s, "asl")) return 0;   /* asl = legacy synonym for lsl */
	if (!strcmp(s, "lsr")) return 1;
	if (!strcmp(s, "asr")) return 2;
	if (!strcmp(s, "ror")) return 3;
	return -1;
}
/* Build the 12-bit shifted-register operand2 for Rm with a shift at tokens [si]=type [si+1]=amount:
 * "Rm, lsl #n" -> (n<<7)|(type<<5)|Rm ; "Rm, lsl Rs" -> (Rs<<8)|(type<<5)|(1<<4)|Rm. */
static u32 shifted_reg(u32 rm, int si) {
	int st = shift_type(toks[si]); if (st < 0) die("%s: bad shift '%s'", toks[0], toks[si]);
	const char *amt = (si + 1 < ntok) ? toks[si + 1] : NULL; if (!amt) die("%s: shift needs an amount", toks[0]);
	if (amt[0] == '#') return (((u32)strtol(amt + 1, NULL, 0) & 31) << 7) | ((u32)st << 5) | rm;
	int rs = reg(amt); if (rs < 0) die("%s: bad shift amount '%s'", toks[0], amt);
	return ((u32)rs << 8) | ((u32)st << 5) | (1u << 4) | rm;
}

/* operand2 at token index opidx: "#imm" -> I=1 + modimm; register [,shift] -> I=0 + shifted-reg. */
static u32 operand2(int opidx, u32 *I) {
	if (toks[opidx][0] == '#') { *I = 1; return modimm(imm(toks[opidx])); }
	int rm = reg(toks[opidx]); if (rm < 0) die("%s: bad operand2 '%s'", toks[0], toks[opidx]);
	*I = 0;
	return (ntok > opidx + 1) ? shifted_reg((u32)rm, opidx + 1) : (u32)rm;
}

static void enc_dp(u32 opc, int form, u32 cond, int s) {
	u32 rd = 0, rn = 0, I; int opidx;
	if (form == DP_MOV)      { rd = need_reg(1);                    opidx = 2; }        /* mov/mvn Rd, op2 */
	else if (form == DP_CMP) { rn = need_reg(1); s = 1;            opidx = 2; }        /* cmp/… Rn, op2 (S forced) */
	else                     { rd = need_reg(1); rn = need_reg(2); opidx = 3; }        /* add/… Rd, Rn, op2 */
	u32 op2 = operand2(opidx, &I);
	emit32((cond << 28) | (I << 25) | (opc << 21) | ((u32)s << 20) | (rn << 16) | (rd << 12) | op2);
}
static void enc_branch(int is_bl, u32 cond) {   /* b/bl{cond} <label> */
	if (ntok < 2) die("%s: missing target", toks[0]);
	u32 base = (cond << 28) | 0x0a000000u | ((u32)is_bl << 24);   /* cond 101 L imm24 */
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
	/* named symbol: defer to md_finish — resolve if defined in THIS section (local label / same-file),
	 * else relocate. The placeholder keeps the cond/101/L opcode byte; imm24 is filled in later. */
	if (nbrfix >= 1024) die("too many branch fixups");
	brfix[nbrfix].sec = cursec; brfix[nbrfix].off = off; brfix[nbrfix].is_bl = is_bl;
	strncpy(brfix[nbrfix].sym, name, sizeof brfix[0].sym - 1); brfix[nbrfix].sym[sizeof brfix[0].sym - 1] = 0;
	nbrfix++;
}

static void enc_bx(u32 cond) {   /* bx{cond} Rm — branch-and-exchange (interworking return) */
	emit32((cond << 28) | 0x012fff10u | need_reg(1));
}

/* ---- single data transfer: ldr/str{b}{cond} Rd, <addr> --------------------------------------------
 * Encoding: cond 01 I P U B W L Rn Rd offset(12). NOTE I is INVERTED vs data-processing: I=0 => the
 * offset is a 12-bit IMMEDIATE (U = sign), I=1 => a register (optionally lsl #n). Addressing:
 *   [Rn]            P=1 W=0 off=0        [Rn,#imm]     P=1 W=0        [Rn,#imm]!  P=1 W=1  (pre, writeback)
 *   [Rn],#imm       P=0 W=0 (post)       [Rn,Rm]       P=1 I=1        [Rn,Rm,lsl #n]  P=1 I=1 shift
 * B = byte (ldrb/strb), L = load (ldr). */
static void enc_ldst(u32 cond, int is_load, int is_byte) {
	u32 rd = need_reg(1);
	if (ntok >= 3 && toks[2][0] != '[') {   /* pc-relative literal load: ldr Rd, label[+/-N] */
		if (is_byte || !is_load) die("%s: literal form supported for word ldr only", toks[0]);
		if (nldrlit >= 256) die("too many ldr literals");
		u32 off = here(); emit32((cond << 28) | 0x059f0000u | (rd << 12));   /* ldr Rd, [pc, #0] placeholder */
		char *plus = strpbrk(toks[2], "+-");
		size_t k = plus ? (size_t)(plus - toks[2]) : strlen(toks[2]);
		if (k >= sizeof ldrlit[0].sym) k = sizeof ldrlit[0].sym - 1;
		ldrlit[nldrlit].sec = cursec; ldrlit[nldrlit].off = off;
		memcpy(ldrlit[nldrlit].sym, toks[2], k); ldrlit[nldrlit].sym[k] = 0;
		ldrlit[nldrlit].addend = plus ? strtol(plus, NULL, 0) : 0;
		nldrlit++;
		return;
	}
	/* the tokenizer split the [ ] address on commas/spaces — rejoin it (spaces preserved) to re-scan */
	char buf[128]; size_t bl = 0;
	for (int i = 2; i < ntok && bl < sizeof buf - 1; i++) bl += (size_t)snprintf(buf + bl, sizeof buf - bl, "%s%s", i > 2 ? " " : "", toks[i]);
	if (buf[0] != '[') die("%s: expected [Rn ...] address, got '%s'", toks[0], buf);
	u32 W = 0; if (bl && buf[bl - 1] == '!') { W = 1; buf[--bl] = 0; }        /* trailing '!' = writeback */
	char *rb = strchr(buf, ']'); if (!rb) die("%s: missing ']' in address", toks[0]);
	*rb = 0; char *after = rb + 1; while (*after == ' ') after++;             /* text after ']' = post-index */

	int rn = reg(strtok(buf + 1, " ")); if (rn < 0) die("%s: bad base register", toks[0]);
	u32 P, U = 1, I = 0, off = 0;
	char *ofs = *after ? after : strtok(NULL, " ");                          /* post uses `after`, else inside */
	P = *after ? 0 : 1;
	if (ofs) {
		if (ofs[0] == '#') {                                                  /* immediate offset */
			long v = strtol(ofs + 1, NULL, 0); if (v < 0) { U = 0; v = -v; }
			off = (u32)v & 0xfff;
		} else {                                                              /* register offset [, lsl #n] */
			int rm = reg(ofs); if (rm < 0) die("%s: bad offset register '%s'", toks[0], ofs);
			I = 1; off = (u32)rm;
			char *sh = P ? strtok(NULL, " ") : NULL;
			if (sh) {
				int st = shift_type(sh); if (st < 0) die("%s: bad index shift '%s'", toks[0], sh);
				char *amt = strtok(NULL, " "); if (!amt || amt[0] != '#') die("%s: %s needs #amount", toks[0], sh);
				off |= ((u32)strtol(amt + 1, NULL, 0) & 31) << 7;             /* shift amount, bits 11:7 */
				off |= (u32)st << 5;                                          /* shift type, bits 6:5 (lsl/lsr/asr/ror) */
			}
		}
	}
	emit32((cond << 28) | (1u << 26) | (I << 25) | (P << 24) | (U << 23) | ((u32)is_byte << 22)
	     | (W << 21) | ((u32)is_load << 20) | ((u32)rn << 16) | (rd << 12) | off);
}

static void enc_extend(u32 cond, u32 base) {   /* {u,s}xt{b,h}{cond} Rd, Rm — zero/sign-extend byte/half (rotate 0) */
	u32 rd = need_reg(1), rm = need_reg(2);
	emit32((cond << 28) | base | (rd << 12) | rm);
}

/* Parse a { … } register list (operand tokens toks[1..]) into a 16-bit mask. Handles ranges (r4-r7)
 * and aliases (sp/lr/pc/fp/…); the '{' and '}' are stripped wherever the tokenizer left them. */
static u32 reglist_at(int start) {   /* parse a { … } register list from toks[start..] into a 16-bit mask */
	u32 mask = 0;
	for (int i = start; i < ntok; i++) {
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
static void enc_push(u32 cond) { emit32((cond << 28) | 0x092d0000u | reglist_at(1)); }   /* STMDB sp!, {list} */
static void enc_pop(u32 cond)  { emit32((cond << 28) | 0x08bd0000u | reglist_at(1)); }   /* LDMIA sp!, {list} */

/* ldm/stm{ia,ib,da,db} Rn[!], {list} — load/store multiple. P/U select the addressing mode:
 * IA=P0U1 (increment after), IB=P1U1, DA=P0U0, DB=P1U0 (decrement before). */
static void enc_ldstm(int is_load, u32 cond, u32 P, u32 U) {
	char rn[8]; strncpy(rn, toks[1], sizeof rn - 1); rn[sizeof rn - 1] = 0;
	u32 wb = 0; size_t l = strlen(rn); if (l && rn[l - 1] == '!') { wb = 1; rn[l - 1] = 0; }
	int r = reg(rn); if (r < 0) die("%s: bad base register '%s'", toks[0], toks[1]);
	u32 base = 0x08000000u | (P << 24) | (U << 23) | ((u32)is_load << 20);
	emit32((cond << 28) | base | (wb << 21) | ((u32)r << 16) | reglist_at(2));
}

/* standalone shifts: lsl/lsr/asr/ror Rd, Rm, #n|Rs  == MOV Rd, Rm <shift>. */
static void enc_shift(int st, u32 cond, int s) {
	u32 rd = need_reg(1), rm = need_reg(2);
	const char *amt = (3 < ntok) ? toks[3] : NULL; if (!amt) die("%s: missing shift amount", toks[0]);
	u32 op2;
	if (amt[0] == '#') op2 = (((u32)strtol(amt + 1, NULL, 0) & 31) << 7) | ((u32)st << 5) | rm;
	else { int rs = reg(amt); if (rs < 0) die("%s: bad shift amount", toks[0]); op2 = ((u32)rs << 8) | ((u32)st << 5) | (1u << 4) | rm; }
	emit32((cond << 28) | (13u << 21) | ((u32)s << 20) | (rd << 12) | op2);   /* MOV */
}

/* movw/movt Rd, #imm16 — 16-bit immediate (imm4:imm12 split). */
static void enc_movw(int is_movt, u32 cond) {
	u32 rd = need_reg(1), v = imm(toks[2]) & 0xffff;
	emit32((cond << 28) | (is_movt ? 0x03400000u : 0x03000000u) | ((v >> 12) << 16) | (rd << 12) | (v & 0xfff));
}

/* multiply / divide / count-leading-zeros. */
static void enc_mul(u32 cond, int s) {   /* mul Rd, Rn, Rm  (Rd=19:16, Rm=11:8, Rn=3:0) */
	u32 rd = need_reg(1), rn = need_reg(2), rm = need_reg(3);
	emit32((cond << 28) | ((u32)s << 20) | (rd << 16) | (rm << 8) | 0x90 | rn);
}
static void enc_mla(u32 cond, int s) {   /* mla Rd, Rn, Rm, Ra */
	u32 rd = need_reg(1), rn = need_reg(2), rm = need_reg(3), ra = need_reg(4);
	emit32((cond << 28) | 0x00200000u | ((u32)s << 20) | (rd << 16) | (ra << 12) | (rm << 8) | 0x90 | rn);
}
static void enc_umull(u32 cond, u32 base) {   /* {u,s}mull/{u,s}mlal RdLo, RdHi, Rn, Rm (64-bit multiply) */
	u32 rdlo = need_reg(1), rdhi = need_reg(2), rn = need_reg(3), rm = need_reg(4);
	emit32((cond << 28) | base | (rdhi << 16) | (rdlo << 12) | (rm << 8) | rn);
}
static void enc_mls(u32 cond) {   /* mls Rd, Rn, Rm, Ra  (Rd = Ra - Rn*Rm) */
	u32 rd = need_reg(1), rn = need_reg(2), rm = need_reg(3), ra = need_reg(4);
	emit32((cond << 28) | 0x00600000u | (rd << 16) | (ra << 12) | (rm << 8) | 0x90 | rn);
}
static void enc_div(int is_sdiv, u32 cond) {   /* udiv/sdiv Rd, Rn, Rm  (Rn=3:0 dividend, Rm=11:8) */
	u32 rd = need_reg(1), rn = need_reg(2), rm = need_reg(3);
	emit32((cond << 28) | (is_sdiv ? 0x0710f010u : 0x0730f010u) | (rd << 16) | (rm << 8) | rn);
}
static void enc_clz(u32 cond) {   /* clz Rd, Rm */
	emit32((cond << 28) | 0x016f0f10u | (need_reg(1) << 12) | need_reg(2));
}

/* supervisor call + branch-and-link-exchange (register). */
static void enc_svc(u32 cond) {
	const char *t = toks[1]; emit32((cond << 28) | 0x0f000000u | ((u32)strtol(t[0] == '#' ? t + 1 : t, NULL, 0) & 0xffffff));
}
static void enc_blx(u32 cond) { emit32((cond << 28) | 0x012fff30u | need_reg(1)); }   /* blx Rm */

/* extra load/store: ldrd/strd/ldrh/strh Rd, [Rn] | [Rn, #±imm].  cond 000 P U 1 W L Rn Rd immhi 1SH1 immlo */
static void enc_xldst(u32 cond, int Lbit, u32 nib) {
	u32 rd = need_reg(1);
	if (ntok < 3 || toks[2][0] != '[') die("%s: expected [Rn ...]", toks[0]);
	char buf[64]; size_t bl = 0;
	for (int i = 2; i < ntok && bl < sizeof buf - 1; i++) bl += (size_t)snprintf(buf + bl, sizeof buf - bl, "%s%s", i > 2 ? " " : "", toks[i]);
	char *rb = strchr(buf, ']'); if (!rb) die("%s: missing ']'", toks[0]);
	*rb = 0;
	int rn = reg(strtok(buf + 1, " ")); if (rn < 0) die("%s: bad base register", toks[0]);
	u32 U = 1, off = 0; char *o = strtok(NULL, " ");
	if (o) { if (o[0] != '#') die("%s: only immediate offset supported here", toks[0]); long v = strtol(o + 1, NULL, 0); if (v < 0) { U = 0; v = -v; } off = (u32)v; }
	emit32((cond << 28) | (1u << 24) | (U << 23) | (1u << 22) | ((u32)Lbit << 20) | ((u32)rn << 16) | (rd << 12)
	     | (((off >> 4) & 0xf) << 8) | (nib << 4) | (off & 0xf));
}

/* ------------------------------------------------------------------ md hooks ---------------------- */
/* Parse an optional UAL suffix after a base mnemonic. Returns 0 on a bad suffix. */
static int suffix_sc(const char *suf, u32 *cond, int *s) { *cond = 14; *s = 0; if (*suf == 's') { *s = 1; suf++; } if (*suf && !lookup_cc(suf, cond)) return 0; return 1; }
static int suffix_c(const char *suf, u32 *cond) { *cond = 14; if (*suf && !lookup_cc(suf, cond)) return 0; return 1; }

void md_assemble(char **t, int n) {
	toks = t; ntok = n;
	const char *m = toks[0]; size_t L = strlen(m); u32 cond; int s;
	char b3[4] = { L > 0 ? m[0] : 0, L > 1 ? m[1] : 0, L > 2 ? m[2] : 0, 0 };   /* first 3 chars, for shifts */

	if (!strcmp(m, "push")) { enc_push(14); return; }
	if (!strcmp(m, "pop"))  { enc_pop(14);  return; }
	if (L == 6 && !strncmp(m, "push", 4) && lookup_cc(m + 4, &cond)) { enc_push(cond); return; }   /* pusheq, … */
	if (L == 5 && !strncmp(m, "pop",  3) && lookup_cc(m + 3, &cond)) { enc_pop(cond);  return; }   /* popeq, popcs, … */

	/* branches: b/bl/bx/blx with an optional condition. ("bic" = b+"ic" isn't a cond -> falls to DP.) */
	if (m[0] == 'b') {
		if (!strcmp(m, "b"))   { enc_branch(0, 14); return; }
		if (!strcmp(m, "bl"))  { enc_branch(1, 14); return; }
		if (!strcmp(m, "bx"))  { enc_bx(14);  return; }
		if (!strcmp(m, "blx")) { enc_blx(14); return; }
		if (L == 3 && lookup_cc(m + 1, &cond))                                    { enc_branch(0, cond); return; }
		if (L == 4 && m[1] == 'l' && lookup_cc(m + 2, &cond))                     { enc_branch(1, cond); return; }
		if (L == 4 && m[1] == 'x' && lookup_cc(m + 2, &cond))                     { enc_bx(cond);  return; }
		if (L == 5 && m[1] == 'l' && m[2] == 'x' && lookup_cc(m + 3, &cond))      { enc_blx(cond); return; }
	}

	/* single data transfer: ldr/str {b|d|h}{cond}. Whole-suffix cond FIRST so "ldrhi"=ldr+hi. */
	if (!strncmp(m, "ldr", 3) || !strncmp(m, "str", 3)) {
		int is_load = (m[0] == 'l'); const char *suf = m + 3; cond = 14;
		if (!*suf || lookup_cc(suf, &cond)) enc_ldst(cond, is_load, 0);            /* word */
		else if (suf[0] == 'b') { if (!suffix_c(suf + 1, &cond)) die("%s: bad suffix", m); enc_ldst(cond, is_load, 1); }
		else if (suf[0] == 'd') { if (!suffix_c(suf + 1, &cond)) die("%s: bad suffix", m); enc_xldst(cond, 0, is_load ? 0xd : 0xf); }
		else if (suf[0] == 'h') { if (!suffix_c(suf + 1, &cond)) die("%s: bad suffix", m); enc_xldst(cond, is_load ? 1 : 0, 0xb); }
		else if (is_load && suf[0] == 's' && suf[1] == 'b') { if (!suffix_c(suf + 2, &cond)) die("%s: bad suffix", m); enc_xldst(cond, 1, 0xd); }   /* ldrsb: L=1, SH=10 */
		else if (is_load && suf[0] == 's' && suf[1] == 'h') { if (!suffix_c(suf + 2, &cond)) die("%s: bad suffix", m); enc_xldst(cond, 1, 0xf); }   /* ldrsh: L=1, SH=11 */
		else die("%s: ldr/str variant not supported", m);
		return;
	}
	if (!strncmp(m, "ldm", 3) || !strncmp(m, "stm", 3)) {   /* load/store multiple, any addressing mode */
		const char *suf = m + 3; u32 P = 0, U = 1;          /* bare ldm/stm defaults to IA */
		if      (!strncmp(suf, "ia", 2)) { P = 0; U = 1; suf += 2; }
		else if (!strncmp(suf, "ib", 2)) { P = 1; U = 1; suf += 2; }
		else if (!strncmp(suf, "da", 2)) { P = 0; U = 0; suf += 2; }
		else if (!strncmp(suf, "db", 2)) { P = 1; U = 0; suf += 2; }
		if (!suffix_c(suf, &cond)) die("%s: unsupported ldm/stm mode", m);
		enc_ldstm(m[0] == 'l', cond, P, U); return;
	}
	if (!strncmp(m, "uxtb", 4)) { if (!suffix_c(m + 4, &cond)) die("%s: bad suffix", m); enc_extend(cond, 0x06ef0070u); return; }
	if (!strncmp(m, "uxth", 4)) { if (!suffix_c(m + 4, &cond)) die("%s: bad suffix", m); enc_extend(cond, 0x06ff0070u); return; }
	if (!strncmp(m, "sxtb", 4)) { if (!suffix_c(m + 4, &cond)) die("%s: bad suffix", m); enc_extend(cond, 0x06af0070u); return; }
	if (!strncmp(m, "sxth", 4)) { if (!suffix_c(m + 4, &cond)) die("%s: bad suffix", m); enc_extend(cond, 0x06bf0070u); return; }
	if (!strncmp(m, "movw", 4) || !strncmp(m, "movt", 4)) { if (!suffix_c(m + 4, &cond)) die("%s: bad suffix", m); enc_movw(m[3] == 't', cond); return; }
	if (!strncmp(m, "mul", 3)) { if (!suffix_sc(m + 3, &cond, &s)) die("%s: bad suffix", m); enc_mul(cond, s); return; }
	if (!strncmp(m, "mla", 3)) { if (!suffix_sc(m + 3, &cond, &s)) die("%s: bad suffix", m); enc_mla(cond, s); return; }
	if (!strncmp(m, "mls", 3)) { if (!suffix_c(m + 3, &cond)) die("%s: bad suffix", m); enc_mls(cond); return; }
	if (!strncmp(m, "umull", 5)) { if (!suffix_c(m + 5, &cond)) die("%s: bad suffix", m); enc_umull(cond, 0x00800090u); return; }
	if (!strncmp(m, "umlal", 5)) { if (!suffix_c(m + 5, &cond)) die("%s: bad suffix", m); enc_umull(cond, 0x00a00090u); return; }
	if (!strncmp(m, "smull", 5)) { if (!suffix_c(m + 5, &cond)) die("%s: bad suffix", m); enc_umull(cond, 0x00c00090u); return; }
	if (!strncmp(m, "smlal", 5)) { if (!suffix_c(m + 5, &cond)) die("%s: bad suffix", m); enc_umull(cond, 0x00e00090u); return; }
	if (!strncmp(m, "udiv", 4)) { if (!suffix_c(m + 4, &cond)) die("%s: bad suffix", m); enc_div(0, cond); return; }
	if (!strncmp(m, "sdiv", 4)) { if (!suffix_c(m + 4, &cond)) die("%s: bad suffix", m); enc_div(1, cond); return; }
	if (!strncmp(m, "clz", 3)) { if (!suffix_c(m + 3, &cond)) die("%s: bad suffix", m); enc_clz(cond); return; }
	if (!strncmp(m, "svc", 3)) { if (!suffix_c(m + 3, &cond)) die("%s: bad suffix", m); enc_svc(cond); return; }
	{ int st = shift_type(b3); if (st >= 0) { if (!suffix_sc(m + 3, &cond, &s)) die("%s: bad suffix", m); enc_shift(st, cond, s); return; } }

	/* data-processing: 3-char base (add/mov/cmp/…) + optional {s}{cond} (UAL order). */
	if (L >= 3)
		for (unsigned i = 0; i < sizeof dp_tab / sizeof *dp_tab; i++)
			if (!strncmp(m, dp_tab[i].name, 3)) {
				if (!suffix_sc(m + 3, &cond, &s)) die("%s: bad condition/suffix", m);
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

/* End of pass: resolve pc-relative ldr literals (target label now defined). offset12 = (label+addend) -
 * (ldr_addr + 8); the sign picks the U bit. Both the ldr and its pool are in the same section, so this
 * is an assembler-internal fixup (no relocation) — exactly what GNU as does for a local literal load. */
void md_finish(void) {
	for (int i = 0; i < nldrlit; i++) {
		int si = sym_find(ldrlit[i].sym);
		if (si < 0 || !syms[si].defined) die("ldr literal: undefined symbol '%s'", ldrlit[i].sym);
		if (syms[si].sec != ldrlit[i].sec) die("ldr literal '%s': target in a different section", ldrlit[i].sym);
		int32_t target = (int32_t)syms[si].value + (int32_t)ldrlit[i].addend;
		int32_t delta = target - (int32_t)(ldrlit[i].off + 8);
		u32 mag = (u32)(delta < 0 ? -delta : delta);
		if (mag > 0xfff) die("ldr literal '%s': offset %d out of +/-4095 range", ldrlit[i].sym, delta);
		u32 w = read32(ldrlit[i].sec, ldrlit[i].off);
		w = (w & ~((1u << 23) | 0xfffu)) | ((delta >= 0 ? 1u : 0u) << 23) | mag;   /* set U + offset12 */
		patch32(ldrlit[i].sec, ldrlit[i].off, w);
	}
	for (int i = 0; i < nbrfix; i++) {   /* named branches: resolve intra-section, else relocate */
		int si = sym_find(brfix[i].sym);
		u32 base = read32(brfix[i].sec, brfix[i].off);
		/* resolve ONLY a LOCAL same-section symbol; a GLOBAL one keeps a reloc even if defined here
		 * (it may be interposed/PLT-routed at link time), matching GNU as. */
		if (si >= 0 && syms[si].defined && !syms[si].global && syms[si].sec == brfix[i].sec) {
			int32_t rel = (int32_t)syms[si].value - (int32_t)(brfix[i].off + 8);
			patch32(brfix[i].sec, brfix[i].off, (base & 0xff000000u) | ((rel >> 2) & 0x00ffffffu));
		} else {
			patch32(brfix[i].sec, brfix[i].off, (base & 0xff000000u) | 0xfffffe);   /* addend -8, ARM REL */
			add_reloc(brfix[i].sec, brfix[i].off, sym_intern(brfix[i].sym), brfix[i].is_bl ? R_ARM_CALL : R_ARM_JUMP24);
		}
	}
}
