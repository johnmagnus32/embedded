@ m1_branch.s — branch coverage: bl (external->R_ARM_CALL), b (forward + backward local labels).
	.text
	.global br
	.type br, %function
br:
	bl	ext_func            @ external -> relocation
1:	b	2f                  @ forward local
	mov	r0, r0
	b	1b                  @ backward local
2:	b	2b                  @ self loop
	.size br, . - br
