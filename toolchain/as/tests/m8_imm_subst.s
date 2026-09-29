@ m8_imm_subst.s — data-processing immediates as GNU as takes them: a value that isn't a modified immediate becomes
@ the partner instruction with its complement (mov/mvn, and/bic, adc/sbc) or negation (add/sub, cmp/cmn) — S and cond
@ kept — and mov #imm16 becomes movw; the '#' is optional (musl's vfork.s `mov r7, 190`, clone.s `and r1,r1,#-16`).
	.syntax unified
	.arm
	.text
	mov r0, #-2
	mvn r0, #-2
	and r0, r1, #-16
	bic r0, r1, #-16
	add r0, r1, #-4
	sub r0, r1, #-4
	adc r0, r1, #-2
	sbc r0, r1, #-2
	cmp r0, #-1
	cmn r0, #-1
	movs r0, #-2
	andseq r0, r1, #-16
	addne r2, r3, #-256
	mov r0, #0x1234
	mov r0, #0xff000000
	mov r7, 190
	add r0, r1, 4
	cmp r0, 3
	and r1, r1, -16
	mov r0, (2+3)
	.equ K, 5
	mov r0, K
	add sp, sp, #-8
