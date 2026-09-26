@ `.set name, target` where the target is defined LATER (GAS resolves aliases at the end), including an
@ alias of an alias; references to the aliases before and after, compared (bytes + relocs) with GNU as.
	.global b
	.set b, a
	.set c, b
	.data
	.word b, c
a:	.word 1, 2
	.word a, b, c
