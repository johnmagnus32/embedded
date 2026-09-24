// expect: 0
/* mov with a non-encodable immediate falls back to mvn of the complement (as GAS does): `mov rd,#-14` ->
 * `mvn rd,#13`. Also covers the cc side: an "i" asm operand prints as `#-14` (kernel maccess -EFAULT fixup). */
int main(void) {
	int x; __asm__("mov %0, #-14" : "=r"(x));           if (x != -14) return 1;
	unsigned u; __asm__("mov %0, #0xFFFFFF00" : "=r"(u)); if (u != 0xFFFFFF00u) return 2;   /* mvn #0xFF */
	int y; __asm__("mov %0, %1" : "=r"(y) : "i"(-14));   if (y != -14) return 3;            /* operand -> #-14 -> mvn */
	return 0;
}
