// expect: 0
/* __builtin_{add,sub,mul}_overflow (kernel check_*_overflow / size_mul): result wrapped into *res's type,
 * value = whether it overflowed. Plus object_size (unknown -> -1/0), isdigit, va_copy, libc aliases. */
typedef unsigned long long u64; typedef long long s64; typedef unsigned long size_t;
static unsigned rt(unsigned x) { return x; }
int main(void) {
	size_t z; unsigned char c8; int i; u64 q; s64 sq;
	int __x = 7;                                                      /* a user name the lowering must not shadow */
	if (__builtin_mul_overflow(rt(0x10000), rt(0x10000), &z) != 1 || z != 0) return 1;
	if (__builtin_mul_overflow(rt(1000), rt(1000), &z) != 0 || z != 1000000) return 2;
	if (__builtin_add_overflow(rt(200), rt(100), &c8) != 1 || c8 != 44) return 3;
	if (__builtin_sub_overflow(rt(1), rt(2), &z) != 1 || z != 0xffffffffu) return 4;
	if (__builtin_add_overflow(0x7fffffff, 1, &i) != 1 || i != (int)0x80000000) return 5;
	if (__builtin_mul_overflow(-3, 4, &i) != 0 || i != -12) return 6;
	if (__builtin_add_overflow((u64)rt(0xffffffff) << 32, (u64)rt(0xffffffff) << 32, &q) != 1) return 7;
	if (__builtin_mul_overflow((u64)rt(0x100000000ull >> 1) , (u64)rt(4), &q) != 0 || q != 0x200000000ull) return 8;
	if (__builtin_mul_overflow((u64)1 << 33, (u64)1 << 31, &q) != 1) return 9;
	if (__builtin_sub_overflow((u64)rt(1), (u64)rt(2), &q) != 1 || q != ~0ull) return 10;
	if (__builtin_add_overflow((s64)0x7fffffffffffffffLL, (s64)rt(1), &sq) != 1) return 11;
	if (__builtin_mul_overflow((s64)rt(3000000), (s64)-(s64)rt(3000000), &sq) != 0 || sq != -9000000000000LL) return 15;
	if (__builtin_mul_overflow((s64)1 << 62, (s64)-2, &sq) != 0 || sq != (s64)0x8000000000000000ULL) return 16;   /* exactly INT64_MIN: fits */
	if (__builtin_mul_overflow((s64)1 << 62, (s64)2, &sq) != 1) return 17;                                      /* 2^63: doesn't */
	if (__builtin_object_size(&z, 0) != (size_t)-1 || __builtin_object_size(&z, 2) != 0) return 12;
	if (!__builtin_isdigit(rt('7')) || __builtin_isdigit(rt('a'))) return 13;
	if (__x != 7) return 14;
	return 0;
}
