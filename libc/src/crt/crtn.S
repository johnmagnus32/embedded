/* crtn.S — closes the _init/_fini frames opened by crti.S. */
	.section .init,"ax",%progbits
	pop {r0, r1, r2, r3, fp, pc}

	.section .fini,"ax",%progbits
	pop {r0, r1, r2, r3, fp, pc}

	.section .note.GNU-stack,"",%progbits
