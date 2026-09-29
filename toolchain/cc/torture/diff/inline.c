/* inline.c — always_inline functions expanded at their calls: __builtin_va_arg_pack() forwarding the caller's
 * variadic arguments into positions with a different register/stack alignment (doubles, long longs, structs),
 * __builtin_va_arg_pack_len(), an expansion inside an expansion, labels and loops in the body, a return from the
 * middle, the names resolved where the function was defined (not in the caller), __func__, GNU extern inline +
 * gnu_inline (never emitted). Must match GCC. */
int printf(const char *, ...);
struct S { int a; double d; };
int g = 100;
static int show(int tag, int n, ...) {
	__builtin_va_list ap; __builtin_va_start(ap, n);
	printf("show %d n=%d:", tag, n);
	for (int i = 0; i < n; i++) {
		int k = __builtin_va_arg(ap, int);
		if (k == 1) printf(" d%d", (int)(__builtin_va_arg(ap, double) * 4));
		else if (k == 2) printf(" ll%llx", __builtin_va_arg(ap, long long));
		else if (k == 3) { struct S s = __builtin_va_arg(ap, struct S); printf(" s%d/%d", s.a, (int)s.d); }
		else printf(" i%d", __builtin_va_arg(ap, int));
	}
	__builtin_va_end(ap); printf("\n"); return tag;
}
extern inline __attribute__((always_inline, gnu_inline)) int fwd(int tag, ...) {
	if (tag < 0) return -1;
	return show(tag, __builtin_va_arg_pack_len() / 2, __builtin_va_arg_pack());   /* tag, n: two words before the pack */
}
static inline __attribute__((always_inline)) int fwd2(int a, int b, int c, ...) {   /* three words before: other alignment */
	int t = 0;
	for (int i = 0; i < 3; i++) { if (i == c) goto done; t += i; }
done:
	return t + show(a + b, __builtin_va_arg_pack_len() / 2, __builtin_va_arg_pack()) + fwd(a, __builtin_va_arg_pack());
}
static inline __attribute__((always_inline)) const char *who(void) { return __func__; }
extern inline __attribute__((gnu_inline)) int twice(int x) { return 2 * x; }   /* inline-only: the external twice is called */
int twice(int x) { return 2 * x + 1000; }
int main(void) {
	int g = 7;   /* the inline functions still see the global g */
	struct S s = { 5, 6.5 };
	printf("r %d\n", fwd(1, 1, 2.5, 2, 0x100000002LL, 3, s, 0, g));
	printf("r %d\n", fwd2(2, 3, 9, 1, 0.25, 0, 11));
	printf("r %d\n", fwd(-5, 1, 1.0));
	printf("who %s g %d twice %d\n", who(), g, twice(21));
	return 0;
}
