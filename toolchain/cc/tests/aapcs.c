// expect: 42
// AAPCS 64-bit argument passing: a 64-bit arg lands in an even-aligned register pair (skipping a register)
// or 8-aligned on the stack when it would straddle; plus __builtin_offsetof / __builtin_expect.
long long f1(int a, long long b)                 { return a + b; }        // a=r0, b=r2:r3 (r1 skipped)
long long f2(int a, int b, int c, long long d)   { return a + b + c + d; } // d straddles r3 -> stack, 8-aligned
int       f3(int a, long long b, int c)          { return (int)(a + b + c); } // a=r0, b=r2:r3, c on stack
struct s { char c; long long v; int x; };

int main(void) {
	int r = 0;
	if (f1(10, 0x100000000LL) == 0x10000000aLL)         r += 1;    // even-align to r2:r3        -> 1
	if (f2(1, 2, 3, 0x100000000LL) == 0x100000006LL)    r += 2;    // 64-bit straddles -> stack   -> 3
	if (f3(5, 0x200000000LL, 7) == 12)                  r += 4;    // 64-bit in regs + int stack  -> 7
	if (__builtin_offsetof(struct s, v) == 8)           r += 8;    // long long member 8-aligned  -> 15
	if (__builtin_offsetof(struct s, x) == 16)          r += 16;   // after the 8-byte v          -> 31
	if (__builtin_expect(11, 0) == 11)                  r += 8;    // returns its first argument  -> 39
	long long acc = f1(1, 2) + f2(0, 0, 0, 0) + f3(0, 0, 0);
	if (acc == 3)                                       r += 2;    //                             -> 41
	if (sizeof(struct s) == 24)                         r += 1;    // 8-aligned struct            -> 42
	return r;
}
