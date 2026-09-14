@ crt_pie.s — self-relocating startup for a -pie (ET_DYN) image loaded at a nonzero bias.
@
@ A PIE is linked at base 0, so every absolute address baked into it (e.g. `int *p = &arr;`) holds a
@ LINK-TIME value that is wrong once the loader drops the image somewhere else. `ld -pie` left an
@ R_ARM_RELATIVE in .rel.dyn for each such word; this crt is the "loader": it computes the load bias,
@ walks the .dynamic array to find that relocation table, adds the bias to every word it names, and only
@ then calls main(). (On qemu -M virt with the MMU off the R-X segment is writable, so patching in place
@ is fine; a real OS would place these words in a writable .data/.got.)
.text
.global _start
_start:
	mov   r4, pc              @ ARM pipeline: PC reads as &(this insn) + 8
	sub   r4, r4, #8          @ r4 = runtime &_start
	ldr   r5, .Llink_start    @ r5 = link-time &_start (this word is itself an R_ARM_RELATIVE target, so it
	                          @      still holds the pre-relocation link-time value when we read it here)
	sub   r6, r4, r5          @ r6 = load bias = runtime - link

	ldr   r7, .Llink_dynamic  @ r7 = link-time &_DYNAMIC
	add   r7, r7, r6          @ r7 = runtime &_DYNAMIC

	@ --- scan .dynamic for DT_REL (rel-table vaddr) and DT_RELSZ (size in bytes) ---
	mov   r8, #0              @ r8 = rel-table link-time vaddr
	mov   r9, #0              @ r9 = rel-table size (bytes)
.Lscan:
	ldr   r0, [r7]            @ d_tag
	ldr   r1, [r7, #4]        @ d_val
	add   r7, r7, #8
	cmp   r0, #0              @ DT_NULL -> end of array
	beq   .Lscandone
	cmp   r0, #17             @ DT_REL
	moveq r8, r1
	cmp   r0, #18             @ DT_RELSZ
	moveq r9, r1
	b     .Lscan
.Lscandone:
	add   r8, r8, r6          @ r8 = runtime &rel[0]
	add   r9, r8, r9          @ r9 = runtime end-of-table

	@ --- apply each R_ARM_RELATIVE: *(bias + r_offset) += bias ---
.Lrel:
	cmp   r8, r9
	bcs   .Lreldone           @ r8 >= r9 (unsigned) -> done
	ldr   r0, [r8]            @ r_offset (link-time vaddr of the word to fix)
	ldr   r1, [r8, #4]        @ r_info
	add   r8, r8, #8
	and   r1, r1, #0xff       @ reloc type = low byte of r_info
	cmp   r1, #23             @ R_ARM_RELATIVE
	bne   .Lrel               @ (M1 emits only RELATIVE; skip anything else defensively)
	add   r2, r0, r6          @ runtime address of the target word
	ldr   r3, [r2]            @ its current (link-time) value
	add   r3, r3, r6          @ add the bias
	str   r3, [r2]
	b     .Lrel
.Lreldone:
	@ --- image is now self-consistent: hand off to C ---
	movw  sp, #0x0000
	movt  sp, #0x4090         @ sp = 0x40900000 (well above the loaded image)
	bl    main                @ result -> r0
	mov   r1, r0              @ exit code
	movw  r2, #0x0026
	movt  r2, #0x0002         @ r2 = 0x20026 = ADP_Stopped_ApplicationExit
	push  {r1}                @ block[1] = exit code
	push  {r2}                @ block[0] = reason (lower address = sp)
	mov   r1, sp              @ r1 -> {reason, code}
	mov   r0, #0x20           @ SYS_EXIT_EXTENDED
	svc   0x123456
.Lhang:
	b     .Lhang

.Llink_start:    .word _start
.Llink_dynamic:  .word _DYNAMIC
