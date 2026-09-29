@ The 16-bit Thumb encodings, one of each form (gastest/thumb_diff.py sweeps the full operand space).
	.syntax unified
	.text
	.thumb
	adds r0, r1, r2
	adds r3, r3, #200
	adds r3, r4, #7
	subs r0, r1, r2
	subs r5, #255
	add r8, r8, r1
	add r0, sp, #1020
	add sp, sp, #508
	sub sp, #4
	add r2, pc, #8
	ands r0, r1
	eors r2, r2, r3
	orrs r4, r5, r4
	bics r6, r7
	adcs r0, r0, r1
	sbcs r1, r2
	lsls r0, r1, #31
	lsrs r2, r3, #32
	asrs r4, r5, #0
	rors r6, r7
	lsls r1, r1, r2
	movs r0, #255
	movs r1, r2
	movs r3, r4, lsr #3
	mov r8, sp
	mov pc, lr
	cmp r0, #0
	cmp r1, r2
	cmp r8, r1
	cmn r3, r4
	tst r5, r6
	mvns r0, r1
	muls r2, r3, r2
	negs r4, r5
	rsbs r6, r7, #0
	ldr r0, [r1, #124]
	str r2, [sp, #1020]
	ldr r3, [pc, #1020]
	ldrb r4, [r5, #31]
	strh r6, [r7, #62]
	ldrsh r0, [r1, r2]
	ldrsb r3, [r4, r5]
	str r6, [r7, r0]
	push {r0-r7, lr}
	pop {r4, pc}
	stmia r0!, {r1, r2}
	ldmia r3, {r2, r3}
	ldmia r4!, {r0, r1}
	ldm r5, {r0}
	ldmia sp!, {r0, r1}
	bx r3
	blx lr
	svc #255
	bkpt
	udf #7
	nop
	yield
	wfe
	wfi
	sev
	sxth r0, r1
	uxtb r2, r3, ror #0
	rev r4, r5
	revsh r6, r7
	cpsid i
	cpsie aif
	setend be
	movs.n r0, #1
