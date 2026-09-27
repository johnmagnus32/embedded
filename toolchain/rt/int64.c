/*
 * int64.c — 64-bit shifts / compares / multiply / negate (RTABI 4.2 + the libgcc names) and the -ftrapv
 * overflow-checking arithmetic (an overflow calls abort(), as libgcc's does, instead of wrapping).
 */

long long __aeabi_llsl(long long v, int n) { return (long long)((unsigned long long)v << n); }
long long __aeabi_llsr(long long v, int n) { return (long long)((unsigned long long)v >> n); }
long long __aeabi_lasr(long long v, int n) { return v >> n; }
long long __ashldi3(long long v, int n) { return (long long)((unsigned long long)v << n); }
long long __lshrdi3(long long v, int n) { return (long long)((unsigned long long)v >> n); }
long long __ashrdi3(long long v, int n) { return v >> n; }

int __aeabi_lcmp(long long a, long long b) { return a < b ? -1 : a > b; }                        /* -1 / 0 / 1 */
int __aeabi_ulcmp(unsigned long long a, unsigned long long b) { return a < b ? -1 : a > b; }
int __cmpdi2(long long a, long long b) { return a < b ? 0 : a > b ? 2 : 1; }                       /* 0 / 1 / 2 */
int __ucmpdi2(unsigned long long a, unsigned long long b) { return a < b ? 0 : a > b ? 2 : 1; }

long long __aeabi_lmul(long long a, long long b) { return (long long)((unsigned long long)a * (unsigned long long)b); }
long long __muldi3(long long a, long long b) { return (long long)((unsigned long long)a * (unsigned long long)b); }
long long __negdi2(long long a) { return (long long)-(unsigned long long)a; }

/* -ftrapv */
void abort(void);
int __addvsi3(int a, int b) { int r; if (__builtin_add_overflow(a, b, &r)) abort(); return r; }
int __subvsi3(int a, int b) { int r; if (__builtin_sub_overflow(a, b, &r)) abort(); return r; }
int __mulvsi3(int a, int b) { int r; if (__builtin_mul_overflow(a, b, &r)) abort(); return r; }
int __negvsi2(int a) { if (a == -2147483647 - 1) abort(); return -a; }
int __absvsi2(int a) { if (a == -2147483647 - 1) abort(); return a < 0 ? -a : a; }
long long __addvdi3(long long a, long long b) { long long r; if (__builtin_add_overflow(a, b, &r)) abort(); return r; }
long long __subvdi3(long long a, long long b) { long long r; if (__builtin_sub_overflow(a, b, &r)) abort(); return r; }
long long __mulvdi3(long long a, long long b) { long long r; if (__builtin_mul_overflow(a, b, &r)) abort(); return r; }
long long __negvdi2(long long a) { if (a == -9223372036854775807LL - 1) abort(); return -a; }
long long __absvdi2(long long a) { if (a == -9223372036854775807LL - 1) abort(); return a < 0 ? -a : a; }
