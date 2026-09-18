/* crti.S — opens the _init/_fini frames for a conforming ARM EABI sysroot.
 * GCC's crtbegin.o/crtend.o supply the body in between; crtn.S closes them.
 * Pushed reg count is even to preserve 8-byte AAPCS stack alignment. */
	.section .init,"ax",%progbits
	.p2align 2
	.global _init
	.type _init, %function
_init:
	push {r0, r1, r2, r3, fp, lr}
	/* fall through into crtbegin's .init contribution */

	.section .fini,"ax",%progbits
	.p2align 2
	.global _fini
	.type _fini, %function
_fini:
	push {r0, r1, r2, r3, fp, lr}
	/* fall through into crtbegin's .fini contribution */

	.section .note.GNU-stack,"",%progbits
