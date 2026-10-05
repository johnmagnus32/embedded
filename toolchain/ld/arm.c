/*
 * arm.c — the MACHINE-DEPENDENT linker backend for ARMv7 (ELF32). THE only architecture-specific file:
 * it knows the ELF machine id and how to apply each relocation type's encoding. The front-end resolves
 * the symbol address (S) and the patch address (P) and hands them here; we compute the field + patch.
 * Another CPU is a sibling of this file (its own md_e_machine + md_apply_reloc), front-end/obj unchanged.
 *
 * Relocation math (ARM uses REL — the addend A lives in the instruction we're patching):
 *   R_ARM_ABS32           word = S + A            (A = the existing 32-bit word)
 *   R_ARM_REL32           word = S + A - P        (`.word sym - .`)
 *   R_ARM_CALL/JUMP24     imm24 = (S + A - P) >> 2 (A = sign-extended imm24 * 4; P = the bl/b address)
 *   R_ARM_PREL31          31 bits = S + A - P, bit 31 kept   (.ARM.exidx: the offset to the function)
 *   R_ARM_TARGET1         as R_ARM_ABS32 (the Linux/EABI choice; GCC's .init_array entries use it)
 *   R_ARM_MOVW/MOVT_*     imm16 = lower/upper 16 bits of S + A (- P for _PREL); A = the imm16 field, SIGNED (AAELF:
 *                         the same 16-bit addend in both halves of a movw/movt pair)
 */
#include <stdint.h>
#include "ld.h"

const u16 md_e_machine = EM_ARM;
const u32 md_r_relative  = 23;     /* R_ARM_RELATIVE: runtime `*P += load_bias` — the only dynamic reloc a PIE emits */
const u32 md_r_jump_slot = 22;     /* R_ARM_JUMP_SLOT: loader writes the resolved fn address into a PLT GOT slot */
const u32 md_r_copy      = 20;     /* R_ARM_COPY: loader memcpy's an imported variable into the exe's .dynbss slot */

#define R_ARM_NONE     0
#define R_ARM_V4BX     40   /* marks a BX for --fix-v4bx (ARMv4 has no BX); without it, nothing to do */
#define R_ARM_ABS32    2
#define R_ARM_REL32    3
#define R_ARM_MOVW_ABS_NC 43
#define R_ARM_MOVT_ABS    44
#define R_ARM_MOVW_PREL_NC 45
#define R_ARM_MOVT_PREL    46
#define R_ARM_CALL     28
#define R_ARM_JUMP24   29
#define R_ARM_TLS_GD32  104   /* GOT(S) + A - P: a (module, offset) GOT pair for __tls_get_addr */
#define R_ARM_TLS_LDM32 105   /* GOT(module) + A - P: this module's (module, 0) pair                 */
#define R_ARM_TLS_LDO32 106   /* S + A - TLS: the offset inside the module's TLS block               */
#define R_ARM_TLS_IE32  107   /* GOT(S) + A - P: a GOT slot holding S's offset from the thread pointer */
#define R_ARM_TLS_LE32  108   /* S + A - TP: S's offset from the thread pointer (executables only)    */
#define R_ARM_TARGET1  38
#define R_ARM_PREL31   42
#define R_ARM_GOT_PREL 96

const u32 md_r_tls_dtpmod32 = 17, md_r_tls_dtpoff32 = 18, md_r_tls_tpoff32 = 19;   /* dynamic TLS relocations */
const u32 md_tcb_size = 8;      /* ARM (TLS variant I): the thread pointer addresses an 8-byte TCB; the program's block follows */
int md_tls_kind(u32 type) {
	switch (type) {
	case R_ARM_TLS_GD32: return TLS_GD; case R_ARM_TLS_LDM32: return TLS_LDM; case R_ARM_TLS_LDO32: return TLS_LDO;
	case R_ARM_TLS_IE32: return TLS_IE; case R_ARM_TLS_LE32: return TLS_LE; default: return TLS_NONE;
	}
}
const u32 md_r_abs32    = R_ARM_ABS32;    /* a symbolic word: the loader stores S + A (an import's address in a .so) */
const u32 md_r_glob_dat = 21;               /* R_ARM_GLOB_DAT: loader writes an imported symbol's addr into its GOT slot */

/* The PLT: one eager-binding stub per called import, PC-RELATIVE so it works at any load bias: it adds a
 * link-time-constant offset to pc to reach its GOT slot, then jumps through it (the loader's JUMP_SLOT fills the
 * slot). The offset is a difference of two in-image addresses — no relocation, never writes .text.
 *   ldr ip, [pc, #4]   ; ip = the .word below
 *   add ip, pc, ip     ; ip = &slot
 *   ldr pc, [ip]       ; jump to *slot
 *   .word slot - (stub + 12)   (pc at the add = stub + 4 + 8) */
const u32 md_plt_entsize = 16;
void md_plt_entry(u8 *p, u32 stub, u32 slot) {
	wr32(p, 0xe59fc004u); wr32(p + 4, 0xe08fc00cu); wr32(p + 8, 0xe59cf000u); wr32(p + 12, slot - (stub + 12));
}

/* In a PIE, an absolute reference (ABS32) is the only thing whose value depends on where we load: it
 * must become a runtime R_ARM_RELATIVE base-fixup. PC-relative branches (CALL/JUMP24) are bias-invariant
 * — the linker resolves them statically — so they need no dynamic reloc. */
int md_needs_dynamic_reloc(u32 type) { return type == R_ARM_ABS32 || type == R_ARM_TARGET1; }
/* An absolute reference that CAN'T carry a dynamic fixup (movw/movt halves): position-dependent code. */
int md_is_abs_nonword(u32 type) { return type == R_ARM_MOVW_ABS_NC || type == R_ARM_MOVT_ABS; }
/* Does this reloc, against an UNDEFINED symbol, denote a CALL that should be routed through a PLT stub? */
int md_is_call_reloc(u32 type) { return type == R_ARM_CALL || type == R_ARM_JUMP24; }
/* Is this a PIC GOT-entry reference? The front-end resolves it to (GOT_slot - P) — S is the slot addr. */
int md_is_got_reloc(u32 type) { return type == R_ARM_GOT_PREL; }
int md_is_marker(u32 type) { return type == R_ARM_NONE || type == R_ARM_V4BX; }

/* ARM group relocations (AAELF 4.6.1.4): X = S + A - P split into G0, G1, G2 — each the top 8 significant bits (at an
 * even shift) of what the earlier groups left — for an ADD/SUB immediate (ALU_PC_Gn), then the remainder as an LDR /
 * LDRS / LDC offset (LDR_PC_Gn: what G0..G(n-1) leave). REL addends come from the instruction: the rotated immediate,
 * negative for a SUB; the offset, negative without the U bit. As bfd's elf32_arm_final_link_relocate. */
static u32 group_mask(u32 value, int n, u32 *residual) {   /* bfd's calculate_group_reloc_mask */
	u32 enc = 0, res = value;
	for (int k = 0; k <= n; k++) {
		int shift = 0;
		if (res) { int msb; for (msb = 30; msb >= 0; msb -= 2) if (res & (3u << msb)) break; shift = msb - 6 < 0 ? 0 : msb - 6; }
		u32 g = res & (0xffu << shift);
		enc = (g >> shift) | ((g <= 0xff ? 0 : (u32)(32 - shift) / 2) << 8);
		res &= ~g;
	}
	*residual = res; return enc;
}
static void alu_group(Obj *o, u8 *loc, u32 w, u32 S, u32 P, int n, int check) {
	u32 rot = (w >> 8 & 0xf) * 2, c = w & 0xff, imm = rot ? (c >> rot | c << (32 - rot)) : c;
	u32 op = w & 0x1e00000;
	if (op != 1u << 23 && op != 1u << 22) die("%s: an ALU group relocation on an instruction that is neither ADD nor SUB (%#x)", o->path, w);
	s32 X = (s32)(S - P + (op == 1u << 22 ? -imm : imm)); u32 res;
	u32 g = group_mask(X < 0 ? (u32)-X : (u32)X, n, &res);
	if (check && res) die("%s: %#x doesn't split into ALU group G%d (residual %#x)", o->path, (u32)X, n, res);
	wr32(loc, (w & 0xff1ff000u) | (X < 0 ? 1u << 22 : 1u << 23) | g);
}
static void ldr_group(Obj *o, u8 *loc, u32 w, u32 S, u32 P, int n, int kind) {   /* kind: 0 LDR imm12, 1 LDRS imm4:imm4, 2 LDC imm8*4 */
	u32 a = kind == 0 ? w & 0xfff : kind == 1 ? (w >> 4 & 0xf0) | (w & 0xf) : (w & 0xff) << 2;
	s32 X = (s32)(S - P + (w & 1u << 23 ? a : -a)); u32 res;
	group_mask(X < 0 ? (u32)-X : (u32)X, n - 1, &res);
	u32 lim = kind == 0 ? 0x1000 : kind == 1 ? 0x100 : 0x400;
	if (res >= lim || (kind == 2 && (res & 3))) die("%s: %#x doesn't fit a group relocation's G%d load offset (residual %#x)", o->path, (u32)X, n, res);
	u32 u = X >= 0 ? 1u << 23 : 0;
	if (kind == 0) wr32(loc, (w & 0xff7ff000u) | u | res);
	else if (kind == 1) wr32(loc, (w & 0xff7ff0f0u) | u | (res & 0xf0) << 4 | (res & 0xf));
	else wr32(loc, (w & 0xff7fff00u) | u | res >> 2);
}
static s32 movw_addend(u32 w) { return (int16_t)(((w >> 4) & 0xf000u) | (w & 0xfffu)); }   /* imm4:imm12, signed */
static u32 movw_set(u32 w, u32 v) { return (w & ~0x000f0fffu) | (((v >> 12) & 0xfu) << 16) | (v & 0xfffu); }
void md_apply_reloc(Obj *o, u32 type, u8 *loc, u32 S, u32 P) {
	u32 w = rd32(loc);
	switch (type) {
	case R_ARM_NONE:        /* a dependency marker only (e.g. .ARM.exidx -> its personality routine) */
	case R_ARM_V4BX:        /* (GNU ld without --fix-v4bx: the BX stays) */
		break;
	case R_ARM_ABS32:
	case R_ARM_TARGET1:
		wr32(loc, S + w);   /* A is the existing word */
		break;
	case R_ARM_REL32:       /* `.word sym - .` (kernel .alt.smp.init / ex_table-style PC-relative entries) */
		wr32(loc, S + w - P);
		break;
	case R_ARM_CALL:
	case R_ARM_JUMP24: {
		s32 A = (s32)(w << 8) >> 6;              /* sign-extend the 24-bit field, then *4 */
		s32 X = (s32)S + A - (s32)P;
		if (X < -0x2000000 || X > 0x1fffffc) die("%s: branch to %#x out of reach (+-32MB) at %#x (and no veneer)", o->path, S + A + 8, P);
		if (X & 3) die("%s: branch at %#x to Thumb code (%#x): BLX conversion is unsupported", o->path, P, S + A + 8);
		wr32(loc, (w & 0xff000000u) | ((X >> 2) & 0x00ffffffu));
		break;
	}
	case 57: case 58: alu_group(o, loc, w, S, P, 0, type == 58); break;   /* R_ARM_ALU_PC_G0_NC, R_ARM_ALU_PC_G0 */
	case 59: case 60: alu_group(o, loc, w, S, P, 1, type == 60); break;   /* R_ARM_ALU_PC_G1_NC, R_ARM_ALU_PC_G1 */
	case 61:          alu_group(o, loc, w, S, P, 2, 1); break;             /* R_ARM_ALU_PC_G2 */
	case 4: case 62: case 63: ldr_group(o, loc, w, S, P, type == 4 ? 0 : type - 61, 0); break;   /* R_ARM_LDR_PC_G0..G2 */
	case 64: case 65: case 66: ldr_group(o, loc, w, S, P, type - 64, 1); break;                  /* R_ARM_LDRS_PC_G0..G2 */
	case 67: case 68: case 69: ldr_group(o, loc, w, S, P, type - 67, 2); break;                  /* R_ARM_LDC_PC_G0..G2 */
	case R_ARM_MOVW_ABS_NC:  wr32(loc, movw_set(w, (S + (u32)movw_addend(w)) & 0xffffu)); break;          /* lower16(S + A) */
	case R_ARM_MOVT_ABS:     wr32(loc, movw_set(w, (S + (u32)movw_addend(w)) >> 16)); break;              /* upper16(S + A) */
	case R_ARM_MOVW_PREL_NC: wr32(loc, movw_set(w, (S + (u32)movw_addend(w) - P) & 0xffffu)); break;      /* lower16(S + A - P) */
	case R_ARM_MOVT_PREL:    wr32(loc, movw_set(w, (S + (u32)movw_addend(w) - P) >> 16)); break;          /* upper16(S + A - P) */
	case R_ARM_TLS_LE32:    /* S = the thread-pointer offset / the module offset (the front-end computes it) */
	case R_ARM_TLS_LDO32:
		wr32(loc, S + w);
		break;
	case R_ARM_TLS_GD32:    /* S = the GOT slot (pair); PC-relative like R_ARM_GOT_PREL */
	case R_ARM_TLS_LDM32:
	case R_ARM_TLS_IE32:
		wr32(loc, S + w - P);
		break;
	case R_ARM_PREL31: {
		s32 A = (s32)(w << 1) >> 1;              /* sign-extend the 31-bit field */
		wr32(loc, (w & 0x80000000u) | ((S + (u32)A - P) & 0x7fffffffu));
		break;
	}
	case R_ARM_GOT_PREL:                             /* S = the symbol's GOT slot; store its PC-relative offset */
		wr32(loc, (u32)((s32)S + (s32)w - (s32)P));  /* the `add rN,pc,rN` then yields the slot address */
		break;
	default:
		die("%s: unsupported relocation type %u", o->path, type);
	}
}

/* A relocatable link (-r) moves input sections into merged output sections: a relocation against an input
 * SECTION symbol becomes one against the output section's, so its addend grows by where that input section
 * landed (delta). ARM's addends live in the patched field itself, each type its own encoding. 0 = this type's
 * addend can't be moved here (the caller fails loudly). */
int md_rel_adjust(u32 type, u8 *loc, u32 delta) {
	u32 w = rd32(loc);
	switch (type) {
	case R_ARM_NONE: case R_ARM_V4BX: return 1;
	case R_ARM_ABS32: case R_ARM_TARGET1: case R_ARM_REL32: case R_ARM_GOT_PREL:
	case R_ARM_TLS_LE32: case R_ARM_TLS_LDO32: case R_ARM_TLS_GD32: case R_ARM_TLS_LDM32: case R_ARM_TLS_IE32:
		wr32(loc, w + delta); return 1;
	case R_ARM_PREL31: {
		s32 A = (s32)(w << 1) >> 1; int64_t n = (int64_t)A + delta;
		if (n < -(1LL << 30) || n >= (1LL << 30)) return 0;
		wr32(loc, (w & 0x80000000u) | ((u32)n & 0x7fffffffu)); return 1; }
	case R_ARM_CALL: case R_ARM_JUMP24: {
		s32 A = (s32)(w << 8) >> 6; int64_t n = (int64_t)A + delta;
		if (n & 3 || n < -(1LL << 25) || n >= (1LL << 25)) return 0;
		wr32(loc, (w & 0xff000000u) | ((u32)(n >> 2) & 0x00ffffffu)); return 1; }
	case R_ARM_MOVW_ABS_NC: case R_ARM_MOVT_ABS: case R_ARM_MOVW_PREL_NC: case R_ARM_MOVT_PREL: {
		int64_t n = (int64_t)movw_addend(w) + delta;
		if (n < -32768 || n > 32767) return 0;   /* (a REL movw/movt addend is a signed 16 bits) */
		wr32(loc, movw_set(w, (u32)n & 0xffffu)); return 1; }
	default: return 0;
	}
}
