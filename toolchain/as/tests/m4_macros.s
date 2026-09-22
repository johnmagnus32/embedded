@ GAS macro engine: .macro (with args), .rept, .if/.else, .ifdef — expansions are ordinary instructions,
@ so the emitted bytes are byte-identical to GNU as when the parity gate runs.
	.text
	.global _t

	.macro addN reg, n
	add \reg, \reg, #\n
	.endm

	.macro clobber
	mov r12, #0
	.endm

_t:
	push {r0-r3, lr}
	addN r0, 5
	addN r1, 10
	clobber
	.rept 3
	nop
	.endr
	.if 4 == 4
	mov r2, #1
	.else
	mov r2, #2
	.endif
	.if 2 > 5
	mov r3, #9        @ skipped
	.else
	mov r3, #7
	.endif
	pop {r0-r3, pc}
