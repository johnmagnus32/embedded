// expect: 0
/* Integer-literal types follow the suffix (C11 6.4.4.1): `7ULL << 32` is a 64-bit shift (was done in 32 bits);
 * 1U is unsigned (so -1 compares as big); 0xFFFFFFFF is unsigned int; a big decimal is long long. */
int main(void) {
	unsigned long long a = (7ULL << 32) | 9;
	if ((a >> 32) != 7 || (unsigned)a != 9) return 1;
	if (sizeof(1ULL) != 8 || sizeof(1LL) != 8 || sizeof(1U) != 4 || sizeof(1) != 4) return 2;
	if (!(-1 < 1)) return 3;
	if (-1 < 1U) return 4;                       /* -1 converts to UINT_MAX */
	if (sizeof(0xFFFFFFFF) != 4 || 0xFFFFFFFF < 0) return 5;
	if (sizeof(4294967296) != 8) return 6;
	if ((1LL << 40) >> 40 != 1) return 7;
	return 0;
}
