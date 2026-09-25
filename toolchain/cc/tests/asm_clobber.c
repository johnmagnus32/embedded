// expect: 0
/* asm clobber lists are honored: (1) an input operand is never allocated to a clobbered register (the template
 * would destroy it), (2) a clobbered callee-saved register (r4-r10) is preserved across the asm, as a GCC-built
 * caller expects. Was: clobbers ignored. */
static int twice(int x) {
	int r;
	/* the template clobbers r0 and r1 before reading %1: if x were allocated to r0/r1 the result would be wrong */
	__asm__("mov r0, #0\n\tmov r1, #0\n\tadd %0, %1, %1" : "=&r"(r) : "r"(x) : "r0", "r1");
	return r;
}
static int keep_r4(void) {
	register int v __asm__("r4") = 1234;   /* a value living in r4 across the asm (as GCC callers do) */
	__asm__ volatile("mov r4, #7" : : : "r4");
	__asm__ volatile("" : "+r"(v));
	return v;
}
int main(void) {
	if (twice(21) != 42) return 1;
	if (keep_r4() != 1234) return 2;
	return 0;
}
