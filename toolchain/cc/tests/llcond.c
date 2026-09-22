// expect: 3
// A conditional (?:) whose arms differ in width: a 64-bit arm and a 32-bit arm. The result is 64-bit, so
// the 32-bit arm must be widened (else its high word is garbage). This is the exact shape of printf's %d
// path (`lng ? va_arg(long) : (long)va_arg(int)` folded into a 64-bit value) that regressed the boot once.
int main(void) {
	int r = 0;
	int t = 1, f = 0;
	long long a = f ? 0x123456789LL : (long)5;   // 32-bit els arm taken -> widened -> 5
	if (a == 5) r += 1;
	long long b = t ? 0x123456789LL : (long)5;   // 64-bit then arm taken
	if (b == 0x123456789LL) r += 2;
	return r;
}
