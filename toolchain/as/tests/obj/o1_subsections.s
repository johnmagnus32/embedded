@ Subsections the way GAS lays them out (subsegs_finish + check_mapping_symbols): each subsection's frag chain is
@ appended in number order; its end puts the section back in its natural state (a $a/$d there); a code section's
@ last subsection pads to min(4, its alignment) — zeros + $d under a word; an SHF_MERGE one pads each to entsize;
@ entering ARM from UNDEF in a later subsection puts a $d at 0; a leading .align's own symbol marks the padding.
	.syntax unified
	.text
	.word 7                  @ UNDEF: deferred
	.subsection 1
	nop                      @ $d@0 (section start) + $a here
	.subsection 0
	.word 8
	.subsection 2
	.align 5                 @ lands after sub 1: padded where it lands
	add r0, r0, #1
	.long 1f - .
1:	.subsection 3
	.align 2
	.word 5                  @ the align's $a marks the pad, the $d the word after it
	.subsection 0
	.byte 1                  @ code section ends unaligned: end pad
	.previous
	.byte 2
	.data
	.subsection 1
	.word 1
	.subsection 0
	.word 2
	.section .rodata.m,"aM",%progbits,4
	.byte 1
	.subsection 1
	.byte 2
	.section .text.b,"ax"
	nop
	.balign 2                @ code alignment doesn't impose an instruction's 4
	.byte 1, 2, 3
	.section .text.c,"ax"
	nop
	.align 12                @ over 63 bytes: GAS NOP-fills only the low 6 bits' worth
	nop
	.balign 128
	.byte 9
