/* apply.c — GCC's untyped call forwarding: __builtin_apply_args / __builtin_apply / __builtin_return, with core and
 * stack arguments and VFP arguments, integer and double results. Must match GCC. */
int printf(const char *, ...);
static int six(int a, int b, int c, int d, int e, int f) { return a + 2 * b + 3 * c + 4 * d + 5 * e + 6 * f; }
static double fp(double x, float y, int k) { return x * 10 + y + k; }
static int fwd_six(int a, int b, int c, int d, int e, int f) {
	void *r = __builtin_apply((void (*)())six, __builtin_apply_args(), 8);   /* e and f: 8 bytes on the stack */
	__builtin_return(r);
}
static double fwd_fp(double x, float y, int k) {
	void *r = __builtin_apply((void (*)())fp, __builtin_apply_args(), 0);
	__builtin_return(r);
}
int main(void) {
	printf("six %d fp %d\n", fwd_six(1, 2, 3, 4, 5, 6), (int)(fwd_fp(1.5, 0.25f, 3) * 4));
	return 0;
}
