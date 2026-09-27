@ aeabi.s — the RTABI division entry points that return TWO results in registers (quotient + remainder),
@ which C can't express: __aeabi_{u,}idivmod -> r0 = q, r1 = r; __aeabi_{u,}ldivmod -> r0:r1 = q, r2:r3 = r
@ (also __aeabi_{u,}idiv: the quotient alone). A zero divisor gives libgcc's result (0 / all-ones /
@ INT_MAX|INT_MIN by the dividend's sign) handed to __aeabi_{i,l}div0 (SIGFPE by default).
	.text

	.global __aeabi_uidiv
	.type __aeabi_uidiv, %function
__aeabi_uidiv:
	cmp	r1, #0
	beq	1f
	udiv	r0, r0, r1
	bx	lr
	.size __aeabi_uidiv, . - __aeabi_uidiv

	.global __aeabi_uidivmod
	.type __aeabi_uidivmod, %function
__aeabi_uidivmod:
	cmp	r1, #0
	beq	1f
	udiv	r2, r0, r1
	mls	r1, r2, r1, r0
	mov	r0, r2
	bx	lr
1:	cmp	r0, #0
	mvnne	r0, #0
	b	__aeabi_idiv0
	.size __aeabi_uidivmod, . - __aeabi_uidivmod

	.global __aeabi_idiv
	.type __aeabi_idiv, %function
__aeabi_idiv:
	cmp	r1, #0
	beq	2f
	sdiv	r0, r0, r1
	bx	lr
	.size __aeabi_idiv, . - __aeabi_idiv

	.global __aeabi_idivmod
	.type __aeabi_idivmod, %function
__aeabi_idivmod:
	cmp	r1, #0
	beq	2f
	sdiv	r2, r0, r1
	mls	r1, r2, r1, r0
	mov	r0, r2
	bx	lr
2:	cmp	r0, #0
	mvngt	r0, #0x80000000
	movlt	r0, #0x80000000
	b	__aeabi_idiv0
	.size __aeabi_idivmod, . - __aeabi_idivmod

@ 64-bit: n = r0:r1, d = r2:r3. The C core takes a remainder pointer (stack argument), read back into r2:r3.
	.global __aeabi_uldivmod
	.type __aeabi_uldivmod, %function
__aeabi_uldivmod:
	orrs	ip, r2, r3
	beq	3f
	push	{r4, lr}
	sub	sp, sp, #16
	add	ip, sp, #8
	str	ip, [sp]
	bl	__os_udivmod64
	ldr	r2, [sp, #8]
	ldr	r3, [sp, #12]
	add	sp, sp, #16
	pop	{r4, pc}
3:	orrs	ip, r0, r1
	mvnne	r0, #0
	mvnne	r1, #0
	b	__aeabi_ldiv0
	.size __aeabi_uldivmod, . - __aeabi_uldivmod

	.global __aeabi_ldivmod
	.type __aeabi_ldivmod, %function
__aeabi_ldivmod:
	orrs	ip, r2, r3
	beq	4f
	push	{r4, lr}
	sub	sp, sp, #16
	add	ip, sp, #8
	str	ip, [sp]
	bl	__os_divmod64
	ldr	r2, [sp, #8]
	ldr	r3, [sp, #12]
	add	sp, sp, #16
	pop	{r4, pc}
4:	cmp	r1, #0
	blt	5f
	orrs	ip, r0, r1
	beq	6f
	mvn	r0, #0
	mvn	r1, #0x80000000
	b	__aeabi_ldiv0
5:	mov	r0, #0
	mov	r1, #0x80000000
6:	b	__aeabi_ldiv0
	.size __aeabi_ldivmod, . - __aeabi_ldivmod

@ Division by zero (RTABI 4.3.2), exactly libgcc's Linux hook: raise(SIGFPE), then return raise()'s result in r0
@ with r1 preserved (so a 64-bit result keeps the caller's high word). One weak body for both names.
	.weak __aeabi_idiv0
	.weak __aeabi_ldiv0
	.type __aeabi_idiv0, %function
	.type __aeabi_ldiv0, %function
__aeabi_idiv0:
__aeabi_ldiv0:
	push	{r1, lr}
	mov	r0, #8
	bl	raise
	pop	{r1, pc}
	.size __aeabi_idiv0, . - __aeabi_idiv0
	.size __aeabi_ldiv0, . - __aeabi_ldiv0

@ __clear_cache(begin, end): make freshly written code visible to instruction fetch (Linux cacheflush).
	.global __clear_cache
	.type __clear_cache, %function
__clear_cache:
	push	{r7, lr}
	mov	r2, #0
	movw	r7, #0x0002
	movt	r7, #0x000f
	svc	#0
	pop	{r7, pc}
	.size __clear_cache, . - __clear_cache
