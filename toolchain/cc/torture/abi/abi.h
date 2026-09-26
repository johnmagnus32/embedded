/* abi.h — AAPCS-VFP interop cases: every function is compiled by one compiler and called from the other. */
typedef __builtin_va_list va_list;
struct hf2 { float x, y; }; struct hd3 { double a, b, c; }; struct mix { float f; int i; };
double f_mix(int a, double b, float c, int d, float e, double f);             /* back-fill: c->s0? */
float f_many(float a0,float a1,float a2,float a3,float a4,float a5,float a6,float a7,float a8,float a9,float a10,float a11,float a12,float a13,float a14,float a15,float a16, double d17, int i);
struct hf2 f_hfa(struct hf2 p, struct hd3 q, float s);
struct hd3 f_hd3(double k);
double f_va(int n, ...);
struct mix f_mixs(struct mix m, double z);
double f_big(double d0, double d1, double d2, double d3, double d4, double d5, double d6, double d7, struct hd3 spill, float after);
struct s16 { int v[16]; };
double f_split(int pa, struct s16 pb, double pc, int pd);   /* a struct split r1-r3 | stack, then d0 */
