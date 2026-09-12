@ m2_wide.s — shifts + shifted operand2, movw/movt, mul/mla/mls/udiv/clz, ldrd/strd/ldrh/strh, ldm/stm, svc, blx.
	.text
	.global m2c
m2c:
	add   r2, r1, r2, lsl #2
	orr   r3, r3, ip, lsl r1
	lsl   r0, r1, #2
	lsr   r5, r5, r1
	asr   r0, r0, #31
	movw  r0, #12345
	movt  r0, #4660
	mul   r0, r0, r1
	mla   r0, r1, r2, r0
	udiv  r0, r5, r0
	clz   r0, r0
	ldrd  r0, [r2]
	ldrd  r2, [r2, #16]
	strd  r4, [r0]
	ldrh  r3, [r0]
	strh  r3, [r0, #2]
	ldm   r3, {r3, r4, r5}
	stm   r8, {r3, r9}
	svc   0
	blx   r9
	bx    lr
	mls   r0, r1, r2, r3
	pusheq {r4, lr}
	popcs  {r4, pc}
	pushne {r0-r3}
