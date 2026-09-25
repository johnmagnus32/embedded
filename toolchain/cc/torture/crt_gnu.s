@ crt_gnu.s — startup for the GCC ORACLE side of the c-torture harness (qemu-system-arm -M virt, semihosting).
@ Same contract as tests/crt.s (stack, main(), exit status via SYS_EXIT_EXTENDED), plus: the reference GCC is a
@ hard-float toolchain, so enable the VFP unit first (CPACR cp10/cp11 full access, FPEXC.EN) or the first FP
@ instruction traps. Our cc emits no FP, so our side keeps using tests/crt.s.
	.text
	.global _start
_start:
	mrc	p15, 0, r0, c1, c0, 2
	orr	r0, r0, #(0xf << 20)      @ CPACR: cp10 + cp11 full access
	mcr	p15, 0, r0, c1, c0, 2
	isb
	mov	r0, #(1 << 30)
	vmsr	fpexc, r0                 @ FPEXC.EN
	movw	sp, #0x0000
	movt	sp, #0x4090
	bl	main
	bl	exit                      @ main's return value -> exit() -> _exit() -> semihosting
1:	b	1b
