// expect: 63
// 64-bit divide/modulo: cc calls the RTABI __aeabi_uldivmod / __aeabi_ldivmod (quotient r0:r1, remainder
// r2:r3), from our compiler runtime (toolchain/rt), linked into every test.

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
