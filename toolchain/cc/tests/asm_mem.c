// expect: 42
// Inline-asm memory operands: GCC "m"/"=m" (general) and ARM "Q" (single-register address). The operand's
// ADDRESS is loaded into a register and substituted as "[reg]" (kernel __raw_writeX / put_unaligned).
int main(void)
{
	int a = 0, b = 0;
	__asm__ volatile("str %1, %0" : "=m" (a) : "r" (40));            /* "=m" memory output */
	__asm__ volatile("str %1, %0" : : "Q" (*(volatile int *)&b), "r" (2));  /* "Q" single-reg memory */
	return a + b;   /* 40 + 2 = 42 */
}
