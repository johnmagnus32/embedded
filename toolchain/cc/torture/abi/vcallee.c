#include "vabi.h"
int f_q(float s, v4si a, int x, v2si b, v4si c) { return a[1] + b[1] * 10 + c[1] * 100 + x * 1000 + (int)s * 10000; }
v4sf f_mul(v4sf a, v4sf b) { return a * b; }
v2si f_small(v2hi a, int x) { v2si r = { a[0] + x, a[1] + x }; return r; }
v8si f_big(v8si a, int x) { a[0] += x; a[7] -= x; return a; }
struct hva2 f_hva(struct hva2 s, float k) { struct hva2 r = { s.b, s.a + (int)k }; return r; }
v4si f_var(int n, ...) { va_list ap; __builtin_va_start(ap, n); v4si r = { 0, 0, 0, 0 }; for (int i = 0; i < n; i++) r += __builtin_va_arg(ap, v4si); __builtin_va_end(ap); return r; }
__attribute__((pcs("aapcs"))) v4si f_base(v2si a, v4si b) { v4si r = { a[0], a[1], b[2], b[3] }; return r + b; }
float f_spill(struct hvq q, v4si c, v4si d, v2si e, float f) { return q.a[0] + q.b[3] + c[1] + d[2] + e[0] * 10 + e[1] * 100 + f; }
