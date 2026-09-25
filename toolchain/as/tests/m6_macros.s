@ GAS macro engine: defaults, :req, :vararg, keyword args, blank = default, .ifnb, .if on an .equ symbol (the same
@ strict evaluator as everything else), .irp / .irpc, .exitm, .rept N.
.equ N, 3
.macro m a, b=7, c:req, d:vararg
	.word \a, \b, \c
	.ifnb \d
	.word \d
	.endif
.endm
.data
	m 1, 2, 3
	m 4, c=5
	m 6, , 8, 9, 10
.if N - 3
	.word 99
.else
	.word 100
.endif
.irp r, 11, 12, 13
	.word \r
.endr
.irpc c, 456
	.byte \c
.endr
.macro early x
	.word \x
	.exitm
	.word 666
.endm
	early 77
.rept N
	.byte 1
.endr
