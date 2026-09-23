// expect: 42
// Accurate __builtin_constant_p + __builtin_clzll folding, as needed by the kernel's ilog2()/order_base_2
// inside an array-size constant expression (kernfs.h: struct mutex m[1 << (2 * ilog2(...))]).
char a[1 << (2 * (__builtin_constant_p(16) ? (16 < 2 ? 0 : 63 - __builtin_clzll(16)) : 1))];  /* 1<<(2*4) = 256 */

int main(void)
{
	return (sizeof(a) == 256 && __builtin_constant_p(7) == 1) ? 42 : 1;
}
