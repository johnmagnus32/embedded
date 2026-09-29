#include "aabi.h"
int f_va(int n, ...) { va_list ap; __builtin_va_start(ap, n); int s = 0; while (n--) s = s * 10 + __builtin_va_arg(ap, int); __builtin_va_end(ap); return s; }
int f_sa(int x, struct SA s) { return x * 100 + s.a * 10 + s.b; }
int f_sl(int x, struct SL s) { return x * 100 + s.a * 10 + (int)s.b; }
int f_sm(int x, struct SM s) { return x * 100 + s.a * 10 + s.b; }
int f_sp(int x, struct SP s) { return x * 100 + s.c * 10 + (int)s.x; }
int f_vsa(int n, ...) { va_list ap; __builtin_va_start(ap, n); struct SA s = __builtin_va_arg(ap, struct SA); int t = __builtin_va_arg(ap, int); __builtin_va_end(ap); return n * 1000 + s.a * 100 + s.b * 10 + t; }
__attribute__((pcs("aapcs"))) double f_cd(int x, _Complex double z) { return x * 100 + __real__ z * 10 + __imag__ z; }
__attribute__((pcs("aapcs"))) double f_ad(int x, a16d d) { return x * 10 + d; }
