@ m2_ldst.s — single data transfer (ldr/str/ldrb/strb) across all addressing modes + uxtb.
	.text
	.global m2ls
m2ls:
	ldrb  ip, [r0]
	ldrb  ip, [r0], #1
	ldrb  ip, [r1, #1]!
	ldrb  ip, [r1], #1
	ldrb  ip, [r1, r3]
	ldrb  r3, [r3, #-1]!
	ldr   r2, [r3, #8]
	ldr   r2, [r0, #-16]
	ldr   r5, [r7]
	str   r3, [r5]
	str   r2, [r0, #-16]
	str   r2, [r5, r0]
	strb  ip, [r0, r3]
	strb  ip, [r2, #-1]!
	streq r0, [r7]
	strne r0, [r6, #4]
	ldrls r0, [r3, r0, lsl #2]
	uxtb  r1, r1
	uxtb  ip, r1
