@ m2_branch.s — conditional branches (b/bl with cond), bx, and bx<cond>. Named targets are EXTERNAL
@ (so GNU as leaves the same placeholder+reloc we do); conditional branches use local labels.
	.text
	.global m2b
m2b:
	bl    ext          @ external -> R_ARM_CALL
	blne  ext          @ external conditional link
1:	beq   1b           @ backward local
	bcs   1b
	bne   2f           @ forward local
	ble   2f
	b     1b
	bx    lr
2:	bxeq  lr
	mov   r0, r0
