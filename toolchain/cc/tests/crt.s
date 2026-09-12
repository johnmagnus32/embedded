@ crt.s — bare-metal semihosting startup for cc tests: set a stack, call main(), then exit qemu with
@ main()'s return value as the process exit code via ARM semihosting SYS_EXIT_EXTENDED (arbitrary code).
.text
.global _start
_start:
	movw sp, #0x0000
	movt sp, #0x4090          @ sp = 0x40900000 (well above the loaded image in virt RAM)
	bl   main                 @ result -> r0
	mov  r1, r0               @ exit code
	movw r2, #0x0026
	movt r2, #0x0002          @ r2 = 0x20026 = ADP_Stopped_ApplicationExit
	push {r1}                 @ block[1] = exit code (higher address)
	push {r2}                 @ block[0] = reason     (lower address = sp)
	mov  r1, sp               @ r1 -> {reason, code}
	mov  r0, #0x20            @ SYS_EXIT_EXTENDED
	svc  0x123456             @ semihosting call
hang:
	b    hang
