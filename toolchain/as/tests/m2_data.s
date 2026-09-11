@ m2_data.s — data directives + pc-relative literal loads + local-vs-global branch resolution.
	.text
	.global m2d
	.type m2d, %function
m2d:
	ldr    r0, .LPOOL          @ pc-relative literal load (forward)
	ldrne  r1, .LPOOL+4        @ conditional + addend
	b      .Lnext              @ local label -> resolved (no reloc)
.Lnext:
	bl     external_fn         @ external -> relocation
	bl     m2d                 @ GLOBAL same section -> relocated (like GNU)
	bx     lr
	.p2align 2
.LPOOL:
	.word  external_fn         @ external  -> R_ARM_ABS32
	.word  0x12345678          @ literal number
