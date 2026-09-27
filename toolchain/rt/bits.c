/*
 * bits.c — bit-counting helpers GCC calls for __builtin_{clz,ctz,ffs,popcount,parity,bswap,clrsb}* when it
 * doesn't expand them inline (libgcc names; clz/ctz of 0 give the width, as the ARM instructions do).
 */

int __clzsi2(unsigned a) { return __builtin_clz(a); }
int __clzdi2(unsigned long long a) { return (unsigned)(a >> 32) ? __builtin_clz((unsigned)(a >> 32)) : 32 + __builtin_clz((unsigned)a); }
int __ctzsi2(unsigned a) { return __builtin_ctz(a); }
int __ctzdi2(unsigned long long a) { return (unsigned)a ? __builtin_ctz((unsigned)a) : 32 + __builtin_ctz((unsigned)(a >> 32)); }
int __ffssi2(unsigned a) { return a ? __builtin_ctz(a) + 1 : 0; }
int __ffsdi2(unsigned long long a) { return a ? __ctzdi2(a) + 1 : 0; }

int __popcountsi2(unsigned a)
{
	a = a - ((a >> 1) & 0x55555555u);
	a = (a & 0x33333333u) + ((a >> 2) & 0x33333333u);
	a = (a + (a >> 4)) & 0x0f0f0f0fu;
	return (int)((a * 0x01010101u) >> 24);
}
int __popcountdi2(unsigned long long a) { return __popcountsi2((unsigned)a) + __popcountsi2((unsigned)(a >> 32)); }
int __paritysi2(unsigned a) { return __popcountsi2(a) & 1; }
int __paritydi2(unsigned long long a) { return __popcountdi2(a) & 1; }

unsigned __bswapsi2(unsigned a) { return __builtin_bswap32(a); }
unsigned long long __bswapdi2(unsigned long long a) { return __builtin_bswap64(a); }

/* leading redundant sign bits: the leading bits equal to the sign bit, minus the sign bit itself */
int __clrsbsi2(int a) { return __builtin_clz((unsigned)(a < 0 ? ~a : a)) - 1; }   /* clz(0) = 32 -> 31 */
int __clrsbdi2(long long a) { unsigned long long v = (unsigned long long)(a < 0 ? ~a : a); return (v ? __clzdi2(v) : 64) - 1; }
