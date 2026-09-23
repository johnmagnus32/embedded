// expect: 42
// Named inline-asm operands %[name] (kernel arch asm, e.g. arch/arm/include/asm/kexec.h register moves).
int main(void)
{
	int r, a = 40, b = 2;
	__asm__("add %[out], %[x], %[y]" : [out] "=r" (r) : [x] "r" (a), [y] "r" (b));
	return r;   /* 40 + 2 = 42 */
}
