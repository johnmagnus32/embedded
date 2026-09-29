@ Thumb: .thumb/.arm/.code switches (back to ARM word-aligns with a $d frag), $t mapping symbols, Thumb NOP padding
@ (narrow then nop.w), a FUNCTION defined in Thumb code gets bit 0 in the symbol table (.type either side, or
@ .thumb_func), relocations against it keep the symbol, Thumb .inst sizes, and the kernel's sigreturn trampolines.
	.syntax unified
	.text
	.thumb
	.type f, %function
f:	movs r0, #1
	.thumb_func
g:	bx lr
h:	nop
	.type h, %function
	.global k
	.thumb_func
k:	nop
.L1:	nop
	.align 3
	movs r1, r2
	.arm
	nop
	.code 16
	nop
	.balign 16
	.code 32
	.word 5
	.thumb
	nop
	.inst 0x4770
	.inst 0xf3af8000
	.inst.n 0xbf00
	.inst.w 0x12345678
	.word g, k, f
	.byte 1
	.section .rodata
	.align
x:	.arm
	mov r7, #119
	swi #119 | 0x900000
	.org x + 8
	.thumb
	movs r7, #119
	swi #0
	.org x + 24
	.arm
	ldr r3, [sp, #756]
	ldmia r3, {r3, r9}
	bx r3
	.org x + 36
	.thumb
	ldr r3, [sp, #756]
	ldmia r3, {r2, r3}
	mov r9, r3
	bx r2
	.space 4
