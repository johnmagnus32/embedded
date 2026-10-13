@ Expressions as GAS evaluates them: its precedence (<< binds tighter than +, comparisons are -1/0), unary minus of a
@ difference, and instruction immediates that name LATER labels (a fixup applied at the end: modified immediate,
@ else the partner instruction — and/bic, add/sub, mov/mvn, cmp/cmn — else movw).
	.syntax unified
	.text
a:	.word 1 + 2 << 3, 6 - 2 & 3, 7 < 6, 2 == 2, 1 && 0, 0 || 3, 8 >> 1 + 1, ~0 + 1, !5, 4 ! 1, 5 & 3 == 1, 1 | 2 ^ 3, -(3 > 1), 10 % 3 * 2
	add r10, r10, #((b - a + 256) & ~255)
	and r1, r1, #-((c - a) & 0xf0)
	mov r2, #((c - a) * 64)
	sub r4, r4, #(. - a) & 0xff
	mvn r5, #(c - a) * 16
	cmp r6, #-(c - b)
	mov r7, #(c - a) + 0x1000
	.word -(c - b), -(. - a), -(c - .)
	.set later, c - a
	.word later
	.set pfwd, (c - a) >> 2		@ parenthesized AND forward: the dispatcher must still DEFER it. Choosing the
	.word pfwd			@ branch on toks[2][0] sent a leading '(' to the constant-only evaluator and died,
	.set pabs, (4 * 4) >> 2		@ while the same expression unparenthesized deferred fine.
	.word pabs
	mrc p15, 0, APSR_nzcv, c7, c14, 3
	.space 300
b:	nop
c:	nop
