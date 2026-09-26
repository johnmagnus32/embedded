@ Branches to a WEAK symbol defined in the same section keep an R_ARM_CALL/JUMP24 (the linker may bind a
@ strong definition instead); a local target resolves in place; a global one keeps its reloc (GNU as).
	.text
	.weak wk
wk:	bx lr
loc:	bx lr
	.global gl
gl:	bx lr
	bl wk
	b wk
	bl loc
	bl gl
	blne wk
