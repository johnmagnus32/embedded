@ System, barrier, byte-reverse and exclusive-access instructions (added to compile mainline kernel code).
@ Encodings verified against the ARM ARM; byte-identical to GNU as when the parity gate runs.
	.text
	.global _t
_t:
	rev    r0, r1
	rev16  r2, r3
	revsh  r4, r5
	rbit   r6, r7
	umull  r0, r1, r2, r3
	smull  r4, r5, r6, r7
	dmb    ish
	dmb    ishst
	dmb    sy
	dsb    sy
	isb    sy
	mrs    r0, cpsr
	msr    cpsr_c, r0
	msr    cpsr_cxsf, r1
	cpsid  i
	cpsie  if
	mcr    p15, 0, r0, c1, c0, 0
	mrc    p15, 0, r1, c0, c0, 5
	ldrex  r0, [r1]
	ldrexb r2, [r3]
	strex  r4, r5, [r6]
	nop
	.long  0x12345678
	.inst  0xe320f000
