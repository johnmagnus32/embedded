// expect: 42
// Local register variable bound with the `asm("rN")` spelling (not just `__asm__`),
// as in arch/arm/include/asm/div64.h: `register unsigned int __base asm("r4") = base;`.
int main(void)
{
	register int a asm("r4") = 40;
	register int b __asm__("r5") = 2;
	return a + b;
}
