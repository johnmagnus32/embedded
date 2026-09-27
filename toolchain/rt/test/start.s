@ start.s — bare-metal entry for the runtime test (qemu -M virt + semihosting): enable VFP, set a stack,
@ run main(), exit qemu with its return value (SYS_EXIT_EXTENDED).
	.text
	.global _start
_start:
	mrc	p15, 0, r0, c1, c0, 2
	orr	r0, r0, #(0xf << 20)
	mcr	p15, 0, r0, c1, c0, 2
	isb
	mov	r0, #(1 << 30)
	vmsr	fpexc, r0
	movw	sp, #0x0000
	movt	sp, #0x4090
	bl	main
	movw	r2, #0x0026
	movt	r2, #0x0002
	push	{r0}
	push	{r2}
	mov	r1, sp
	mov	r0, #0x20
	svc	0x123456
1:	b	1b
