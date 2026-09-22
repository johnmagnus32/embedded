/*
 * lldiv.c — 64-bit integer divide/modulo helpers. Our cc emits `bl __udivdi3` / `__umoddi3` / `__divdi3`
 * / `__moddi3` for `/` and `%` on 64-bit operands (ARMv7 has no 64-bit divide instruction), exactly as
 * gcc/libgcc does. Same role as libgcc's __udivdi3 &c.; we provide our own so TOOLCHAIN=custom needs no
 * libgcc. The core is a plain shift-subtract long division built only from 64-bit shift/compare/subtract
 * (all of which cc now generates inline), so the helpers don't recurse into 64-bit divide.
 */

/* Unsigned 64-bit divide: returns the quotient; if rem != 0, stores the remainder through it. */
static unsigned long long udivmod(unsigned long long n, unsigned long long d, unsigned long long *rem)
{
	unsigned long long q = 0, r = 0;
	int i = 63;
	while (i >= 0) {
		r = (r << 1) | ((n >> i) & 1);              /* shift the next dividend bit into the remainder */
		if (r >= d && d != 0) { r = r - d; q = q | (((unsigned long long)1) << i); }
		i = i - 1;
	}
	if (rem) *rem = r;
	return q;
}

unsigned long long __udivdi3(unsigned long long n, unsigned long long d) { return udivmod(n, d, 0); }
unsigned long long __umoddi3(unsigned long long n, unsigned long long d) { unsigned long long r; udivmod(n, d, &r); return r; }

long long __divdi3(long long n, long long d)
{
	int neg = 0;
	if (n < 0) { n = -n; neg = neg ^ 1; }
	if (d < 0) { d = -d; neg = neg ^ 1; }
	unsigned long long q = udivmod((unsigned long long)n, (unsigned long long)d, 0);
	return neg ? -(long long)q : (long long)q;
}

long long __moddi3(long long n, long long d)
{
	int neg = 0;
	if (n < 0) { n = -n; neg = 1; }                 /* the remainder takes the dividend's sign */
	if (d < 0) { d = -d; }
	unsigned long long r;
	udivmod((unsigned long long)n, (unsigned long long)d, &r);
	return neg ? -(long long)r : (long long)r;
}
