// expect: 42
// 64-bit (long long) values in register pairs: wide constants, add/mul/shift/compare/negate/bitand on
// values that exceed 32 bits, narrowing casts, a 64-bit global, and 64-bit parameters + return value.
long long add64(long long a, long long b) { return a + b; }

unsigned long long gull = 0x1122334455667788ULL;

int main(void) {
	int r = 0;
	long long a = 0x100000000LL;                     // 2^32 — needs both words
	if (a + 1 == 0x100000001LL)        r += 1;       // 64-bit add (carry into high word)     -> 1
	if (a > 0xffffffffLL)              r += 2;       // 64-bit compare across the word boundary-> 3
	if ((a >> 32) == 1)                r += 4;       // 64-bit shift by 32                     -> 7
	long long b = 3;
	if (a * b == 0x300000000LL)        r += 8;       // 64-bit multiply (umull)               -> 15
	if (add64(a, a) == 0x200000000LL)  r += 16;      // 64-bit args + 64-bit return           -> 31
	unsigned long long u = gull;                     // 64-bit global load
	if ((u >> 40) == 0x112233ULL)      r += 4;       // unsigned 64-bit logical shift         -> 35
	if ((int)(a + 7) == 7)             r += 4;       // narrow 64->32 keeps the low word       -> 39
	if (-a == -0x100000000LL)          r += 2;       // 64-bit negate                          -> 41
	if ((a & 0xffffffffLL) == 0)       r += 1;       // 64-bit bitwise-and (low word is zero)  -> 42
	return r;
}
