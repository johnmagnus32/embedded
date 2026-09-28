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
 */
#include "ld.h"

const u16 md_e_machine = EM_ARM;
const u32 md_r_relative  = 23;     /* R_ARM_RELATIVE: runtime `*P += load_bias` — the only dynamic reloc a PIE emits */
const u32 md_r_jump_slot = 22;     /* R_ARM_JUMP_SLOT: loader writes the resolved fn address into a PLT GOT slot */
const u32 md_r_copy      = 20;     /* R_ARM_COPY: loader memcpy's an imported variable into the exe's .dynbss slot */

#define R_ARM_NONE     0
#define R_ARM_ABS32    2
#define R_ARM_REL32    3
#define R_ARM_MOVW_ABS_NC 43
#define R_ARM_MOVT_ABS    44
#define R_ARM_CALL     28
#define R_ARM_JUMP24   29
#define R_ARM_TARGET1  38
#define R_ARM_PREL31   42
#define R_ARM_GOT_PREL 96

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
int md_is_marker(u32 type) { return type == R_ARM_NONE; }

void md_apply_reloc(Obj *o, u32 type, u8 *loc, u32 S, u32 P) {
	u32 w = rd32(loc);
	switch (type) {
	case R_ARM_NONE:        /* a dependency marker only (e.g. .ARM.exidx -> its personality routine) */
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
		wr32(loc, (w & 0xff000000u) | ((X >> 2) & 0x00ffffffu));
		break;
	}
	case R_ARM_MOVW_ABS_NC: { u32 v = S & 0xffffu;         wr32(loc, (w & ~0x000f0fffu) | ((v >> 12) << 16) | (v & 0xfff)); break; }   /* imm16 = lower16(S) */
	case R_ARM_MOVT_ABS:    { u32 v = (S >> 16) & 0xffffu; wr32(loc, (w & ~0x000f0fffu) | ((v >> 12) << 16) | (v & 0xfff)); break; }   /* imm16 = upper16(S) */
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
