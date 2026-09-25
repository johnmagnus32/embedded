// expect: 0
/* GCC bit/frame builtins with RUNTIME arguments expand inline (was: `bl __builtin_clz` -> undefined symbol). */
typedef unsigned long long u64;
static unsigned rt(unsigned x) { return x; }
static u64 rt64(u64 x) { return x; }
int main(void) {
	if (__builtin_clz(rt(1)) != 31 || __builtin_clz(rt(0x80000000u)) != 0) return 1;
	if (__builtin_clzll(rt64(1)) != 63 || __builtin_clzll(rt64(1ULL << 40)) != 23 || __builtin_clzll(rt(1)) != 63) return 2;
	if (__builtin_ctz(rt(8)) != 3 || __builtin_ctzl(rt(0x80000000u)) != 31) return 3;
	if (__builtin_ctzll(rt64(1ULL << 40)) != 40 || __builtin_ctzll(rt64(4)) != 2) return 4;
	if (__builtin_ffs(rt(0)) != 0 || __builtin_ffs(rt(8)) != 4) return 5;
	if (__builtin_ffsll(rt64(0)) != 0 || __builtin_ffsll(rt64(1ULL << 40)) != 41 || __builtin_ffsll(rt64(2)) != 2) return 6;
	if (__builtin_bswap16((unsigned short)rt(0x1234)) != 0x3412) return 7;
	if (__builtin_bswap32(rt(0x11223344)) != 0x44332211u) return 8;
	if (__builtin_bswap64(rt64(0x0102030405060708ULL)) != 0x0807060504030201ULL) return 9;
	if (__builtin_frame_address(0) == 0 || __builtin_return_address(0) == 0) return 10;
	return 0;
}
