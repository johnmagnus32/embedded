// expect: 42
// GCC extended inline asm: %N operand substitution with "r"/"=r"/"+r"/"i" constraints, multi-line
// templates (\n\t), reused operands (%0), and an ignored clobber list.
int main(void) {
	int r = 0;
	int a = 10, b = 7, d;
	__asm__("add %0, %1, %2" : "=r"(d) : "r"(a), "r"(b));                    // d = 17
	if (d == 17)   r += 1;                                                   // -> 1
	int x;
	__asm__("mov %0, #42" : "=r"(x));                                        // x = 42 (no inputs)
	if (x == 42)   r += 2;                                                   // -> 3
	int y = 5;
	__asm__("add %0, %0, #1" : "+r"(y));                                     // y = 6 (in-out)
	if (y == 6)    r += 4;                                                   // -> 7
	int z;
	__asm__("mov %0, %1" : "=r"(z) : "i"(100));                             // z = 100 (immediate)
	if (z == 100)  r += 8;                                                   // -> 15
	int m;
	__asm__("lsl %0, %1, #2\n\tadd %0, %0, #3" : "=r"(m) : "r"(b));          // m = (7<<2)+3 = 31 (multi-line, %0 reused)
	if (m == 31)   r += 16;                                                  // -> 31
	int cnt;
	__asm__ volatile("mov %0, #11" : "=r"(cnt) :: "memory");                 // clobber ignored
	if (cnt == 11) r += 8;                                                   // -> 39
	int p = 1, q = 2, s;
	__asm__("add %0, %1, %2\n\tadd %0, %0, %3" : "=r"(s) : "r"(p), "r"(q), "i"(0));  // s = 1+2+0 = 3
	if (s == 3)    r += 2;                                                   // -> 41
	int one;
	__asm__("mov %0, #1" : "=r"(one));
	if (one == 1)  r += 1;                                                   // -> 42
	return r;
}
