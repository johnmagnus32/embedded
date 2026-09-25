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
#include <strings.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include "as.h"

/* ELF identity this backend targets (read by the obj backend when it writes the header). */
const u16 md_e_machine = EM_ARM;
const u32 md_e_flags   = 0x05000000;  /* EF_ARM_EABI_VER5 */

/* ARM relocation type codes (arch-private; the front-end stores them opaquely, the obj backend writes them). */
#define R_ARM_ABS32  2    /* .word <symbol> — 32-bit absolute */
#define R_ARM_REL32  3    /* .word <sym> - . — 32-bit PC-relative (S + A - P) */
#define R_ARM_CALL   28   /* bl/blx to a symbol — imm24, addend held in-place */
#define R_ARM_JUMP24 29   /* b to a symbol */
#define R_ARM_GOT_PREL 96 /* .word <symbol>(GOT_PREL) — PC-relative offset to the symbol's GOT slot (PIC) */
#define R_ARM_MOVW_ABS_NC 43 /* movw Rd, #:lower16:sym — imm16 = (S+A) & 0xffff */
#define R_ARM_MOVT_ABS    44 /* movt Rd, #:upper16:sym — imm16 = ((S+A) >> 16) & 0xffff */
const u32 md_r_abs32    = R_ARM_ABS32;    /* the front-end uses this for `.word <symbol>`       */
const u32 md_r_rel32    = R_ARM_REL32;    /* ...and this for `.word <symbol> - .`                */

/* Pc-relative literal loads (`ldr Rd, .Llabel[+/-N]`) — the target pool label usually sits AFTER the
 * code, so we emit `ldr Rd, [pc,#0]` and fix the 12-bit offset in md_finish once all labels are known. */
static struct { int sec; u32 off; char sym[64]; long addend; int kind; } ldrlit[16384]; static int nldrlit;
static void join_toks(int from, char *out, size_t n); static void pool_ref(u32 insn, u32 val, int sym); static void pool_flush(int sec);   /* kind: 0 = ldr Rd,literal ; 1 = adr Rd,label */
/* Named-symbol branches (b/bl <sym>): deferred to md_finish so we can RESOLVE ones defined in the same
 * section (like GNU as does for local labels) and only RELOCATE truly external/cross-section ones. */
static struct { int sec; u32 off; char sym[64]; int is_bl; } brfix[65536]; static int nbrfix;

/* The current instruction's tokens (set by md_assemble; the enc_* helpers read them, like tc-arm.c). */
static char **toks; static int ntok;

/* ------------------------------------------------------------------ operand parsing --------------- */
/* Register names, case-insensitive (GAS): r0-r15, APCS a1-a4 v1-v8 sb sl fp ip sp lr pc, and `.req` aliases.
 * The whole token must be the name (was atoi: `r15x` was accepted as r15). -1 if not a register. */
static struct { char name[32]; int r; } reqs[256]; static int nreqs;
static int reg(const char *t) {
	if (!t) return -1;
	char n[32]; size_t L = strlen(t); if (L == 0 || L >= sizeof n) return -1;
	for (size_t i = 0; i <= L; i++) n[i] = (char)tolower((unsigned char)t[i]);
	static const struct { const char *n; int r; } nm[] = { {"sp",13},{"lr",14},{"pc",15},{"fp",11},{"ip",12},{"sl",10},{"sb",9},
		{"wr",7},{"a1",0},{"a2",1},{"a3",2},{"a4",3},{"v1",4},{"v2",5},{"v3",6},{"v4",7},{"v5",8},{"v6",9},{"v7",10},{"v8",11} };
	for (unsigned i = 0; i < sizeof nm / sizeof *nm; i++) if (!strcmp(n, nm[i].n)) return nm[i].r;
	if (n[0] == 'r' && isdigit((unsigned char)n[1])) { char *e; long v = strtol(n + 1, &e, 10); if (!*e && v >= 0 && v <= 15 && (n[1] != '0' || !n[2])) return (int)v; }
	for (int i = nreqs - 1; i >= 0; i--) if (!strcmp(reqs[i].name, n)) return reqs[i].r;
	return -1;
}
void md_req(const char *alias, const char *regname) {
	int r = reg(regname); if (r < 0) die(".req: '%s' is not a register", regname);
	if (nreqs >= 256) die("too many .req aliases");
	size_t L = strlen(alias); if (L >= sizeof reqs[0].name) die(".req: alias too long");
	for (size_t i = 0; i <= L; i++) reqs[nreqs].name[i] = (char)tolower((unsigned char)alias[i]);
	reqs[nreqs].r = r; nreqs++;
}
void md_unreq(const char *alias) {
	char n[32]; size_t L = strlen(alias); if (L >= sizeof n) die(".unreq: bad name");
	for (size_t i = 0; i <= L; i++) n[i] = (char)tolower((unsigned char)alias[i]);
	for (int i = nreqs - 1; i >= 0; i--) if (!strcmp(reqs[i].name, n)) { reqs[i] = reqs[--nreqs]; return; }
	die(".unreq: '%s' is not an alias", alias);
}
/* `sym(OP)` relocation operators in data words — GAS names -> ELF ARM relocation types. */
int md_reloc_operator(const char *op, u32 *type) {
	/* exactly GNU as 2.42's ARM data-relocation operators (checked one by one): note (PLT) on data is plain
	 * R_ARM_ABS32, and the IE/LE TLS spellings are GOTTPOFF/TPOFF (was: TLSIE/TLSLE, which GAS rejects; PLT -> PLT32) */
	static const struct { const char *n; u32 t; } ops[] = { {"GOT",26},{"GOTOFF",24},{"GOT_PREL",96},{"TARGET1",38},{"TARGET2",41},
		{"SBREL",9},{"PLT",2},{"TLSGD",104},{"TLSLDM",105},{"TLSLDO",106},{"GOTTPOFF",107},{"TPOFF",108},{"TLSDESC",90},{"TLSCALL",91} };
	for (unsigned i = 0; i < sizeof ops / sizeof *ops; i++) if (!strcmp(op, ops[i].n)) { *type = ops[i].t; return 1; }
	return 0;
}
u32 md_data_reloc_for(const char *sym, u32 dflt) { return !strcmp(sym, "_GLOBAL_OFFSET_TABLE_") ? 25 /* R_ARM_GOTPC */ : dflt; }
static u32 imm(const char *t) {   /* #<num> immediate (dec / 0x hex / negative) */
	if (!t || t[0] != '#') die("expected #immediate, got '%s'", t ? t : "(nil)");
	return (u32)eval_const_expr(t + 1);   /* strict: `#(. - bar - 8)`, `#N` (.equ), `#-4`; junk is an error */
}
/* ARM modified-immediate: encode v as (rot<<8)|imm8 where v == ror(imm8, 2*rot). Recover imm8 for a
 * candidate rot as rol(v, 2*rot); the smallest rot whose imm8 fits in 8 bits wins (matches GNU as). */
static int modimm_try(u32 v, u32 *out) {
	for (int rot = 0; rot < 16; rot++) {
		u32 s = (2 * rot) & 31;
		u32 imm8 = (v << s) | (v >> ((32 - s) & 31));   /* rol(v, 2*rot); &31 avoids UB shift at rot 0 */
		if (imm8 <= 0xff) { *out = (rot << 8) | imm8; return 1; }
	}
	return 0;
}
static u32 modimm(u32 v) {
	u32 o; if (modimm_try(v, &o)) return o;
	die("immediate #%u not encodable as an ARM modified-immediate", v); return 0;
}

/* ------------------------------------------------------------------ instruction encoders ---------- */
/* A register operand where r15 is UNPREDICTABLE (multiplies, DSP, media, saturate, clz, exclusives, ...): GAS
 * rejects it, so do we (was: silently encoded). */
static u32 need_reg_nopc(int i);
static u32 need_reg(int i) {   /* operand i must be a register */
	int r = (i < ntok) ? reg(toks[i]) : -1;
	if (r < 0) die("%s: expected a register at operand %d", toks[0], i);
	return (u32)r;
}
static u32 need_reg_nopc(int i) { u32 r = need_reg(i); if (r == 15) die("%s: r15 not allowed here", toks[0]); return r; }

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
	if (!strcasecmp(s, "lsl") || !strcasecmp(s, "asl")) return 0;   /* asl = legacy synonym for lsl */
	if (!strcasecmp(s, "lsr")) return 1;
	if (!strcasecmp(s, "asr")) return 2;
	if (!strcasecmp(s, "ror")) return 3;
	return -1;
}
/* A strict numeric operand: optional '#', a constant expression, range-checked (was strtol: junk ignored,
 * out-of-range values silently masked — e.g. `[r1, #5000]` encoded offset 904). */
static long snum(const char *t, long lo, long hi, const char *what) {
	if (!t) die("%s: missing %s", toks[0], what);
	const char *q = t; if (*q == '#') q++;
	long v = eval_const_expr(q);
	if (v < lo || v > hi) die("%s: %s %ld out of range [%ld, %ld]", toks[0], what, v, lo, hi);
	return v;
}
/* Immediate shift field (bits 11:5) for type st: lsl #0-31, lsr/asr #1-32 (32 encodes as 0), ror #1-31. */
static u32 shift_imm_field(int st, const char *amt) {
	long n = snum(amt, st == 0 ? 0 : 1, (st == 1 || st == 2) ? 32 : 31, "shift amount");
	return (((u32)n & 31) << 7) | ((u32)st << 5);
}
/* Build the 12-bit shifted-register operand2 for Rm with a shift at tokens [si]=type [si+1]=amount:
 * "Rm, lsl #n" -> (n<<7)|(type<<5)|Rm ; "Rm, lsl Rs" -> (Rs<<8)|(type<<5)|(1<<4)|Rm. */
static u32 shifted_reg(u32 rm, int si) {
	if (!strcasecmp(toks[si], "rrx")) { if (si + 1 < ntok) die("%s: rrx takes no amount", toks[0]); return (3u << 5) | rm; }   /* rrx = ror #0 */
	int st = shift_type(toks[si]); if (st < 0) die("%s: bad shift '%s'", toks[0], toks[si]);
	const char *amt = (si + 1 < ntok) ? toks[si + 1] : NULL; if (!amt) die("%s: shift needs an amount", toks[0]);
	if (amt[0] == '#') return shift_imm_field(st, amt) | rm;
	int rs = reg(amt); if (rs < 0) die("%s: bad shift amount '%s'", toks[0], amt);
	return ((u32)rs << 8) | ((u32)st << 5) | (1u << 4) | rm;
}

/* `#imm8, rot` — an explicit-rotation immediate (value = imm8 ror rot; rot even, 0..30). GAS accepts it and
 * encodes it verbatim (was: the rotation operand silently dropped). */
static int explicit_rot(int opidx) { return opidx + 1 < ntok && toks[opidx + 1][0] != '#' && isdigit((unsigned char)toks[opidx + 1][0]); }
static u32 explicit_rot_enc(int opidx) {
	u32 v = imm(toks[opidx]); long rot = snum(toks[opidx + 1], 0, 30, "rotation");
	if (v > 0xff || rot < 0 || rot > 30 || (rot & 1)) die("%s: bad explicit-rotation immediate '%s, %s'", toks[0], toks[opidx], toks[opidx + 1]);
	return ((u32)rot / 2) << 8 | v;
}
/* operand2 at token index opidx: "#imm" -> I=1 + modimm; register [,shift] -> I=0 + shifted-reg. */
static u32 operand2(int opidx, u32 *I) {
	if (toks[opidx][0] == '#') { *I = 1; return explicit_rot(opidx) ? explicit_rot_enc(opidx) : modimm(imm(toks[opidx])); }
	int rm = reg(toks[opidx]); if (rm < 0) die("%s: bad operand2 '%s'", toks[0], toks[opidx]);
	*I = 0;
	return (ntok > opidx + 1) ? shifted_reg((u32)rm, opidx + 1) : (u32)rm;
}

static void enc_dp(u32 opc, int form, u32 cond, int s) {
	u32 rd = 0, rn = 0, I; int opidx;
	if (form == DP_MOV)      { rd = need_reg(1);                    opidx = 2; }        /* mov/mvn Rd, op2 */
	else if (form == DP_CMP) { rn = need_reg(1); s = 1;            opidx = 2; }        /* cmp/… Rn, op2 (S forced) */
	else if (ntok == 3 || (ntok >= 4 && reg(toks[2]) >= 0 && (shift_type(toks[3]) >= 0 || !strcasecmp(toks[3], "rrx"))))
	                         { rd = need_reg(1); rn = rd; opidx = 2; }                   /* add Rd, op2  ==  add Rd, Rd, op2 (GAS) */
	else                     { rd = need_reg(1); rn = need_reg(2); opidx = 3; }        /* add/… Rd, Rn, op2 */
	u32 op2;
	if (form == DP_MOV && opidx < ntok && toks[opidx][0] == '#' && explicit_rot(opidx)) { I = 1; op2 = explicit_rot_enc(opidx); }
	else if (form == DP_MOV && opidx < ntok && toks[opidx][0] == '#') {   /* mov/mvn #imm: if not encodable, use the complement (mov<->mvn, e.g. `mov rd,#-14` -> `mvn rd,#13`) */
		u32 v = imm(toks[opidx]), enc; I = 1;
		if (modimm_try(v, &enc)) op2 = enc;
		else if (modimm_try(~v, &enc)) { op2 = enc; opc ^= 2; }        /* mov(13) <-> mvn(15) differ by bit 1 */
		else die("immediate #%u not encodable (mov/mvn)", v);
	} else op2 = operand2(opidx, &I);
	emit32((cond << 28) | (I << 25) | (opc << 21) | ((u32)s << 20) | (rn << 16) | (rd << 12) | op2);
}
static void enc_branch(int is_bl, u32 cond) {   /* b/bl{cond} <label> */
	if (ntok < 2) die("%s: missing target", toks[0]);
	u32 base = (cond << 28) | 0x0a000000u | ((u32)is_bl << 24);   /* cond 101 L imm24 */
	u32 off = here(); emit32(base);        /* placeholder; patched below or by md_apply_fix */
	char tgt[256]; join_toks(1, tgt, sizeof tgt);
	size_t tl = strlen(tgt);
	if (tl > 5 && !strcmp(tgt + tl - 5, "(PLT)")) tgt[tl -= 5] = 0;   /* `bl foo(PLT)`: GAS emits the same CALL/JUMP24 */
	for (size_t k = 0; k < tl; k++) if (!(isalnum((unsigned char)tgt[k]) || tgt[k] == '_' || tgt[k] == '.' || tgt[k] == '$'))
		die("%s: unsupported branch target '%s' (was: taken verbatim as a symbol name)", toks[0], tgt);
	const char *name = tgt;
	int n; char ldir;
	char fbn[64];
	if (parse_local_ref(name, &n, &ldir) == (int)strlen(name)) {   /* 1b / 1f: the fb label's hidden symbol */
		strncpy(fbn, syms[fb_symbol(n, ldir)].name, sizeof fbn - 1); fbn[sizeof fbn - 1] = 0; name = fbn;
	}
	/* named symbol: defer to md_finish — resolve if defined in THIS section (local label / same-file),
	 * else relocate. The placeholder keeps the cond/101/L opcode byte; imm24 is filled in later. */
	if (nbrfix >= 65536) die("too many branch fixups");
	brfix[nbrfix].sec = cursec; brfix[nbrfix].off = off; brfix[nbrfix].is_bl = is_bl;
	strncpy(brfix[nbrfix].sym, name, sizeof brfix[0].sym - 1); brfix[nbrfix].sym[sizeof brfix[0].sym - 1] = 0;
	sym_intern(name);   /* create it NOW (GAS symbol-table order = first reference), resolve in md_finish */
	nbrfix++;
}

static void enc_bx(u32 cond) {   /* bx{cond} Rm — branch-and-exchange (interworking return) */
	emit32((cond << 28) | 0x012fff10u | need_reg(1));
}

/* A load/store address: [Rn], [Rn, #±e], [Rn, ±Rm{, shift}] {!}, or post-indexed [Rn], #±e | ±Rm{, shift}.
 * Offsets are constant expressions (`[pc, #(bar - . - 8)]` was silently 0), `#-0` keeps U=0 (GAS does), and
 * the whole text is parsed — nothing trailing is ignored. One parser for ldr/str and the extra load/stores. */
typedef struct { int rn, P, U, W, isreg, rm; long imm; u32 shift; } Addr;
static Addr parse_addr(int first) {
	char buf[512]; size_t bl = 0;
	for (int i = first; i < ntok; i++) {
		int n = snprintf(buf + bl, sizeof buf - bl, "%s%s", i > first ? " " : "", toks[i]);
		if (n < 0 || (size_t)n >= sizeof buf - bl) die("%s: address operand too long", toks[0]);
		bl += (size_t)n;
	}
	if (buf[0] != '[') die("%s: expected [Rn ...] address, got '%s'", toks[0], buf);
	Addr a = { 0, 1, 1, 0, 0, 0, 0, 0 };
	char *rb = strchr(buf, ']'); if (!rb) die("%s: missing ']' in address", toks[0]);
	*rb = 0; char *after = rb + 1; while (*after == ' ') after++;
	if (*after == '!') { a.W = 1; after++; while (*after == ' ') after++; }
	char *in = buf + 1; while (*in == ' ') in++;
	char *sp = in; while (*sp && *sp != ' ') sp++;
	char save = *sp; *sp = 0; a.rn = reg(in); *sp = save;
	if (a.rn < 0) die("%s: bad base register in '%s'", toks[0], buf + 1);
	char *spec = sp; while (*spec == ' ') spec++;
	if (*after) { if (*spec) die("%s: offset both inside and after ']'", toks[0]); if (a.W) die("%s: '!' with post-index", toks[0]); a.P = 0; spec = after; }
	if (!*spec) return a;
	if (*spec == '#') {
		const char *q = spec + 1; while (*q == ' ') q++;
		long v = eval_const_expr(q);
		if (v < 0 || (v == 0 && *q == '-')) { a.U = 0; v = -v; }
		a.imm = v; return a;
	}
	if (*spec == '-') { a.U = 0; spec++; } else if (*spec == '+') spec++;
	char *e = spec; while (*e && *e != ' ') e++;
	save = *e; *e = 0; a.rm = reg(spec); *e = save;
	if (a.rm < 0) die("%s: bad offset '%s'", toks[0], spec);
	a.isreg = 1;
	char *sh = e; while (*sh == ' ') sh++;
	if (*sh) {
		if (!strcasecmp(sh, "rrx")) { a.shift = 3u << 5; return a; }
		char *amt = sh; while (*amt && *amt != ' ') amt++;
		if (!*amt) die("%s: shift '%s' needs an amount", toks[0], sh);
		*amt++ = 0; while (*amt == ' ') amt++;
		int st = shift_type(sh); if (st < 0) die("%s: bad index shift '%s'", toks[0], sh);
		if (amt[0] != '#') die("%s: index shift needs #amount", toks[0]);
		a.shift = shift_imm_field(st, amt);
	}
	return a;
}
/* PC in an address (GAS ldr-bad): writeback / post-index with a pc base, pc as the index register, and a load INTO
 * pc from a pc-relative address whose offset isn't word-aligned are all errors (were silently encoded). */
static void check_pc_addr(Addr a, u32 rd) {
	if (a.rn == 15 && (a.W || !a.P)) die("%s: writeback/post-index with a pc base is not allowed", toks[0]);
	if (a.isreg && a.rm == 15) die("%s: pc not allowed as the index register", toks[0]);
	if (rd == 15 && a.rn == 15 && !a.isreg && (a.imm & 3)) die("%s: ldr to register 15 must be 4-byte aligned", toks[0]);
}
/* ---- single data transfer: ldr/str{b}{cond} Rd, <addr> --------------------------------------------
 * Encoding: cond 01 I P U B W L Rn Rd offset(12). NOTE I is INVERTED vs data-processing: I=0 => the
 * offset is a 12-bit IMMEDIATE (U = sign), I=1 => a register (optionally lsl #n). Addressing:
 *   [Rn]            P=1 W=0 off=0        [Rn,#imm]     P=1 W=0        [Rn,#imm]!  P=1 W=1  (pre, writeback)
 *   [Rn],#imm       P=0 W=0 (post)       [Rn,Rm]       P=1 I=1        [Rn,Rm,lsl #n]  P=1 I=1 shift
 * B = byte (ldrb/strb), L = load (ldr). */
static void enc_ldst(u32 cond, int is_load, int is_byte) {
	u32 rd = need_reg(1);
	if (ntok >= 3 && toks[2][0] == '=') {   /* ldr Rd, =expr: mov/mvn if it fits, else a literal-pool word */
		if (is_byte || !is_load) die("%s: `=` literal form is for word ldr only", toks[0]);
		char ex[512]; join_toks(2, ex, sizeof ex);
		long c; int sy, dt; eval_reloc_expr(ex + 1, &c, &sy, &dt);
		if (dt) die("%s: `=` literal can't be PC-relative", toks[0]);
		u32 enc;
		if (sy < 0 && modimm_try((u32)c, &enc)) { emit32((cond << 28) | 0x03a00000u | (rd << 12) | enc); return; }    /* mov */
		if (sy < 0 && modimm_try(~(u32)c, &enc)) { emit32((cond << 28) | 0x03e00000u | (rd << 12) | enc); return; }   /* mvn */
		pool_ref((cond << 28) | 0x059f0000u | (rd << 12), (u32)c, sy);
		return;
	}
	if (ntok >= 3 && toks[2][0] != '[') {   /* pc-relative load: ldr Rd, <expr> (label, label+N, ., ...) */
		if (is_byte || !is_load) die("%s: literal form supported for word ldr only", toks[0]);
		if (nldrlit >= 16384) die("too many ldr literals");
		char ex[512]; join_toks(2, ex, sizeof ex);   /* whole operand (was toks[2] only: `ldr r0, l + 4` lost the +4) */
		long c; int sy, dt; eval_reloc_expr(ex, &c, &sy, &dt);
		u32 off = here();
		if (sy < 0 && dt == 1) {   /* `.`-relative: target = here + c, same section, resolve now */
			int32_t delta = (int32_t)c - 8; u32 mag = (u32)(delta < 0 ? -delta : delta);
			if (rd == 15 && (mag & 3)) die("%s: ldr to register 15 must be 4-byte aligned", toks[0]);
			if (mag > 0xfff) die("%s: offset %d out of range", toks[0], delta);
			emit32((cond << 28) | 0x051f0000u | ((delta >= 0 ? 1u : 0u) << 23) | (rd << 12) | mag); return;
		}
		if (sy < 0 || dt) die("%s: bad pc-relative operand '%s'", toks[0], ex);
		emit32((cond << 28) | 0x059f0000u | (rd << 12));   /* ldr Rd, [pc, #0] placeholder */
		ldrlit[nldrlit].sec = cursec; ldrlit[nldrlit].off = off;
		strncpy(ldrlit[nldrlit].sym, syms[sy].name, sizeof ldrlit[0].sym - 1); ldrlit[nldrlit].sym[sizeof ldrlit[0].sym - 1] = 0;
		ldrlit[nldrlit].addend = c; ldrlit[nldrlit].kind = 0;
		nldrlit++;
		return;
	}
	Addr a = parse_addr(2);
	if (!a.isreg && a.imm > 0xfff) die("%s: offset %ld out of range (12-bit)", toks[0], a.imm);
	check_pc_addr(a, rd);
	u32 off = a.isreg ? ((u32)a.rm | a.shift) : (u32)a.imm;
	emit32((cond << 28) | (1u << 26) | ((u32)a.isreg << 25) | ((u32)a.P << 24) | ((u32)a.U << 23) | ((u32)is_byte << 22)
	     | ((u32)a.W << 21) | ((u32)is_load << 20) | ((u32)a.rn << 16) | (rd << 12) | off);
}


/* Patch an `add Rd,pc,#0` placeholder (at sec:off) into add/sub Rd,pc,#modimm(delta). */
static void patch_adr(int sec, u32 off, int32_t delta) {
	u32 w = read32(sec, off), cond = (w >> 28) & 0xf, rd = (w >> 12) & 0xf;
	u32 mag = (u32)(delta < 0 ? -delta : delta);
	patch32(sec, off, (cond << 28) | (delta >= 0 ? 0x028f0000u : 0x024f0000u) | (rd << 12) | modimm(mag));
}
static void enc_adr(u32 cond) {   /* adr Rd, label -> add/sub Rd, pc, #(label-.-8) */
	u32 rd = need_reg(1);
	u32 off = here();
	emit32((cond << 28) | 0x028f0000u | (rd << 12));   /* add Rd, pc, #0 placeholder */
	char ex[512]; join_toks(2, ex, sizeof ex);   /* whole operand (was toks[2] only: `adr r0, l + 4` lost the +4) */
	long c; int sy, dt; eval_reloc_expr(ex, &c, &sy, &dt);
	if (sy < 0 && dt == 1) { patch_adr(cursec, off, (int32_t)c - 8); return; }   /* `.`-relative */
	if (sy < 0 || dt) die("adr: bad operand '%s'", ex);
	if (nldrlit >= 16384) die("too many pc-relative fixups");   /* symbol (incl. 1f/1b): resolve in md_finish */
	ldrlit[nldrlit].sec = cursec; ldrlit[nldrlit].off = off;
	strncpy(ldrlit[nldrlit].sym, syms[sy].name, sizeof ldrlit[0].sym - 1); ldrlit[nldrlit].sym[sizeof ldrlit[0].sym - 1] = 0;
	ldrlit[nldrlit].addend = c;
	ldrlit[nldrlit].kind = 1; nldrlit++;
}
/* `, ror #0|8|16|24` on an extend: bits 11:10 (was: silently dropped -> sxtb r0, r1, ror #8 encoded ror #0) */
static u32 ror_field(int i) {
	if (i >= ntok) return 0;
	if (strcasecmp(toks[i], "ror") || i + 1 >= ntok) die("%s: expected `ror #0|8|16|24`, got '%s'", toks[0], toks[i]);
	long r = snum(toks[i + 1], 0, 24, "rotation"); if (r & 7) die("%s: rotation must be 0, 8, 16 or 24", toks[0]);
	if (i + 2 < ntok) die("%s: too many operands", toks[0]);
	return (u32)(r / 8) << 10;
}
static void enc_extend(u32 cond, u32 base) {   /* {u,s}xt{b,h,b16}{cond} Rd, Rm{, ror #n}; rev/rbit Rd, Rm */
	u32 rd = need_reg_nopc(1), rm = need_reg_nopc(2);
	u32 rot = ((base & 0x0ff000f0u) == 0x06a00070u || (base & 0x0f8000f0u) == 0x06800070u) ? ror_field(3) : (ntok > 3 ? (die("%s: too many operands", toks[0]), 0) : 0);
	emit32((cond << 28) | base | (rd << 12) | rot | rm);
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
/* push/pop = STMDB/LDMIA sp!, {list}; a SINGLE register is `str Rt, [sp, #-4]!` / `ldr Rt, [sp], #4` (as GAS) */
static int single_reg(u32 l) { return l && !(l & (l - 1)) ? __builtin_ctz(l) : -1; }
static void enc_push(u32 cond) { u32 l = reglist_at(1); int r = single_reg(l);
	emit32(r >= 0 ? (cond << 28) | 0x052d0004u | ((u32)r << 12) : (cond << 28) | 0x092d0000u | l); }
static void enc_pop(u32 cond)  { u32 l = reglist_at(1); int r = single_reg(l);
	emit32(r >= 0 ? (cond << 28) | 0x049d0004u | ((u32)r << 12) : (cond << 28) | 0x08bd0000u | l); }

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
	if (ntok != 4) die("%s: expected Rd, Rm, #n|Rs (was: extra operands silently ignored)", toks[0]);
	u32 op2;
	if (amt[0] == '#') op2 = shift_imm_field(st, amt) | rm;
	else { int rs = reg(amt); if (rs < 0) die("%s: bad shift amount", toks[0]); op2 = ((u32)rs << 8) | ((u32)st << 5) | (1u << 4) | rm; }
	emit32((cond << 28) | (13u << 21) | ((u32)s << 20) | (rd << 12) | op2);   /* MOV */
}

/* movw/movt Rd, #imm16 — 16-bit immediate (imm4:imm12 split). */
static void enc_movw(int is_movt, u32 cond) {
	u32 rd = need_reg(1);
	if (toks[2][0] == '#' && toks[2][1] == ':') {   /* #:lower16:sym / #:upper16:sym -> a MOVW/MOVT_ABS relocation */
		int lower = !strncmp(toks[2], "#:lower16:", 10), upper = !strncmp(toks[2], "#:upper16:", 10);
		if (!lower && !upper) die("movw/movt: bad relocation operand '%s'", toks[2]);
		char nm[128]; strncpy(nm, toks[2] + 10, sizeof nm - 1); nm[sizeof nm - 1] = 0;
		u32 off = here();
		emit32((cond << 28) | (is_movt ? 0x03400000u : 0x03000000u) | (rd << 12));   /* imm16 = 0 placeholder */
		add_reloc(cursec, off, sym_intern(nm), lower ? R_ARM_MOVW_ABS_NC : R_ARM_MOVT_ABS);
		return;
	}
	u32 v = imm(toks[2]) & 0xffff;
	emit32((cond << 28) | (is_movt ? 0x03400000u : 0x03000000u) | ((v >> 12) << 16) | (rd << 12) | (v & 0xfff));
}

/* multiply / divide / count-leading-zeros. */
static void enc_mul(u32 cond, int s) {   /* mul Rd, Rn, Rm  (Rd=19:16, Rm=11:8, Rn=3:0) */
	u32 rd = need_reg_nopc(1), rn = need_reg_nopc(2), rm = need_reg_nopc(3);
	emit32((cond << 28) | ((u32)s << 20) | (rd << 16) | (rm << 8) | 0x90 | rn);
}
static void enc_mla(u32 cond, int s) {   /* mla Rd, Rn, Rm, Ra */
	u32 rd = need_reg_nopc(1), rn = need_reg_nopc(2), rm = need_reg_nopc(3), ra = need_reg_nopc(4);
	emit32((cond << 28) | 0x00200000u | ((u32)s << 20) | (rd << 16) | (ra << 12) | (rm << 8) | 0x90 | rn);
}
static void enc_umull_s(u32 cond, u32 base, int s) {
	base |= (u32)s << 20;   /* {u,s}mull/{u,s}mlal RdLo, RdHi, Rn, Rm (64-bit multiply) */
	u32 rdlo = need_reg_nopc(1), rdhi = need_reg_nopc(2), rn = need_reg_nopc(3), rm = need_reg_nopc(4);
	emit32((cond << 28) | base | (rdhi << 16) | (rdlo << 12) | (rm << 8) | rn);
}
static void enc_mls(u32 cond) {   /* mls Rd, Rn, Rm, Ra  (Rd = Ra - Rn*Rm) */
	u32 rd = need_reg_nopc(1), rn = need_reg_nopc(2), rm = need_reg_nopc(3), ra = need_reg_nopc(4);
	emit32((cond << 28) | 0x00600000u | (rd << 16) | (ra << 12) | (rm << 8) | 0x90 | rn);
}
static void enc_div(int is_sdiv, u32 cond) {   /* udiv/sdiv Rd, Rn, Rm  (Rn=3:0 dividend, Rm=11:8) */
	u32 rd = need_reg_nopc(1), rn = need_reg_nopc(2), rm = need_reg_nopc(3);
	emit32((cond << 28) | (is_sdiv ? 0x0710f010u : 0x0730f010u) | (rd << 16) | (rm << 8) | rn);
}
static void enc_clz(u32 cond) {   /* clz Rd, Rm */
	emit32((cond << 28) | 0x016f0f10u | (need_reg_nopc(1) << 12) | need_reg_nopc(2));
}

/* supervisor call + branch-and-link-exchange (register). */
static void enc_svc(u32 cond) {
	emit32((cond << 28) | 0x0f000000u | (u32)snum(toks[1], 0, 0xffffff, "svc number"));
}
static void enc_blx(u32 cond) { emit32((cond << 28) | 0x012fff30u | need_reg(1)); }   /* blx Rm */
static void enc_barrier(u32 base) {   /* dmb/dsb/isb {option} — memory/instruction barriers (unconditional) */
	u32 opt = 15;   /* default 'sy' (full system) */
	if (ntok >= 2) {
		char ob[16]; size_t ol = strlen(toks[1]); if (ol >= sizeof ob) die("%s: bad option", toks[0]);
		for (size_t i = 0; i <= ol; i++) ob[i] = (char)tolower((unsigned char)toks[1][i]);
		const char *o = ob;
		if      (!strcasecmp(o, "sy"))    opt = 15; else if (!strcasecmp(o, "st"))    opt = 14;
		else if (!strcasecmp(o, "ish") || !strcasecmp(o, "sh")) opt = 11; else if (!strcasecmp(o, "ishst") || !strcasecmp(o, "shst")) opt = 10;
		else if (!strcasecmp(o, "ishld")) opt = 9; else if (!strcasecmp(o, "ld")) opt = 13; else if (!strcasecmp(o, "nshld")) opt = 5; else if (!strcasecmp(o, "oshld")) opt = 1;
		else if (!strcasecmp(o, "un"))    opt = 7;  else if (!strcasecmp(o, "unst")) opt = 6;   /* legacy aliases */
		else if (!strcasecmp(o, "nsh"))   opt = 7;  else if (!strcasecmp(o, "nshst")) opt = 6;
		else if (!strcasecmp(o, "osh"))   opt = 3;  else if (!strcasecmp(o, "oshst")) opt = 2;
		else opt = (u32)snum(o, 0, 15, "barrier option");
	}
	emit32(base | opt);
}
static void enc_mrs(u32 cond) {   /* mrs Rd, (c|s)psr */
	u32 rd = need_reg(1);
	/* source must be exactly cpsr/apsr/spsr (was: only the first letter checked -> `mrs r0, iapsr` accepted) */
	if (ntok != 3) die("mrs: expected Rd, cpsr|apsr|spsr");
	u32 R;
	if (!strcasecmp(toks[2], "cpsr") || !strcasecmp(toks[2], "apsr")) R = 0;
	else if (!strcasecmp(toks[2], "spsr")) R = 1u << 22;
	else die("mrs: unsupported source '%s' (banked registers not implemented)", toks[2]);
	emit32((cond << 28) | 0x010f0000u | R | (rd << 12));
}
static void enc_msr(u32 cond) {   /* msr (c|s)psr_<fields>, Rm | #imm  (fields: c=1 x=2 s=4 f=8) */
	/* GAS: bare cpsr/spsr = _fc (was: all four fields); legacy _all=fc _flg=f _ctl=c; APSR_nzcvq=f _g=s
	 * _nzcvqg=fs; field letters c/x/s/f in any order/case. Anything else is an error (was silently ignored). */
	char nm[32]; size_t L = strlen(toks[1]); if (L >= sizeof nm) die("msr: bad operand '%s'", toks[1]);
	for (size_t k = 0; k <= L; k++) nm[k] = (char)tolower((unsigned char)toks[1][k]);
	char *u = strchr(nm, '_'); if (u) *u++ = 0;
	u32 R, mask = 0;
	if (!strcmp(nm, "cpsr") || !strcmp(nm, "apsr")) R = 0; else if (!strcmp(nm, "spsr")) R = 1u << 22;
	else die("msr: unsupported register '%s' (banked registers not implemented)", toks[1]);
	if (!u) mask = 9;
	else if (!strcmp(u, "all")) mask = 9;
	else if (!strcmp(u, "flg")) mask = 8;
	else if (!strcmp(u, "ctl")) mask = 1;
	else if (!strcmp(nm, "apsr") && !strcmp(u, "nzcvq")) mask = 8;
	else if (!strcmp(nm, "apsr") && !strcmp(u, "g")) mask = 4;
	else if (!strcmp(nm, "apsr") && !strcmp(u, "nzcvqg")) mask = 12;
	else for (const char *c = u; *c; c++) {
		u32 bit = *c == 'c' ? 1 : *c == 'x' ? 2 : *c == 's' ? 4 : *c == 'f' ? 8 : 0;
		if (!bit || (mask & bit)) die("msr: bad field specifier '%s'", toks[1]);
		mask |= bit;
	}
	if (toks[2][0] == '#') emit32((cond << 28) | 0x0320f000u | R | (mask << 16) | modimm(imm(toks[2])));
	else                   emit32((cond << 28) | 0x0120f000u | R | (mask << 16) | need_reg(2));
}
static void enc_cps(u32 base) {   /* cpsid/cpsie <aif>[, #mode] | cps #mode (a=0x100 i=0x80 f=0x40; M=1<<17 + mode) */
	u32 f = 0; int ai = 1;
	if (base != 0xf1000000u) {
		if (ntok < 2) die("%s: needs interrupt flags", toks[0]);
		for (const char *c = toks[1]; *c; c++) {
			u32 bit = *c == 'a' ? 0x100 : *c == 'i' ? 0x80 : *c == 'f' ? 0x40 : 0;
			if (!bit || (f & bit)) die("%s: bad interrupt flags '%s'", toks[0], toks[1]);
			f |= bit;
		}
		ai = 2;
	}
	if (ai < ntok) f |= (1u << 17) | (u32)snum(toks[ai], 0, 31, "mode");
	else if (base == 0xf1000000u) die("cps: needs #mode");
	if (ai + 1 < ntok) die("%s: too many operands", toks[0]);
	emit32(base | f);
}
static long numop(const char *t) { return snum(t, 0, 7, "coprocessor opcode"); }
static u32 cpreg(const char *t, char pfx, const char *what) {   /* p<n> (or bare n) / c<n> or cr<n>, 0-15, strictly */
	char *e; if (!t) die("%s: missing %s", toks[0], what);
	const char *q = t;
	if (tolower((unsigned char)q[0]) == pfx) { q++; if (pfx == 'c' && tolower((unsigned char)q[0]) == 'r') q++; }
	else if (!(pfx == 'p' && isdigit((unsigned char)q[0]))) die("%s: expected %s (%c<n>), got '%s'", toks[0], what, pfx, t);
	long v = strtol(q, &e, 10); if (e == q) die("%s: bad %s '%s'", toks[0], what, t);
	if (*e || v < 0 || v > 15) die("%s: bad %s '%s'", toks[0], what, t);
	return (u32)v;
}
static void enc_mcr(u32 cond, u32 L) {   /* mcr/mrc p<cp>, <opc1>, Rt, c<CRn>, c<CRm>{, <opc2>} */
	if (ntok < 6) die("%s: expected p<cp>, <opc1>, Rt, c<n>, c<m>[, <opc2>]", toks[0]);
	u32 cp = cpreg(toks[1], 'p', "coprocessor"), opc1 = (u32)numop(toks[2]), rt = need_reg(3);
	u32 crn = cpreg(toks[4], 'c', "CRn"), crm = cpreg(toks[5], 'c', "CRm");
	u32 opc2 = (ntok >= 7) ? (u32)numop(toks[6]) : 0;
	emit32((cond << 28) | 0x0e000010u | L | (opc1 << 21) | (crn << 16) | (rt << 12) | (cp << 8) | (opc2 << 5) | crm);
}

/* extra load/store: ldrd/strd/ldrh/strh Rd, [Rn] | [Rn, #±imm].  cond 000 P U 1 W L Rn Rd immhi 1SH1 immlo */
static void enc_xldst(u32 cond, int Lbit, u32 nib) {
	u32 rd = need_reg(1);
	int ai = 2;
	/* ldrd/strd: `Rt, Rt2, [..]` (Rt2 must be Rt+1) or the legacy `Rt, [..]` */
	if (Lbit == 0 && (nib == 0xd || nib == 0xf) && ai < ntok && toks[ai][0] != '[') {   /* ldrd/strd only (ldrsb shares nib 0xd, L=1) */
		int r2 = reg(toks[ai]); if (r2 != (int)rd + 1) die("%s: second register must be r%u", toks[0], rd + 1); ai++; }
	Addr a = parse_addr(ai);
	if (a.isreg && a.shift) die("%s: no shifted index for halfword/doubleword/signed transfers", toks[0]);
	if (!a.isreg && a.imm > 0xff) die("%s: offset %ld out of range (8-bit)", toks[0], a.imm);
	u32 lo = a.isreg ? (u32)a.rm : (u32)a.imm & 0xf, hi = a.isreg ? 0 : ((u32)a.imm >> 4) & 0xf;
	emit32((cond << 28) | ((u32)a.P << 24) | ((u32)a.U << 23) | ((u32)!a.isreg << 22) | ((u32)a.W << 21) | ((u32)Lbit << 20)
	     | ((u32)a.rn << 16) | (rd << 12) | (hi << 8) | (nib << 4) | lo);
}


/* Extract the base register from a `[Rn]` operand spanning toks[start..] (exclusive loads/stores: no offset). */
static int bracket_reg(int start) {
	char buf[64]; size_t bl = 0;
	for (int i = start; i < ntok && bl < sizeof buf - 1; i++) bl += (size_t)snprintf(buf + bl, sizeof buf - bl, "%s", toks[i]);
	char *rb = strchr(buf, ']'); if (rb) *rb = 0;
	char *lb = strchr(buf, '['); int rn = reg(lb ? lb + 1 : buf);
	if (rn < 0) die("%s: bad base register", toks[0]);
	return rn;
}
static void enc_ldrex(u32 cond, u32 base) {   /* ldrex{b,h} Rt, [Rn] */
	u32 rt = need_reg_nopc(1), rn = (u32)bracket_reg(2);
	if (rn == 15) die("%s: instruction does not accept this addressing mode (pc base)", toks[0]);
	emit32((cond << 28) | base | (rn << 16) | (rt << 12));
}
static void enc_strex(u32 cond, u32 base) {   /* strex{b,h} Rd, Rt, [Rn] */
	u32 rd = need_reg_nopc(1), rt = need_reg_nopc(2), rn = (u32)bracket_reg(3);
	if (rn == 15) die("%s: instruction does not accept this addressing mode (pc base)", toks[0]);
	emit32((cond << 28) | base | (rn << 16) | (rd << 12) | rt);
}
/* 64-bit exclusives (kernel atomic64): Rt must be even, Rt2 = Rt+1 (implied by the encoding, checked). */
static void enc_ldrexd(u32 cond) {   /* ldrexd Rt, Rt2, [Rn] */
	int one = ntok == 3;   /* legacy `ldrexd Rt, [Rn]` (Rt2 implied) */
	u32 rt = need_reg_nopc(1), rt2 = one ? rt + 1 : need_reg_nopc(2), rn = (u32)bracket_reg(one ? 2 : 3);
	if ((rt & 1) || rt2 != rt + 1 || rt == 14) die("ldrexd: need an even/odd pair (Rt, Rt+1), got r%u, r%u", rt, rt2);
	if (rn == 15) die("%s: instruction does not accept this addressing mode (pc base)", toks[0]);
	emit32((cond << 28) | 0x01b00f9fu | (rn << 16) | (rt << 12));
}
static void enc_strexd(u32 cond) {   /* strexd Rd, Rt, Rt2, [Rn] */
	int one = ntok == 4;   /* legacy `strexd Rd, Rt, [Rn]` */
	u32 rd = need_reg_nopc(1), rt = need_reg_nopc(2), rt2 = one ? rt + 1 : need_reg_nopc(3), rn = (u32)bracket_reg(one ? 3 : 4);
	if ((rt & 1) || rt2 != rt + 1 || rt == 14) die("strexd: need an even/odd pair (Rt, Rt+1), got r%u, r%u", rt, rt2);
	if (rn == 15) die("%s: instruction does not accept this addressing mode (pc base)", toks[0]);
	emit32((cond << 28) | 0x01a00f90u | (rn << 16) | (rd << 12) | rt);
}

/* ------------------------------------------------------------------ md hooks ---------------------- */
/* Parse an optional UAL suffix after a base mnemonic. Returns 0 on a bad suffix. */
static int suffix_sc(const char *suf, u32 *cond, int *s) { *cond = 14; *s = 0; if (*suf == 's') { *s = 1; suf++; } if (*suf && !lookup_cc(suf, cond)) return 0; return 1; }
static int suffix_c(const char *suf, u32 *cond) { *cond = 14; if (*suf && !lookup_cc(suf, cond)) return 0; return 1; }

/* ---- build attributes: the .ARM.attributes "aeabi" section (ARM IHI 0045) — GAS semantics -----------------
 * Defaults = our target, ARMv7-A (== GNU as -march=armv7-a). `.cpu`/`.arch`/`.arch_extension`/`.fpu` set the
 * derived tags from the tables below (values checked against GNU as 2.42 for every entry); `.eabi_attribute`
 * overrides any tag. Written: Tag_conformance, then Tag_nodefaults, then the rest by ascending tag; zero-valued
 * integer tags are omitted (Tag_nodefaults is written whenever it's set). Unknown CPUs/archs/FPUs are errors. */
typedef struct { const char *key, *name; u8 arch, prof, arm, thumb, mp, div, virt; } ArchAttr;
static const ArchAttr cpu_tab[] = {
	{"arm7tdmi","ARM7TDMI",2,0,1,1,0,0,0}, {"arm9tdmi","ARM9TDMI",2,0,1,1,0,0,0}, {"arm926ej-s","ARM926EJ-S",5,0,1,1,0,0,0},
	{"arm1136j-s","ARM1136J-S",6,0,1,1,0,0,0}, {"arm1136jf-s","ARM1136JF-S",6,0,1,1,0,0,0},
	{"arm1176jz-s","ARM1176JZ-S",7,0,1,1,0,0,1}, {"arm1176jzf-s","ARM1176JZF-S",7,0,1,1,0,0,1}, {"mpcore","MPCore",9,0,1,1,0,0,0},
	{"cortex-a5","Cortex-A5",10,'A',1,2,1,0,1}, {"cortex-a7","Cortex-A7",10,'A',1,2,1,2,3}, {"cortex-a8","Cortex-A8",10,'A',1,2,0,0,1},
	{"cortex-a9","Cortex-A9",10,'A',1,2,1,0,1}, {"cortex-a12","Cortex-A12",10,'A',1,2,1,2,3}, {"cortex-a15","Cortex-A15",10,'A',1,2,1,2,3},
	{"cortex-a17","Cortex-A17",10,'A',1,2,1,2,3}, {"cortex-r4","Cortex-R4",10,'R',1,2,0,0,0}, {"cortex-r5","Cortex-R5",10,'R',1,2,0,2,0},
	{"cortex-m3","Cortex-M3",10,'M',0,2,0,0,0}, {"cortex-m4","Cortex-M4",13,'M',0,2,0,0,0}, {NULL,NULL,0,0,0,0,0,0,0} };
static const ArchAttr arch_tab[] = {
	{"armv4","4",1,0,1,0,0,0,0}, {"armv4t","4T",2,0,1,1,0,0,0}, {"armv5t","5T",3,0,1,1,0,0,0}, {"armv5te","5TE",4,0,1,1,0,0,0},
	{"armv5tej","5TEJ",5,0,1,1,0,0,0}, {"armv6","6",6,0,1,1,0,0,0}, {"armv6k","6K",9,0,1,1,0,0,0}, {"armv6kz","6KZ",7,0,1,1,0,0,1},
	{"armv6t2","6T2",8,0,1,2,0,0,0}, {"armv7","7",10,0,0,2,0,0,0}, {"armv7-a","7-A",10,'A',1,2,0,0,0},
	{"armv7ve","7VE",10,'A',1,2,1,2,3}, {"armv7-r","7-R",10,'R',1,2,0,0,0}, {"armv7-m","7-M",10,'M',0,2,0,0,0},
	{"armv6-m","6-M",11,'M',0,1,0,0,0}, {NULL,NULL,0,0,0,0,0,0,0} };
static ArchAttr attr_cur = {"armv7-a","7-A",10,'A',1,2,0,0,0};   /* our target */
static int attr_fp, attr_simd;
static struct { int set; long ival; char *sval; } eattr[1024];
static const struct { const char *n; int tag; } tag_names[] = {
	{"Tag_CPU_raw_name",4},{"Tag_CPU_name",5},{"Tag_CPU_arch",6},{"Tag_CPU_arch_profile",7},{"Tag_ARM_ISA_use",8},{"Tag_THUMB_ISA_use",9},
	{"Tag_FP_arch",10},{"Tag_VFP_arch",10},{"Tag_WMMX_arch",11},{"Tag_Advanced_SIMD_arch",12},{"Tag_PCS_config",13},{"Tag_ABI_PCS_R9_use",14},
	{"Tag_ABI_PCS_RW_data",15},{"Tag_ABI_PCS_RO_data",16},{"Tag_ABI_PCS_GOT_use",17},{"Tag_ABI_PCS_wchar_t",18},{"Tag_ABI_FP_rounding",19},
	{"Tag_ABI_FP_denormal",20},{"Tag_ABI_FP_exceptions",21},{"Tag_ABI_FP_user_exceptions",22},{"Tag_ABI_FP_number_model",23},
	{"Tag_ABI_align_needed",24},{"Tag_ABI_align8_needed",24},{"Tag_ABI_align_preserved",25},{"Tag_ABI_align8_preserved",25},
	{"Tag_ABI_enum_size",26},{"Tag_ABI_HardFP_use",27},{"Tag_ABI_VFP_args",28},{"Tag_ABI_WMMX_args",29},{"Tag_ABI_optimization_goals",30},
	{"Tag_ABI_FP_optimization_goals",31},{"Tag_compatibility",32},{"Tag_CPU_unaligned_access",34},{"Tag_FP_HP_extension",36},
	{"Tag_VFP_HP_extension",36},{"Tag_ABI_FP_16bit_format",38},{"Tag_MPextension_use",42},{"Tag_DIV_use",44},{"Tag_DSP_extension",46},
	{"Tag_MVE_arch",48},{"Tag_PAC_extension",50},{"Tag_BTI_extension",52},{"Tag_nodefaults",64},{"Tag_also_compatible_with",65},
	{"Tag_T2EE_use",66},{"Tag_conformance",67},{"Tag_Virtualization_use",68},{"Tag_FramePointer_use",72},{"Tag_BTI_use",74},{"Tag_PACRET_use",76},{NULL,0} };
static int tag_is_string(int t) { return t == 4 || t == 5 || t == 67 || (t > 32 && (t & 1)); }
static char *decode_str(const char *tok) {   /* a "..." token -> bytes (C escapes incl. \ooo and \xhh) */
	if (!tok || tok[0] != '"') die(".eabi_attribute: expected a string, got '%s'", tok ? tok : "");
	char *out = malloc(strlen(tok) + 1); size_t k = 0; const char *p = tok + 1;
	while (*p && *p != '"') {
		if (*p != '\\') { out[k++] = *p++; continue; }
		p++;
		if (*p >= '0' && *p <= '7') { int v = 0, n = 0; while (n < 3 && *p >= '0' && *p <= '7') { v = v * 8 + (*p++ - '0'); n++; } out[k++] = (char)v; continue; }
		if (*p == 'x') { p++; int v = 0; while (isxdigit((unsigned char)*p)) { v = v * 16 + (isdigit((unsigned char)*p) ? *p - '0' : (tolower((unsigned char)*p) - 'a' + 10)); p++; } out[k++] = (char)v; continue; }
		char c = *p++; out[k++] = c == 'n' ? '\n' : c == 't' ? '\t' : c == 'r' ? '\r' : c == 'b' ? '\b' : c == 'f' ? '\f' : c;
	}
	if (*p != '"') die(".eabi_attribute: unterminated string");
	out[k] = 0; return out;
}
static void attr_directive(char **t, int n) {
	const char *d = t[0];
	if (!strcmp(d, ".cpu") || !strcmp(d, ".arch")) {
		if (n < 2) die("%s: missing name", d);
		char key[64]; size_t L = strlen(t[1]); if (L >= sizeof key) die("%s: name too long", d);
		for (size_t i = 0; i <= L; i++) key[i] = (char)tolower((unsigned char)t[1][i]);
		const ArchAttr *tab = !strcmp(d, ".cpu") ? cpu_tab : arch_tab;
		for (int i = 0; tab[i].key; i++) if (!strcmp(tab[i].key, key)) { attr_cur = tab[i]; return; }
		die("%s: unknown %s '%s'", d, !strcmp(d, ".cpu") ? "CPU" : "architecture", t[1]);
	}
	if (!strcmp(d, ".arch_extension")) {
		if (n < 2) die(".arch_extension: missing name");
		if (!strcmp(t[1], "idiv")) attr_cur.div = 2;
		else if (!strcmp(t[1], "mp")) attr_cur.mp = 1;
		else if (!strcmp(t[1], "sec")) attr_cur.virt |= 1;
		else if (!strcmp(t[1], "virt")) { attr_cur.virt |= 2; attr_cur.div = 2; }
		else die(".arch_extension: unsupported extension '%s'", t[1]);
		return;
	}
	if (!strcmp(d, ".fpu")) {
		static const struct { const char *n; int fp, simd; } fpus[] = { {"softvfp",0,0}, {"vfp",2,0}, {"vfpv2",2,0}, {"vfpv3",3,0},
			{"vfpv3-d16",4,0}, {"vfpv4",5,0}, {"vfpv4-d16",6,0}, {"neon",3,1}, {"neon-vfpv4",5,2}, {NULL,0,0} };
		if (n < 2) die(".fpu: missing name");
		for (int i = 0; fpus[i].n; i++) if (!strcmp(fpus[i].n, t[1])) { attr_fp = fpus[i].fp; attr_simd = fpus[i].simd; return; }
		die(".fpu: unknown FPU '%s'", t[1]);
	}
	/* .eabi_attribute <tag number|name>, <value>  |  Tag_compatibility: <flag>, "<vendor>" */
	if (n < 3) die(".eabi_attribute: expected tag, value");
	int tag = -1;
	for (int i = 0; tag_names[i].n; i++) if (!strcmp(tag_names[i].n, t[1])) { tag = tag_names[i].tag; break; }
	if (tag < 0) tag = (int)snum(t[1], 0, 1023, "attribute tag");
	eattr[tag].set = 1;
	if (tag == 32) { if (n < 4) die(".eabi_attribute Tag_compatibility: expected flag, \"vendor\""); eattr[tag].ival = eval_const_expr(t[2]); eattr[tag].sval = decode_str(t[3]); }
	else if (tag_is_string(tag)) eattr[tag].sval = decode_str(t[2]);
	else eattr[tag].ival = eval_const_expr(t[2]);
}
static void uleb(unsigned long v) { do { u8 b = v & 0x7f; v >>= 7; if (v) b |= 0x80; emit(&b, 1); } while (v); }
void md_emit_attributes(void) {
	long iv[1024] = {0}; const char *sv[1024] = {0};
	sv[5] = attr_cur.name; iv[6] = attr_cur.arch; iv[7] = attr_cur.prof; iv[8] = attr_cur.arm; iv[9] = attr_cur.thumb;
	iv[10] = attr_fp; iv[12] = attr_simd; iv[42] = attr_cur.mp; iv[44] = attr_cur.div; iv[68] = attr_cur.virt;
	for (int t = 0; t < 1024; t++) if (eattr[t].set) { iv[t] = eattr[t].ival; sv[t] = eattr[t].sval; }
	int save = cursec; sec_get(".ARM.attributes", 0x70000003u, 0);   /* SHT_ARM_ATTRIBUTES */
	emit("A", 1);
	u32 sec_len_at = (u32)secs[cursec].len; emit32(0); emit("aeabi", 6);
	u32 file_at = (u32)secs[cursec].len; u8 one = 1; emit(&one, 1); emit32(0);
	int order[1024], no = 0; order[no++] = 67; order[no++] = 64;
	for (int t = 1; t < 1024; t++) if (t != 67 && t != 64) order[no++] = t;
	for (int i = 0; i < no; i++) { int t = order[i];
		if (t == 64) { if (eattr[64].set) { uleb(64); uleb((unsigned long)iv[64]); } continue; }
		if (t == 32) { if (eattr[32].set) { uleb(32); uleb((unsigned long)iv[32]); emit(sv[32], strlen(sv[32]) + 1); } continue; }
		if (tag_is_string(t)) { if (sv[t] && sv[t][0]) { uleb((unsigned long)t); emit(sv[t], strlen(sv[t]) + 1); } }
		else if (iv[t]) { uleb((unsigned long)t); uleb((unsigned long)iv[t]); }
	}
	u32 end = (u32)secs[cursec].len;
	patch32(cursec, sec_len_at, end - sec_len_at); patch32(cursec, file_at + 1, end - file_at);
	cursec = save;
}

/* ---- literal pools (`ldr Rd, =expr`) — GAS semantics: one pool per section, identical entries shared, dumped
 * word-aligned at `.ltorg`/`.pool` or at the end of assembly (into the section), bracketed by $d / $a; a symbol
 * entry gets an R_ARM_ABS32. A pool load at offset exactly 0 is encoded `[pc, #-0]` (U=0), as GAS does. */
static struct { int sec; u32 val; int sym; int emitted; u32 at; } pool[8192]; static int npool;
static struct { int sec; u32 off; int entry; } poolref[16384]; static int npoolref;
static void join_toks(int from, char *out, size_t n) {
	size_t k = 0; out[0] = 0;
	for (int i = from; i < ntok; i++) { int w = snprintf(out + k, n - k, "%s%s", i > from ? " " : "", toks[i]); if (w < 0 || (size_t)w >= n - k) die("%s: operand too long", toks[0]); k += (size_t)w; }
}
static void pool_ref(u32 insn, u32 val, int sym) {
	int e = -1;
	for (int i = 0; i < npool; i++) if (!pool[i].emitted && pool[i].sec == cursec && pool[i].val == val && pool[i].sym == sym) { e = i; break; }
	if (e < 0) { if (npool >= 8192) die("too many literal-pool entries"); e = npool++; pool[e].sec = cursec; pool[e].val = val; pool[e].sym = sym; pool[e].emitted = 0; }
	if (npoolref >= 16384) die("too many literal-pool loads");
	poolref[npoolref].sec = cursec; poolref[npoolref].off = here(); poolref[npoolref].entry = e; npoolref++;
	emit32(insn);
}
static void pool_flush(int sec) {   /* dump sec's pending entries here, patch their loads */
	int any = 0; for (int i = 0; i < npool; i++) if (!pool[i].emitted && pool[i].sec == sec) { any = 1; break; }
	if (!any) return;
	int save = cursec; cursec = sec;
	while (secs[cursec].len & 3) { u8 z = 0; emit(&z, 1); }
	map_pool_data();
	for (int i = 0; i < npool; i++) if (!pool[i].emitted && pool[i].sec == sec) {
		pool[i].at = here(); pool[i].emitted = 1;
		if (pool[i].sym >= 0) add_reloc(cursec, pool[i].at, pool[i].sym, R_ARM_ABS32);
		emit32(pool[i].val);
	}
	for (int r = 0; r < npoolref; r++) if (poolref[r].sec == sec && pool[poolref[r].entry].emitted && poolref[r].entry >= 0) {
		int32_t delta = (int32_t)pool[poolref[r].entry].at - (int32_t)(poolref[r].off + 8);
		u32 mag = (u32)(delta < 0 ? -delta : delta);
		if (mag > 0xfff) die("literal pool entry out of range (%d bytes) — add a .ltorg closer to the load", delta);
		u32 w = read32(sec, poolref[r].off);
		patch32(sec, poolref[r].off, (w & ~((1u << 23) | 0xfffu)) | ((delta > 0 ? 1u : 0u) << 23) | mag);   /* 0 -> #-0 (GAS) */
		poolref[r].entry = -1;   /* done */
	}
	cursec = save;
}
void md_flush_pools(void) { for (int s = 0; s < nsec; s++) pool_flush(s); }

/* ---- batch: ARMv7-A ARM-state instructions the gas suite / kernel use ------------------------------ */
static void enc_pld(const char *m) {   /* pld/pldw/pli [Rn, #±imm12] | [Rn, ±Rm{, shift}] (unconditional) */
	Addr a = parse_addr(1);
	if (!a.P || a.W) die("%s: only offset addressing", m);
	u32 base = !strcmp(m, "pli") ? 0xf450f000u : !strcmp(m, "pldw") ? 0xf510f000u : 0xf550f000u;   /* imm forms */
	if (a.isreg) { base += 0x02000000u; emit32(base | ((u32)a.U << 23) | ((u32)a.rn << 16) | (u32)a.rm | a.shift); return; }
	if (a.imm > 0xfff) die("%s: offset %ld out of range", m, a.imm);
	emit32(base | ((u32)a.U << 23) | ((u32)a.rn << 16) | (u32)a.imm);
}
static void enc_bitfield(u32 cond, int op) {   /* op 0=bfc Rd,#lsb,#w  1=bfi Rd,Rn,#lsb,#w  2=sbfx  3=ubfx Rd,Rn,#lsb,#w */
	u32 rd = need_reg(1), rn = 15; int i = 2;
	if (op) { rn = (op == 1 && toks[2] && toks[2][0] == '#') ? (snum(toks[2], 0, 0, "bfi #0"), 15u) : need_reg(2); i = 3; }   /* `bfi Rd, #0, ...` == bfc */
	long lsb = snum(i < ntok ? toks[i] : NULL, 0, 31, "lsb"), w = snum(i + 1 < ntok ? toks[i + 1] : NULL, 1, 32 - lsb, "width");
	if (i + 2 < ntok) die("%s: too many operands", toks[0]);
	if (op <= 1) emit32((cond << 28) | 0x07c00010u | ((u32)(lsb + w - 1) << 16) | (rd << 12) | ((u32)lsb << 7) | rn);
	else emit32((cond << 28) | (op == 2 ? 0x07a00050u : 0x07e00050u) | ((u32)(w - 1) << 16) | (rd << 12) | ((u32)lsb << 7) | rn);
}
static void enc_swp(u32 cond, int b) {   /* swp{b} Rt, Rt2, [Rn] */
	u32 rt = need_reg_nopc(1), rt2 = need_reg_nopc(2), rn = (u32)bracket_reg(3);
	if (rn == 15) die("%s: r15 not allowed here", toks[0]);
	emit32((cond << 28) | 0x01000090u | ((u32)b << 22) | (rn << 16) | (rt << 12) | rt2);
}
static void enc_mcrr(u32 cond, u32 L) {   /* mcrr/mrrc{2} p<cp>, <opc1 0-15>, Rt, Rt2, c<CRm> */
	if (ntok != 6) die("%s: expected p<cp>, <opc1>, Rt, Rt2, c<m>", toks[0]);
	u32 cp = cpreg(toks[1], 'p', "coprocessor"), opc1 = (u32)snum(toks[2], 0, 15, "opc1"), rt = need_reg(3), rt2 = need_reg(4), crm = cpreg(toks[5], 'c', "CRm");
	emit32((cond << 28) | 0x0c400000u | L | (rt2 << 16) | (rt << 12) | (cp << 8) | (opc1 << 4) | crm);
}
static void enc_cdp(u32 cond) {   /* cdp{2} p<cp>, <opc1 0-15>, c<CRd>, c<CRn>, c<CRm>{, <opc2 0-7>} */
	if (ntok < 6 || ntok > 7) die("%s: expected p<cp>, <opc1>, c<d>, c<n>, c<m>[, <opc2>]", toks[0]);
	u32 cp = cpreg(toks[1], 'p', "coprocessor"), opc1 = (u32)snum(toks[2], 0, 15, "opc1");
	u32 crd = cpreg(toks[3], 'c', "CRd"), crn = cpreg(toks[4], 'c', "CRn"), crm = cpreg(toks[5], 'c', "CRm");
	u32 opc2 = ntok == 7 ? (u32)snum(toks[6], 0, 7, "opc2") : 0;
	emit32((cond << 28) | 0x0e000000u | (opc1 << 20) | (crn << 16) | (crd << 12) | (cp << 8) | (opc2 << 5) | crm);
}
static void enc_dsp_mul(u32 cond, u32 base, int nregs, int xy) {   /* halfword multiplies; xy: bit5 = x (top of Rn), bit6 = y (top of Rm) */
	u32 r[4]; for (int i = 0; i < nregs; i++) r[i] = need_reg_nopc(i + 1);
	if (ntok != nregs + 1) die("%s: expected %d registers", toks[0], nregs);
	u32 w;
	if (base == 0x01400080u) w = base | (r[1] << 16) | (r[0] << 12) | (r[3] << 8) | r[2];                      /* smlal<xy> RdLo, RdHi, Rn, Rm */
	else if (nregs == 4)     w = base | (r[0] << 16) | (r[3] << 12) | (r[2] << 8) | r[1];                       /* smla<xy>/smlaw<y> Rd, Rn, Rm, Ra */
	else                     w = base | (r[0] << 16) | (r[2] << 8) | r[1];                                      /* smul<xy>/smulw<y> Rd, Rn, Rm */
	emit32((cond << 28) | w | (u32)xy);
}
static void enc_qarith(u32 cond, u32 base) {   /* qadd/qsub/qdadd/qdsub Rd, Rm, Rn */
	if (ntok != 4) die("%s: expected Rd, Rm, Rn", toks[0]);
	u32 rd = need_reg_nopc(1), rm = need_reg_nopc(2), rn = need_reg_nopc(3);
	emit32((cond << 28) | base | (rn << 16) | (rd << 12) | rm);
}
static void enc_ldst_t(u32 cond, int is_load, int is_byte) {   /* ldrt/strt/ldrbt/strbt Rt, [Rn]{, #±imm | ±Rm{, shift}} — post-indexed, W=1 */
	u32 rd = need_reg(1); Addr a = parse_addr(2);
	if (a.P == 1 && (a.imm || a.isreg)) die("%s: only post-indexed addressing", toks[0]);
	if (!a.isreg && a.imm > 0xfff) die("%s: offset %ld out of range (12-bit)", toks[0], a.imm);
	u32 off = a.isreg ? ((u32)a.rm | a.shift) : (u32)a.imm;
	emit32((cond << 28) | (1u << 26) | ((u32)a.isreg << 25) | ((u32)a.U << 23) | ((u32)is_byte << 22) | (1u << 21)
	     | ((u32)is_load << 20) | ((u32)a.rn << 16) | (rd << 12) | off);
}
/* ldc/stc{2}{l}{cond} p<cp>, c<CRd>, [Rn, #±imm]{!} | [Rn], #±imm | [Rn], {option}  (imm: multiple of 4, <= 1020) */
static void enc_ldc(u32 cond, int L, int N) {
	if (ntok < 4) die("%s: expected p<cp>, c<CRd>, <address>", toks[0]);
	u32 cp = cpreg(toks[1], 'p', "coprocessor"), crd = cpreg(toks[2], 'c', "CRd");
	char ex[256]; size_t k = 0; ex[0] = 0;
	for (int i = 3; i < ntok; i++) k += (size_t)snprintf(ex + k, sizeof ex - k, "%s%s", i > 3 ? " " : "", toks[i]);
	char *br = strchr(ex, '{');
	if (br) {   /* unindexed: [Rn], {option} -> P=0 U=1 W=0, imm8 = option */
		char *rb = strchr(ex, ']'); if (!ex[0] || ex[0] != '[' || !rb) die("%s: bad address", toks[0]);
		*rb = 0; int rn = reg(ex + 1); if (rn < 0) die("%s: bad base register", toks[0]);
		char *close = strchr(br, '}'); if (!close || close[1]) die("%s: bad {option}", toks[0]); *close = 0;
		long opt = eval_const_expr(br + 1); if (opt < 0 || opt > 255) die("%s: option %ld out of range", toks[0], opt);
		emit32((cond << 28) | 0x0c800000u | ((u32)N << 22) | ((u32)L << 20) | ((u32)rn << 16) | (crd << 12) | (cp << 8) | (u32)opt); return;
	}
	Addr a = parse_addr(3);
	if (a.isreg) die("%s: register offsets not allowed", toks[0]);
	if ((a.imm & 3) || a.imm > 1020) die("%s: offset %ld must be a multiple of 4 up to 1020", toks[0], a.imm);
	if (!a.P) a.W = 1;   /* post-indexed: W=1 */
	emit32((cond << 28) | 0x0c000000u | ((u32)a.P << 24) | ((u32)a.U << 23) | ((u32)N << 22) | ((u32)a.W << 21) | ((u32)L << 20)
	     | ((u32)a.rn << 16) | (crd << 12) | (cp << 8) | ((u32)a.imm >> 2));
}
/* Pre-UAL ("divided" syntax, GAS's default) puts the condition BEFORE the size/mode suffix: ldreqb, swpgeb,
 * umlaleqs, ldmeqfd. Rewrite base+cond+suffix -> base+suffix+cond (UAL). Only when the suffix is non-empty and
 * valid for that base, so a real UAL name (e.g. ldrhs = ldr + hs) is never reinterpreted. */
static const char *ual_name(const char *m, char *buf, size_t n) {
	static const struct { const char *base; const char *suf[12]; } lg[] = {
		{"ldr", {"b","h","sb","sh","d","t","bt",0}}, {"str", {"b","h","d","t","bt",0}},
		{"ldm", {"ia","ib","da","db","fd","ed","fa","ea",0}}, {"stm", {"ia","ib","da","db","fd","ed","fa","ea",0}},
		{"swp", {"b",0}}, {"ldc", {"l",0}}, {"stc", {"l",0}},
		{"umull", {"s",0}}, {"umlal", {"s",0}}, {"smull", {"s",0}}, {"smlal", {"s",0}}, {"mul", {"s",0}}, {"mla", {"s",0}},
		{"and",{"s",0}},{"eor",{"s",0}},{"sub",{"s",0}},{"rsb",{"s",0}},{"add",{"s",0}},{"adc",{"s",0}},{"sbc",{"s",0}},{"rsc",{"s",0}},
		{"orr",{"s",0}},{"mov",{"s",0}},{"bic",{"s",0}},{"mvn",{"s",0}},{"lsl",{"s",0}},{"lsr",{"s",0}},{"asr",{"s",0}},{"ror",{"s",0}},
		{"tst",{"p",0}},{"teq",{"p",0}},{"cmp",{"p",0}},{"cmn",{"p",0}},
	};
	size_t L = strlen(m);
	for (unsigned i = 0; i < sizeof lg / sizeof *lg; i++) {
		size_t bl = strlen(lg[i].base); u32 cc;
		if (L < bl + 3 || strncmp(m, lg[i].base, bl)) continue;
		char c2[3] = { m[bl], m[bl + 1], 0 }; if (!lookup_cc(c2, &cc)) continue;
		const char *rest = m + bl + 2;
		for (int j = 0; lg[i].suf[j]; j++) if (!strcmp(rest, lg[i].suf[j])) {
			if (snprintf(buf, n, "%s%s%s", lg[i].base, rest, c2) >= (int)n) return m;
			return buf;
		}
	}
	return m;
}
/* `xy` suffix of a halfword multiply: "bb"/"bt"/"tb"/"tt" -> bits; returns chars consumed (2) or 0. */
static int dsp_xy(const char *q, int *xy) {
	if ((q[0] != 'b' && q[0] != 't') || (q[1] != 'b' && q[1] != 't')) return 0;
	*xy = (q[0] == 't' ? 0x20 : 0) | (q[1] == 't' ? 0x40 : 0); return 2;
}
static int dsp_y(const char *q, int *xy) { if (q[0] != 'b' && q[0] != 't') return 0; *xy = q[0] == 't' ? 0x40 : 0; return 1; }

/* ---- ARMv6 media instructions ------------------------------------------------------------------------- */
static void enc_3reg_media(u32 cond, u32 base) {   /* Rd, Rn, Rm -> Rn<<16 | Rd<<12 | Rm (parallel add/sub, sel) */
	if (ntok != 4) die("%s: expected Rd, Rn, Rm", toks[0]);
	emit32((cond << 28) | base | (need_reg_nopc(2) << 16) | (need_reg_nopc(1) << 12) | need_reg_nopc(3));
}
static void enc_pkh(u32 cond, int tb) {   /* pkhbt Rd, Rn, Rm{, lsl #0-31} | pkhtb Rd, Rn, Rm{, asr #1-32} */
	u32 rd = need_reg_nopc(1), rn = need_reg_nopc(2), rm = need_reg_nopc(3), sh = 0;
	if (ntok > 4) {
		if (strcasecmp(toks[4], tb ? "asr" : "lsl") || ntok != 6) die("%s: expected `, %s #n`", toks[0], tb ? "asr" : "lsl");
		long n = snum(toks[5], tb ? 1 : 0, tb ? 32 : 31, "shift"); sh = (u32)(n & 31);
	} else if (tb) { u32 t = rn; rn = rm; rm = t; tb = 0; }   /* pkhtb Rd, Rn, Rm == pkhbt Rd, Rm, Rn (GAS) */
	emit32((cond << 28) | 0x06800010u | ((u32)tb << 6) | (rn << 16) | (rd << 12) | (sh << 7) | rm);
}
static void enc_sat(u32 cond, int u, int is16) {   /* {s,u}sat Rd, #sat, Rn{, lsl|asr #n}; {s,u}sat16 Rd, #sat, Rn */
	u32 rd = need_reg_nopc(1); long sat = snum(ntok > 2 ? toks[2] : NULL, u ? 0 : 1, is16 ? (u ? 15 : 16) : (u ? 31 : 32), "saturate position");
	u32 rn = need_reg_nopc(3), sf = u ? (u32)sat : (u32)(sat - 1);
	if (is16) { if (ntok != 4) die("%s: expected Rd, #sat, Rn", toks[0]); emit32((cond << 28) | (u ? 0x06e00f30u : 0x06a00f30u) | (sf << 16) | (rd << 12) | rn); return; }
	u32 sh = 0, amt = 0;
	if (ntok > 4) {
		if (ntok != 6) die("%s: expected `, lsl|asr #n`", toks[0]);
		if (!strcasecmp(toks[4], "lsl")) amt = (u32)snum(toks[5], 0, 31, "shift");
		else if (!strcasecmp(toks[4], "asr")) { sh = 1; amt = (u32)snum(toks[5], 1, 32, "shift") & 31; }
		else die("%s: bad shift '%s'", toks[0], toks[4]);
	}
	emit32((cond << 28) | (u ? 0x06e00010u : 0x06a00010u) | (sf << 16) | (rd << 12) | (amt << 7) | (sh << 6) | rn);
}
static void enc_xt_add(u32 cond, u32 base) {   /* {s,u}xta{b,h,b16} Rd, Rn, Rm{, ror #n} */
	u32 rd = need_reg_nopc(1), rn = need_reg_nopc(2), rm = need_reg_nopc(3);
	emit32((cond << 28) | base | (rn << 16) | (rd << 12) | ror_field(4) | rm);
}
static void enc_mul4(u32 cond, u32 base, int regs, int hilo) {   /* Rd, Rn, Rm[, Ra]  or  RdLo, RdHi, Rn, Rm (hilo) */
	if (ntok != regs + 1) die("%s: expected %d registers", toks[0], regs);
	u32 a = need_reg_nopc(1), b = need_reg_nopc(2), c = need_reg_nopc(3), d = regs == 4 ? need_reg_nopc(4) : 15;
	if (hilo) emit32((cond << 28) | base | (b << 16) | (a << 12) | (d << 8) | c);          /* RdHi<<16 RdLo<<12 Rm<<8 Rn */
	else      emit32((cond << 28) | base | (a << 16) | (d << 12) | (c << 8) | b);          /* Rd<<16 Ra<<12 Rm<<8 Rn */
}
static void enc_rfe_srs(int srs, u32 P, u32 U) {   /* rfe{mode} Rn{!}  |  srs{mode} sp{!}, #mode  (unconditional) */
	char r[16]; strncpy(r, toks[1] ? toks[1] : "", sizeof r - 1); r[sizeof r - 1] = 0;
	u32 W = 0; size_t l = strlen(r); if (l && r[l - 1] == '!') { W = 1; r[l - 1] = 0; }
	if (srs) {   /* srs sp{!}, #mode  |  legacy srs #mode{!} (sp implied) */
		int ai = 2;
		if (r[0] == '#') ai = 1; else if (reg(r) != 13) die("%s: base register must be sp", toks[0]);
		char mb[32]; strncpy(mb, ai < ntok ? toks[ai] : "", sizeof mb - 1); mb[sizeof mb - 1] = 0;
		size_t ml = strlen(mb); if (ai == 1 && ml && mb[ml - 1] == '!') mb[--ml] = 0;
		u32 mode = (u32)snum(mb, 0, 31, "mode");
		emit32(0xf84d0500u | (P << 24) | (U << 23) | (W << 21) | mode); return; }
	int rn = reg(r); if (rn < 0) die("%s: bad base register '%s'", toks[0], toks[1] ? toks[1] : "");
	if (rn == 15) die("%s: r15 not allowed here", toks[0]);
	emit32(0xf8100a00u | (P << 24) | (U << 23) | (W << 21) | ((u32)rn << 16));
}
/* Parallel add/subtract: <prefix><op>{cond}. prefix: s q sh u uq uh; op: add16 asx sax sub16 add8 sub8 (+ legacy
 * addsubx=asx, subaddx=sax). Returns 1 if matched. */
static int try_parallel(const char *m) {
	static const struct { const char *p; u32 op1; } pf[] = { {"sh",3}, {"uq",6}, {"uh",7}, {"s",1}, {"q",2}, {"u",5} };
	static const struct { const char *o; u32 op2; } ops[] = { {"add16",0}, {"addsubx",1}, {"asx",1}, {"subaddx",2}, {"sax",2}, {"sub16",3}, {"add8",4}, {"sub8",7} };
	for (unsigned i = 0; i < sizeof pf / sizeof *pf; i++) { size_t pl = strlen(pf[i].p); if (strncmp(m, pf[i].p, pl)) continue;
		for (unsigned j = 0; j < sizeof ops / sizeof *ops; j++) { size_t ol = strlen(ops[j].o); u32 cond;
			if (!strncmp(m + pl, ops[j].o, ol) && suffix_c(m + pl + ol, &cond)) {
				enc_3reg_media(cond, 0x06000f10u | (pf[i].op1 << 20) | (ops[j].op2 << 5)); return 1; } } }
	return 0;
}
static int media_insn(const char *m) {
	u32 cond; size_t L = strlen(m);
	if (try_parallel(m)) return 1;
	if (!strncmp(m, "pkhbt", 5) && suffix_c(m + 5, &cond)) { enc_pkh(cond, 0); return 1; }
	if (!strncmp(m, "pkhtb", 5) && suffix_c(m + 5, &cond)) { enc_pkh(cond, 1); return 1; }
	if (!strncmp(m, "sel", 3) && suffix_c(m + 3, &cond)) { enc_3reg_media(cond, 0x06800fb0u); return 1; }
	if (!strncmp(m, "ssat16", 6) && suffix_c(m + 6, &cond)) { enc_sat(cond, 0, 1); return 1; }
	if (!strncmp(m, "usat16", 6) && suffix_c(m + 6, &cond)) { enc_sat(cond, 1, 1); return 1; }
	if (!strncmp(m, "ssat", 4) && suffix_c(m + 4, &cond)) { enc_sat(cond, 0, 0); return 1; }
	if (!strncmp(m, "usat", 4) && suffix_c(m + 4, &cond)) { enc_sat(cond, 1, 0); return 1; }
	static const struct { const char *n; u32 b; } xa[] = { {"sxtab16",0x06800070u}, {"uxtab16",0x06c00070u}, {"sxtab",0x06a00070u},
		{"sxtah",0x06b00070u}, {"uxtab",0x06e00070u}, {"uxtah",0x06f00070u} };
	for (unsigned i = 0; i < sizeof xa / sizeof *xa; i++) { size_t l = strlen(xa[i].n);
		if (!strncmp(m, xa[i].n, l) && suffix_c(m + l, &cond)) { enc_xt_add(cond, xa[i].b); return 1; } }
	if (!strncmp(m, "sxtb16", 6) && suffix_c(m + 6, &cond)) { enc_extend(cond, 0x068f0070u); return 1; }
	if (!strncmp(m, "uxtb16", 6) && suffix_c(m + 6, &cond)) { enc_extend(cond, 0x06cf0070u); return 1; }
	static const struct { const char *n; u32 b; int regs, hilo; } mu[] = {
		{"smladx",0x07000030u,4,0}, {"smlad",0x07000010u,4,0}, {"smlsdx",0x07000070u,4,0}, {"smlsd",0x07000050u,4,0},
		{"smuadx",0x0700f030u,3,0}, {"smuad",0x0700f010u,3,0}, {"smusdx",0x0700f070u,3,0}, {"smusd",0x0700f050u,3,0},
		{"smlaldx",0x07400030u,4,1}, {"smlald",0x07400010u,4,1}, {"smlsldx",0x07400070u,4,1}, {"smlsld",0x07400050u,4,1},
		{"smmlar",0x07500030u,4,0}, {"smmla",0x07500010u,4,0}, {"smmlsr",0x075000f0u,4,0}, {"smmls",0x075000d0u,4,0},
		{"smmulr",0x0750f030u,3,0}, {"smmul",0x0750f010u,3,0}, {"usada8",0x07800010u,4,0}, {"usad8",0x0780f010u,3,0},
		{"umaal",0x00400090u,4,1} };
	for (unsigned i = 0; i < sizeof mu / sizeof *mu; i++) { size_t l = strlen(mu[i].n);
		if (!strncmp(m, mu[i].n, l) && suffix_c(m + l, &cond)) { enc_mul4(cond, mu[i].b, mu[i].regs, mu[i].hilo); return 1; } }
	static const struct { const char *n; u32 P, U; } am[] = { {"ia",0,1}, {"ib",1,1}, {"da",0,0}, {"db",1,0}, {"",0,1},
		{"fd",0,1}, {"ed",1,1}, {"fa",0,0}, {"ea",1,0} };
	if (!strncmp(m, "rfe", 3) || !strncmp(m, "srs", 3))
		for (unsigned i = 0; i < sizeof am / sizeof *am; i++) if (!strcmp(m + 3, am[i].n)) {
			int srs = m[0] == 's'; u32 P = am[i].P, U = am[i].U;
			if (i >= 5) { /* stack aliases: rfe = load (fd=ia..), srs = store (fd=db..) */
				static const u32 lP[] = {0,1,0,1}, lU[] = {1,1,0,0}, sP[] = {1,0,1,0}, sU[] = {0,0,1,1};
				P = srs ? sP[i - 5] : lP[i - 5]; U = srs ? sU[i - 5] : lU[i - 5]; }
			enc_rfe_srs(srs, P, U); return 1; }
	if (!strcmp(m, "setend")) { if (ntok != 2) die("setend: expected be|le");
		if (!strcasecmp(toks[1], "be")) emit32(0xf1010200u); else if (!strcasecmp(toks[1], "le")) emit32(0xf1010000u); else die("setend: expected be|le"); return 1; }
	(void)L;
	return 0;
}

void md_assemble(char **t, int n) {
	toks = t; ntok = n;
	for (char *c = toks[0]; *c; c++) *c = (char)tolower((unsigned char)*c);   /* GAS: mnemonics are case-insensitive */
	static char ualbuf[32]; const char *m = ual_name(toks[0], ualbuf, sizeof ualbuf); toks[0] = (char *)m;
	size_t L = strlen(m); u32 cond; int s;
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

	if (media_insn(m)) return;
	if (!strncmp(m, "clrex", 5) && !m[5]) { emit32(0xf57ff01fu); return; }
	if (!strcmp(m, "bkpt")) { u32 v = ntok > 1 ? (u32)snum(toks[1], 0, 0xffff, "bkpt number") : 0; emit32(0xe1200070u | ((v >> 4) << 8) | (v & 15)); return; }
	{   /* tstp/teqp/cmpp/cmnp{cond}: legacy (26-bit) flag-setting compares = Rd field r15 */
		static const struct { const char *n; u32 opc; } pc[] = { {"tstp",8}, {"teqp",9}, {"cmpp",10}, {"cmnp",11} };
		for (unsigned i = 0; i < 4; i++) if (!strncmp(m, pc[i].n, 4) && suffix_c(m + 4, &cond)) {
			u32 I, rn = need_reg(1), op2 = operand2(2, &I);
			emit32((cond << 28) | (I << 25) | (pc[i].opc << 21) | (1u << 20) | (rn << 16) | (15u << 12) | op2); return; }
	}
	if (!strncmp(m, "bxj", 3) && suffix_c(m + 3, &cond)) { emit32((cond << 28) | 0x012fff20u | need_reg(1)); return; }
	if (!strncmp(m, "bfc", 3) && suffix_c(m + 3, &cond)) { enc_bitfield(cond, 0); return; }
	if (!strncmp(m, "bfi", 3) && suffix_c(m + 3, &cond)) { enc_bitfield(cond, 1); return; }
	if (!strncmp(m, "sbfx", 4)) { if (!suffix_c(m + 4, &cond)) die("%s: bad suffix", m); enc_bitfield(cond, 2); return; }
	if (!strncmp(m, "ubfx", 4)) { if (!suffix_c(m + 4, &cond)) die("%s: bad suffix", m); enc_bitfield(cond, 3); return; }
	if (!strncmp(m, "swpb", 4) && suffix_c(m + 4, &cond)) { enc_swp(cond, 1); return; }
	if (!strncmp(m, "swp", 3) && suffix_c(m + 3, &cond)) { enc_swp(cond, 0); return; }
	if (!strncmp(m, "ldc", 3) || !strncmp(m, "stc", 3)) {   /* ldc/stc{2}{l}{cond} */
		const char *q = m + 3; int two = 0, N = 0;
		if (*q == '2') { two = 1; q++; }
		if (*q == 'l') { N = 1; q++; }
		if (two ? *q == 0 : suffix_c(q, &cond)) { enc_ldc(two ? 15 : cond, m[0] == 'l', N); return; }
	}
	if (!strcmp(m, "mcrr2")) { enc_mcrr(15, 0); return; }
	if (!strcmp(m, "mrrc2")) { enc_mcrr(15, 1u << 20); return; }
	if (!strncmp(m, "mcrr", 4)) { if (!suffix_c(m + 4, &cond)) die("%s: bad suffix", m); enc_mcrr(cond, 0); return; }
	if (!strncmp(m, "mrrc", 4)) { if (!suffix_c(m + 4, &cond)) die("%s: bad suffix", m); enc_mcrr(cond, 1u << 20); return; }
	if (!strcmp(m, "mcr2")) { enc_mcr(15, 0); return; }
	if (!strcmp(m, "mrc2")) { enc_mcr(15, 1u << 20); return; }
	if (!strcmp(m, "cdp2")) { enc_cdp(15); return; }
	if (!strncmp(m, "cdp", 3)) { if (!suffix_c(m + 3, &cond)) die("%s: bad suffix", m); enc_cdp(cond); return; }
	if (!strncmp(m, "cpy", 3) && suffix_c(m + 3, &cond)) { if (ntok != 3) die("cpy: expected Rd, Rm"); emit32((cond << 28) | 0x01a00000u | (need_reg(1) << 12) | need_reg(2)); return; }
	{   /* q-arithmetic + halfword DSP multiplies (before smull/smlal, which share prefixes) */
		static const struct { const char *n; u32 b; } qa[] = { {"qdadd",0x01400050u}, {"qdsub",0x01600050u}, {"qadd",0x01000050u}, {"qsub",0x01200050u} };
		for (unsigned i = 0; i < sizeof qa / sizeof *qa; i++) { size_t l = strlen(qa[i].n);
			if (!strncmp(m, qa[i].n, l) && suffix_c(m + l, &cond)) { enc_qarith(cond, qa[i].b); return; } }
		int xy = 0, k;
		if (!strncmp(m, "smlal", 5) && (k = dsp_xy(m + 5, &xy)) && suffix_c(m + 7, &cond)) { enc_dsp_mul(cond, 0x01400080u, 4, xy); return; }
		if (!strncmp(m, "smlaw", 5) && (k = dsp_y(m + 5, &xy)) && suffix_c(m + 6, &cond)) { enc_dsp_mul(cond, 0x01200080u, 4, xy); return; }
		if (!strncmp(m, "smulw", 5) && (k = dsp_y(m + 5, &xy)) && suffix_c(m + 6, &cond)) { enc_dsp_mul(cond, 0x012000a0u, 3, xy); return; }
		if (!strncmp(m, "smla", 4) && (k = dsp_xy(m + 4, &xy)) && suffix_c(m + 6, &cond)) { enc_dsp_mul(cond, 0x01000080u, 4, xy); return; }
		if (!strncmp(m, "smul", 4) && (k = dsp_xy(m + 4, &xy)) && suffix_c(m + 6, &cond)) { enc_dsp_mul(cond, 0x01600080u, 3, xy); return; }
	}
	if ((!strncmp(m, "ldrht", 5) || !strncmp(m, "strht", 5) || !strncmp(m, "ldrsbt", 6) || !strncmp(m, "ldrsht", 6))) {   /* v6T2 unprivileged halfword/signed */
		int sgn = m[3] == 's'; u32 nib = !sgn ? 0xb : m[4] == 'b' ? 0xd : 0xf; int L = m[0] == 'l';
		if (!suffix_c(m + (sgn ? 6 : 5), &cond)) die("%s: bad suffix", m);
		u32 rd = need_reg(1); Addr a = parse_addr(2);
		if (a.P == 1 && (a.imm || a.isreg)) die("%s: only post-indexed addressing", m);
		if (a.isreg && a.shift) die("%s: no shifted index", m);
		if (!a.isreg && a.imm > 0xff) die("%s: offset %ld out of range (8-bit)", m, a.imm);
		u32 lo = a.isreg ? (u32)a.rm : (u32)a.imm & 0xf, hi = a.isreg ? 0 : ((u32)a.imm >> 4) & 0xf;
		emit32((cond << 28) | ((u32)a.U << 23) | ((u32)!a.isreg << 22) | (1u << 21) | ((u32)L << 20) | ((u32)a.rn << 16) | (rd << 12) | (hi << 8) | (nib << 4) | lo);
		return;
	}
	if (m[0] == 'i' && m[1] == 't') {   /* IT blocks in ARM state: accepted, no code (GAS: they only check the conditions) */
		int ok = 1; for (const char *c = m + 2; *c; c++) if (*c != 't' && *c != 'e') ok = 0;
		if (ok && strlen(m) <= 5) { u32 cc; if (ntok != 2 || !lookup_cc(toks[1], &cc)) die("%s: expected a condition", m); return; }
	}
	if ((!strncmp(m, "ldr", 3) || !strncmp(m, "str", 3)) && (m[3] == 't' || (m[3] == 'b' && m[4] == 't'))) {   /* ldrt/ldrbt/strt/strbt */
		int bb = m[3] == 'b'; if (!suffix_c(m + (bb ? 5 : 4), &cond)) die("%s: bad suffix", m);
		enc_ldst_t(cond, m[0] == 'l', bb); return;
	}
	/* exclusive load/store (atomics): ldrex/strex {b,h}. Checked before the general ldr/str decode. */
	if (!strncmp(m, "ldrex", 5) || !strncmp(m, "strex", 5)) {   /* {,b,h,d}{cond} — was: any suffix silently -> word */
		int ld = m[0] == 'l'; const char *q = m + 5; char w = 0;
		if (*q == 'b' || *q == 'h' || *q == 'd') { w = *q; q++; }
		u32 cond = 14; if (*q && !lookup_cc(q, &cond)) die("%s: bad suffix '%s'", m, q);
		if (w == 'd') { if (ld) enc_ldrexd(cond); else enc_strexd(cond); return; }
		if (ld) enc_ldrex(cond, w=='b'?0x01d00f9fu : w=='h'?0x01f00f9fu : 0x01900f9fu);
		else    enc_strex(cond, w=='b'?0x01c00f90u : w=='h'?0x01e00f90u : 0x01800f90u);
		return;
	}
	/* single data transfer: ldr/str {b|d|h}{cond}. Whole-suffix cond FIRST so "ldrhi"=ldr+hi. */
	if (!strncmp(m, "adr", 3)) { const char *suf = m + 3; u32 cond = 14; if (!*suf || lookup_cc(suf, &cond)) { enc_adr(cond); return; } }
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
		else if (suf[0] && suf[1] && strchr("fe", suf[0]) && strchr("da", suf[1]) && (suf[2] == 0 || lookup_cc(suf + 2, &cond))) {
			/* stack aliases: full/empty + descending/ascending. ldmfd=ia ldmed=ib ldmfa=da ldmea=db; stm the mirror */
			/* LDM: FD=IA ED=IB FA=DA EA=DB  (U = descending, P = empty)   STM: FD=DB ED=DA FA=IB EA=IA  (U = ascending, P = full) */
			int full = suf[0] == 'f', desc = suf[1] == 'd';
			if (m[0] == 'l') { U = desc; P = !full; } else { U = !desc; P = full; }
			suf += 2;
		}
		if (!suffix_c(suf, &cond)) die("%s: unsupported ldm/stm mode", m);
		enc_ldstm(m[0] == 'l', cond, P, U); return;
	}
	if (!strncmp(m, "uxtb", 4)) { if (!suffix_c(m + 4, &cond)) die("%s: bad suffix", m); enc_extend(cond, 0x06ef0070u); return; }
	if (!strncmp(m, "uxth", 4)) { if (!suffix_c(m + 4, &cond)) die("%s: bad suffix", m); enc_extend(cond, 0x06ff0070u); return; }
	if (!strncmp(m, "sxtb", 4)) { if (!suffix_c(m + 4, &cond)) die("%s: bad suffix", m); enc_extend(cond, 0x06af0070u); return; }
	if (!strncmp(m, "sxth", 4)) { if (!suffix_c(m + 4, &cond)) die("%s: bad suffix", m); enc_extend(cond, 0x06bf0070u); return; }
	if (!strncmp(m, "rev16", 5)) { if (!suffix_c(m + 5, &cond)) die("%s: bad suffix", m); enc_extend(cond, 0x06bf0fb0u); return; }   /* byte-reverse in each halfword */
	if (!strncmp(m, "revsh", 5)) { if (!suffix_c(m + 5, &cond)) die("%s: bad suffix", m); enc_extend(cond, 0x06ff0fb0u); return; }   /* reverse + sign-extend halfword */
	if (!strncmp(m, "rbit", 4))  { if (!suffix_c(m + 4, &cond)) die("%s: bad suffix", m); enc_extend(cond, 0x06ff0f30u); return; }   /* reverse bit order */
	if (!strncmp(m, "rev", 3))   { if (!suffix_c(m + 3, &cond)) die("%s: bad suffix", m); enc_extend(cond, 0x06bf0f30u); return; }   /* byte-reverse word */
	if (!strncmp(m, "movw", 4) || !strncmp(m, "movt", 4)) { if (!suffix_c(m + 4, &cond)) die("%s: bad suffix", m); enc_movw(m[3] == 't', cond); return; }
	if (!strncmp(m, "mul", 3)) { if (!suffix_sc(m + 3, &cond, &s)) die("%s: bad suffix", m); enc_mul(cond, s); return; }
	if (!strncmp(m, "mla", 3)) { if (!suffix_sc(m + 3, &cond, &s)) die("%s: bad suffix", m); enc_mla(cond, s); return; }
	if (!strncmp(m, "mls", 3)) { if (!suffix_c(m + 3, &cond)) die("%s: bad suffix", m); enc_mls(cond); return; }
	if (!strncmp(m, "umull", 5)) { if (!suffix_sc(m + 5, &cond, &s)) die("%s: bad suffix", m); enc_umull_s(cond, 0x00800090u, s); return; }
	if (!strncmp(m, "umlal", 5)) { if (!suffix_sc(m + 5, &cond, &s)) die("%s: bad suffix", m); enc_umull_s(cond, 0x00a00090u, s); return; }
	if (!strncmp(m, "smull", 5)) { if (!suffix_sc(m + 5, &cond, &s)) die("%s: bad suffix", m); enc_umull_s(cond, 0x00c00090u, s); return; }
	if (!strncmp(m, "smlal", 5)) { if (!suffix_sc(m + 5, &cond, &s)) die("%s: bad suffix", m); enc_umull_s(cond, 0x00e00090u, s); return; }
	if (!strncmp(m, "udiv", 4)) { if (!suffix_c(m + 4, &cond)) die("%s: bad suffix", m); enc_div(0, cond); return; }
	if (!strncmp(m, "sdiv", 4)) { if (!suffix_c(m + 4, &cond)) die("%s: bad suffix", m); enc_div(1, cond); return; }
	if (!strncmp(m, "clz", 3)) { if (!suffix_c(m + 3, &cond)) die("%s: bad suffix", m); enc_clz(cond); return; }
	if (!strncmp(m, "svc", 3)) { if (!suffix_c(m + 3, &cond)) die("%s: bad suffix", m); enc_svc(cond); return; }
	if (!strcmp(m, "pld") || !strcmp(m, "pldw") || !strcmp(m, "pli")) { enc_pld(m); return; }   /* was: silently a NOP */
	{   /* NOP-space hints, optionally conditional (e.g. `wfene` in spinlock loops) */
		static const struct { const char *n; u32 h; } hn[] = { {"nop",0},{"yield",1},{"wfe",2},{"wfi",3},{"sev",4},{"sevl",5} };
		for (unsigned i = 0; i < sizeof hn/sizeof *hn; i++) { size_t l = strlen(hn[i].n);
			if (!strncmp(m, hn[i].n, l)) { u32 cc = 14;
				if (m[l] == 0 || (strlen(m+l) == 2 && lookup_cc(m+l, &cc))) { emit32((cc << 28) | 0x0320f000u | hn[i].h); return; }
			} }
	}
	if (!strcmp(m, "dmb")) { enc_barrier(0xf57ff050u); return; }
	if (!strcmp(m, "dsb")) { enc_barrier(0xf57ff040u); return; }
	if (!strcmp(m, "isb")) { enc_barrier(0xf57ff060u); return; }
	if (!strncmp(m, "mrs", 3)) { if (!suffix_c(m + 3, &cond)) die("%s: bad suffix", m); enc_mrs(cond); return; }
	if (!strncmp(m, "msr", 3)) { if (!suffix_c(m + 3, &cond)) die("%s: bad suffix", m); enc_msr(cond); return; }
	if (!strcmp(m, "cpsid")) { enc_cps(0xf10c0000u); return; }
	if (!strcmp(m, "cpsie")) { enc_cps(0xf1080000u); return; }
	if (!strcmp(m, "cps"))   { enc_cps(0xf1000000u); return; }
	if (!strncmp(m, "mcr", 3) && m[3] != 'r') { if (!suffix_c(m + 3, &cond)) die("%s: bad suffix", m); enc_mcr(cond, 0); return; }
	if (!strncmp(m, "mrc", 3) && m[3] != 'r') { if (!suffix_c(m + 3, &cond)) die("%s: bad suffix", m); enc_mcr(cond, 1u << 20); return; }
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


int md_directive(char **t, int n) {
	/* ARM/ABI metadata we accept and don't (yet) encode: build attributes, arch/cpu/fpu selection (we are fixed
	 * ARMv7-A), unwind annotations, file/ident. Explicit list — was a substring match, so `.thumb` (or anything
	 * containing "arm"/"save"/"file") was silently ignored and Thumb code got assembled as ARM. */
	static const char *const ok[] = { ".arch", ".arch_extension", ".cpu", ".fpu", ".object_arch", ".eabi_attribute",
		".syntax", ".arm", ".fnstart", ".fnend", ".cantunwind", ".save", ".vsave", ".setfp", ".pad", ".movsp",
		".personality", ".personalityindex", ".handlerdata", ".unwind_raw", ".file", ".ident", ".loc", ".cfi_sections", NULL };
	const char *d = t[0];
	if (!strcmp(d, ".thumb") || !strcmp(d, ".thumb_func") || !strcmp(d, ".force_thumb") || !strcmp(d, ".thumb_set")
	    || (!strcmp(d, ".code") && n >= 2 && !strcmp(t[1], "16")))
		die("%s: Thumb is not supported (ARM state only)", d);
	if (!strcmp(d, ".code")) { if (n < 2 || strcmp(t[1], "32")) die(".code: expected 32"); return 1; }
	if (!strcmp(d, ".ltorg") || !strcmp(d, ".pool")) { pool_flush(cursec); return 1; }
	if (!strcmp(d, ".cpu") || !strcmp(d, ".arch") || !strcmp(d, ".arch_extension") || !strcmp(d, ".fpu") || !strcmp(d, ".eabi_attribute")) { attr_directive(t, n); return 1; }
	for (int i = 0; ok[i]; i++) if (!strcmp(d, ok[i])) return 1;
	return 0;
}

/* End of pass: resolve pc-relative ldr literals (target label now defined). offset12 = (label+addend) -
 * (ldr_addr + 8); the sign picks the U bit. Both the ldr and its pool are in the same section, so this
 * is an assembler-internal fixup (no relocation) — exactly what GNU as does for a local literal load. */
int md_is_branch_reloc(u32 type) { return type == R_ARM_CALL || type == R_ARM_JUMP24; }
void md_finish(void) {
	for (int i = 0; i < nldrlit; i++) {
		int si = sym_find(ldrlit[i].sym);
		if (si < 0 || !syms[si].defined) die("ldr literal: undefined symbol '%s'", ldrlit[i].sym);
		if (syms[si].sec != ldrlit[i].sec) die("ldr literal '%s': target in a different section", ldrlit[i].sym);
		int32_t target = (int32_t)syms[si].value + (int32_t)ldrlit[i].addend;
		int32_t delta = target - (int32_t)(ldrlit[i].off + 8);
		u32 mag = (u32)(delta < 0 ? -delta : delta);
		if (ldrlit[i].kind == 1) { patch_adr(ldrlit[i].sec, ldrlit[i].off, delta); }   /* adr Rd, named-label */
		else {
			u32 w = read32(ldrlit[i].sec, ldrlit[i].off);
			if (mag > 0xfff) die("ldr literal '%s': offset %d out of +/-4095 range", ldrlit[i].sym, delta);
			w = (w & ~((1u << 23) | 0xfffu)) | ((delta >= 0 ? 1u : 0u) << 23) | mag;   /* set U + offset12 */
			patch32(ldrlit[i].sec, ldrlit[i].off, w);
		}
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
			/* R_ARM_CALL only for an UNCONDITIONAL bl: the linker may turn a CALL into blx (interworking), which can't
			 * be conditional — so `blne ext` is R_ARM_JUMP24, as GAS emits (was CALL: a conditional call could become
			 * an unconditional blx at link time) */
			int uncond = (base >> 28) == 0xe;
			add_reloc(brfix[i].sec, brfix[i].off, sym_intern(brfix[i].sym), brfix[i].is_bl && uncond ? R_ARM_CALL : R_ARM_JUMP24);
		}
	}
}
