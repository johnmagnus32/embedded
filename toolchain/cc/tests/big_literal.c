// expect: 0
/* Integer literals > LONG_MAX must wrap to their unsigned 64-bit value, not saturate (xxh64's PRIME64_1/2). */
typedef unsigned long long u64;
static u64 id(u64 x) { return x; }
int main(void) {
	u64 a = 14029467366897019727ULL, b = 0xFFFFFFFFFFFFFFFFULL;
	if ((a >> 32) != 0xc2b2ae3dULL) return 1;
	if ((unsigned)a != 0x27d4eb4fu) return 2;
	if (b + 1 != 0) return 3;
	if (id(11400714785074694791ULL) != 0x9e3779b185ebca87ULL) return 4;
	return 0;
}
