@ m2_dp.s — the data-processing family: 3-op / mov / compare forms, #imm + register operand2,
@ condition-code suffixes, and the S-bit. All must encode byte-identically to GNU as.
	.text
	.global m2dp
m2dp:
	add   r0, r1, r2
	add   r0, r1, #8
	sub   r3, r4, #0xff
	rsb   r0, r1, #0
	and   r5, r5, #0x0f
	orr   r6, r6, r7
	eor   r8, r8, #1
	adc   r0, r1, r2
	sbc   r0, r1, r2
	mov   r0, #0x1000
	mov   r1, r2
	mvn   r3, #0
	cmp   r0, #0
	cmp   r0, r1
	cmn   r0, #1
	tst   r0, #0x80
	teq   r0, r1
	adds  r0, r0, #1
	subs  r1, r1, #1
	movs  r2, r3
	addne r0, r0, #4
	moveq r1, #0
	movls r2, r3
	bichi r4, r4, #1
