@ Relocation ORDER (GAS write_relocs): fixups per subsection, in creation order, subsections in number order;
@ .reloc entries merged in before the first fixup at a higher address. Plus group relocations and PREL movw/movt.
	.syntax unified
	.text
	.globl ext
f:	bl ext
	.subsection 1
	bl ext2
	.word ext3
	.subsection 0
	.reloc ., R_ARM_NONE, .
	.reloc 0, R_ARM_NONE, ext4
	b ext5
	.set .Lpc, . + 8
	movw r0, #:lower16:ext6 - .Lpc
	movt r0, #:upper16:ext6 - .Lpc
	add r0, r0, pc
	.reloc .L0, R_ARM_ALU_PC_G0_NC, ext7
	.reloc .L1, R_ARM_ALU_PC_G1_NC, ext7
	.reloc .L2, R_ARM_LDR_PC_G2, ext7
	add r1, r1, pc
.L0:	sub r1, r1, #4
.L1:	sub r1, r1, #0
.L2:	ldr r1, [r1, #4]
	.section .rodata.str,"aMS",%progbits,1
.LC0:	.asciz "abc"
	.text
	.word .LC0, .LC0 + 1     @ merge section: the symbol stays only with a non-zero addend
