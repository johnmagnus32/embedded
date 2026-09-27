/*
 * divide.c — integer division (ARM RTABI 4.3.1 + the libgcc names). ARMv7-A (Cortex-A7) divides 32 bits
 * in hardware, so the 32-bit helpers are thin; 64-bit division is a normalized shift-subtract with a
 * 32-bit hardware fast path. The register-returning __aeabi_{u,}{i,l}divmod entry points are in aeabi.s.
 */

int __aeabi_idiv0(int r); long long __aeabi_ldiv0(long long r);   /* aeabi.s: SIGFPE (weak, overridable) */

/* n / d with the remainder through *rem (d != 0). */
unsigned long long __os_udivmod64(unsigned long long n, unsigned long long d, unsigned long long *rem)
{
	if (!(n >> 32) && !(d >> 32)) {   /* both fit a word: the hardware divider */
		unsigned q = (unsigned)n / (unsigned)d;
		*rem = (unsigned)n - q * (unsigned)d;
		return q;
	}
	if (n < d) { *rem = n; return 0; }
	int sh = __builtin_clzll(d) - __builtin_clzll(n);   /* align d's top bit with n's */
	unsigned long long q = 0;
	d <<= sh;
	for (int i = 0; i <= sh; i++) {
		q <<= 1;
		if (n >= d) { n -= d; q |= 1; }
		d >>= 1;
	}
	*rem = n;
	return q;
}

/* C99 truncating signed division: the quotient rounds toward zero, the remainder takes n's sign. */
long long __os_divmod64(long long n, long long d, long long *rem)
{
	unsigned long long un = n < 0 ? -(unsigned long long)n : (unsigned long long)n;
	unsigned long long ud = d < 0 ? -(unsigned long long)d : (unsigned long long)d, r;
	unsigned long long q = __os_udivmod64(un, ud, &r);
	*rem = n < 0 ? -(long long)r : (long long)r;
	return (n < 0) != (d < 0) ? -(long long)q : (long long)q;
}

/* libgcc names */
unsigned long long __udivmoddi4(unsigned long long n, unsigned long long d, unsigned long long *rem)
{
	unsigned long long r, q;
	if (!d) return (unsigned long long)__aeabi_ldiv0(0);
	q = __os_udivmod64(n, d, &r);
	if (rem) *rem = r;
	return q;
}
long long __divmoddi4(long long n, long long d, long long *rem)
{
	long long r, q;
	if (!d) return __aeabi_ldiv0(0);
	q = __os_divmod64(n, d, &r);
	if (rem) *rem = r;
	return q;
}
unsigned long long __udivdi3(unsigned long long n, unsigned long long d) { return __udivmoddi4(n, d, 0); }
unsigned long long __umoddi3(unsigned long long n, unsigned long long d) { unsigned long long r = 0; __udivmoddi4(n, d, &r); return r; }
long long __divdi3(long long n, long long d) { return __divmoddi4(n, d, 0); }
long long __moddi3(long long n, long long d) { long long r = 0; __divmoddi4(n, d, &r); return r; }

unsigned __udivsi3(unsigned n, unsigned d) { return d ? n / d : (unsigned)__aeabi_idiv0(0); }
unsigned __umodsi3(unsigned n, unsigned d) { return d ? n % d : (unsigned)__aeabi_idiv0(0); }
int __divsi3(int n, int d) { return d ? n / d : __aeabi_idiv0(0); }
int __modsi3(int n, int d) { return d ? n % d : __aeabi_idiv0(0); }
