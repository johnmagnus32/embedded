// expect: 63
// 64-bit divide/modulo: cc emits `bl __udivdi3` / `__umoddi3` / `__divdi3` / `__moddi3`; the helpers
// (same shift-subtract core as libc/src/lldiv.c) are defined here so the test links standalone.
static unsigned long long udivmod(unsigned long long n, unsigned long long d, unsigned long long *rem) {
	unsigned long long q = 0, r = 0;
	int i = 63;
	while (i >= 0) {
		r = (r << 1) | ((n >> i) & 1);
		if (r >= d && d != 0) { r = r - d; q = q | (((unsigned long long)1) << i); }
		i = i - 1;
	}
	if (rem) *rem = r;
	return q;
}
unsigned long long __udivdi3(unsigned long long n, unsigned long long d) { return udivmod(n, d, 0); }
unsigned long long __umoddi3(unsigned long long n, unsigned long long d) { unsigned long long r; udivmod(n, d, &r); return r; }
long long __divdi3(long long n, long long d) {
	int neg = 0;
	if (n < 0) { n = -n; neg = neg ^ 1; }
	if (d < 0) { d = -d; neg = neg ^ 1; }
	unsigned long long q = udivmod((unsigned long long)n, (unsigned long long)d, 0);
	return neg ? -(long long)q : (long long)q;
}
long long __moddi3(long long n, long long d) {
	int neg = 0;
	if (n < 0) { n = -n; neg = 1; }
	if (d < 0) { d = -d; }
	unsigned long long r;
	udivmod((unsigned long long)n, (unsigned long long)d, &r);
	return neg ? -(long long)r : (long long)r;
}

int main(void) {
	int r = 0;
	unsigned long long a = 0x100000000ULL;                 // 2^32
	if (a / 3 == 1431655765ULL)                    r += 1;  // unsigned 64-bit divide      -> 1
	if (a % 7 == 4)                                r += 2;  // unsigned 64-bit modulo      -> 3
	if ((0x1122334455667788ULL / 0x10000ULL) == 0x112233445566ULL) r += 4;  // wide / wide -> 7
	long long s = -7;
	if (s / 2 == -3)                               r += 8;  // signed divide (toward zero) -> 15
	if (s % 2 == -1)                               r += 16; // signed modulo (dividend sign)-> 31
	if ((-1000000000000LL / 1000LL) == -1000000000LL) r += 32; // > 32-bit signed divide   -> 63
	return r;
}
