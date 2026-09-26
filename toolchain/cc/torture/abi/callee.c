#include "abi.h"
double f_mix(int a, double b, float c, int d, float e, double f) { return a + b * 10 + c * 100 + d * 1000 + e * 10000 + f * 100000; }
float f_many(float a0,float a1,float a2,float a3,float a4,float a5,float a6,float a7,float a8,float a9,float a10,float a11,float a12,float a13,float a14,float a15,float a16, double d17, int i)
{ return a0 + a15 * 2 + a16 * 4 + (float)d17 * 8 + i * 16; }
struct hf2 f_hfa(struct hf2 p, struct hd3 q, float s) { struct hf2 r = { p.x + (float)q.c, p.y * s }; return r; }
struct hd3 f_hd3(double k) { struct hd3 r = { k, k * 2, k * 3 }; return r; }
double f_va(int n, ...) { va_list ap; __builtin_va_start(ap, n); double s = 0; for (int i = 0; i < n; i++) s += __builtin_va_arg(ap, double); __builtin_va_end(ap); return s; }
struct mix f_mixs(struct mix m, double z) { struct mix r = { m.f + (float)z, m.i + 1 }; return r; }
double f_big(double d0, double d1, double d2, double d3, double d4, double d5, double d6, double d7, struct hd3 spill, float after)
{ return d0 + d7 * 2 + spill.a * 4 + spill.c * 8 + after * 16; }
double f_split(int pa, struct s16 pb, double pc, int pd) { double s = pc + pd * 1000; for (int i = 0; i < 16; i++) s += pb.v[i]; return s; }
