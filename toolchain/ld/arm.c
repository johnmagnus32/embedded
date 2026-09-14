/*
 * arm.c — the MACHINE-DEPENDENT linker backend for ARMv7 (ELF32). THE only architecture-specific file:
 * it knows the ELF machine id and how to apply each relocation type's encoding. The front-end resolves
 * the symbol address (S) and the patch address (P) and hands them here; we compute the field + patch.
 * Another CPU is a sibling of this file (its own md_e_machine + md_apply_reloc), front-end/obj unchanged.
 *
 * Relocation math (ARM uses REL — the addend A lives in the instruction we're patching):
 *   R_ARM_ABS32           word = S + A            (A = the existing 32-bit word)
 *   R_ARM_CALL/JUMP24     imm24 = (S + A - P) >> 2 (A = sign-extended imm24 * 4; P = the bl/b address)
 */
#include "ld.h"

const u16 md_e_machine = EM_ARM;
const u32 md_r_relative = 23;      /* R_ARM_RELATIVE: runtime `*P += load_bias` — the only dynamic reloc a PIE emits */

#define R_ARM_ABS32  2
#define R_ARM_CALL   28
#define R_ARM_JUMP24 29

/* In a PIE, an absolute reference (ABS32) is the only thing whose value depends on where we load: it
 * must become a runtime R_ARM_RELATIVE base-fixup. PC-relative branches (CALL/JUMP24) are bias-invariant
 * — the linker resolves them statically — so they need no dynamic reloc. */
int md_needs_dynamic_reloc(u32 type) { return type == R_ARM_ABS32; }

void md_apply_reloc(Obj *o, u32 type, u8 *loc, u32 S, u32 P) {
	u32 w = rd32(loc);
	switch (type) {
	case R_ARM_ABS32:
		wr32(loc, S + w);   /* A is the existing word */
		break;
	case R_ARM_CALL:
	case R_ARM_JUMP24: {
		s32 A = (s32)(w << 8) >> 6;              /* sign-extend the 24-bit field, then *4 */
		s32 X = (s32)S + A - (s32)P;
		wr32(loc, (w & 0xff000000u) | ((X >> 2) & 0x00ffffffu));
		break;
	}
	default:
		die("%s: unsupported relocation type %u", o->path, type);
	}
}
