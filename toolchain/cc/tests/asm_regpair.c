// expect: 0
/* 64-bit asm operands live in an even/odd register PAIR: %0 = low reg, %H0 = high (kernel atomic64:
 * `ldrexd %0, %H0, [%3]` / `adds %Q0,%Q0,%Q4` / `adc %R0,%R0,%R4`). Was: one reg -> `ldrexd r0, r0, ...`. */
typedef long long s64; typedef unsigned long long u64;
static void atomic64_add(s64 i, s64 *counter) {
	s64 result; unsigned long tmp;
	__asm__ __volatile__(
	"1:	ldrexd	%0, %H0, [%3]\n"
	"	adds	%Q0, %Q0, %Q4\n"
	"	adc	%R0, %R0, %R4\n"
	"	strexd	%1, %0, %H0, [%3]\n"
	"	teq	%1, #0\n"
	"	bne	1b"
	: "=&r" (result), "=&r" (tmp), "+Qo" (*counter)
	: "r" (counter), "r" (i)
	: "cc");
}
int main(void) {
	s64 c = 0x00000001FFFFFFFFLL;
	atomic64_add(1, &c);                                 /* carry out of the low word */
	if (c != 0x0000000200000000LL) return 1;
	atomic64_add(-0x100000001LL, &c);
	if (c != 0xFFFFFFFFLL) return 2;
	u64 v; __asm__("mov %0, %2\n\tmov %H0, %1" : "=&r"(v) : "r"(7), "r"(9));
	if (v != ((7ULL << 32) | 9)) return 3;
	return 0;
}
