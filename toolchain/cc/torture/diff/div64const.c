/* div64const.c — 64-bit division and remainder by constants (powers of two, 1, a zero-extended 32-bit dividend), the
 * forms GCC emits without __aeabi_[u]ldivmod; signed ones round toward zero. Must print what GCC's build prints. */
typedef unsigned long long u64; typedef long long s64;
int printf(const char *, ...);
static u64 ud[] = { 0, 1, 5, 1023, 1024, 1025, 0xffffffffULL, 0x100000000ULL, 0x123456789abcdefULL, 0xffffffffffffffffULL };
static s64 sd[] = { 0, 1, -1, 7, -7, 8, -8, 1023, -1023, -1024, 0x123456789abcdefLL, -0x123456789abcdefLL, (s64)0x8000000000000001ULL };
volatile unsigned u32v = 4000000000u;
int main(void) {
	unsigned long long h = 0;
	for (int i = 0; i < 10; i++) { u64 x = ud[i]; h = h * 31 + x / 1024 + x % 1024 * 7 + x / 0x100000000ULL + x % 0x100000000ULL + x / 0x40000000000ULL + x % 0x40000000000ULL + x / 1 + x % 1; }
	for (int i = 0; i < 13; i++) { s64 x = sd[i]; h = h * 31 + (u64)(x / 8) + (u64)(x % 8) * 3 + (u64)(x / 0x100000000LL) + (u64)(x % 0x100000000LL) + (u64)(x / 0x4000000000LL) + (u64)(x % 0x4000000000LL); }
	u64 w = u32v; h = h * 31 + w / 1000 + w % 1000 + w / 3;
	printf("%llx\n", h); return 0;
}
