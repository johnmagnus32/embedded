@ m1_dp.s — data-processing coverage: mov (reg) + bic (#imm), incl. a rotated modified-immediate.
	.text
	.global dp
	.type dp, %function
dp:
	mov	r0, r1
	mov	r3, sp
	mov	sp, lr
	bic	r2, r2, #7
	bic	r0, r1, #0xff00     @ needs rotation: ror(0xff, 24) -> rot=12,imm8=0xff
	.size dp, . - dp
