/* vabi.h — AAPCS-VFP interop for GCC generic vectors: every function is compiled by one compiler and called from
 * the other. 64/128-bit vectors are VFP candidates (d/q registers; a q register q-aligned; back-filling); smaller ones
 * go in core registers; bigger ones are composites (sret, 8-aligned, split r3 | stack); variadic / pcs("aapcs") use
 * the base PCS (a vector of up to 16 bytes returned in r0-r3). */
typedef __builtin_va_list va_list;
typedef int v4si __attribute__((vector_size(16)));
typedef int v2si __attribute__((vector_size(8)));
typedef short v2hi __attribute__((vector_size(4)));
typedef int v8si __attribute__((vector_size(32)));
typedef float v4sf __attribute__((vector_size(16)));
struct hva2 { v2si a, b; };                       /* a homogeneous vector aggregate: d0-d1 */
struct hvq { v4si a, b; };                        /* ...of q registers: q0-q1 */
int f_q(float s, v4si a, int x, v2si b, v4si c);  /* s0 | a q1 | x r0 | b d1 (back-filled) | c q2 */
v4sf f_mul(v4sf a, v4sf b);                       /* q0, q1 -> q0 */
v2si f_small(v2hi a, int x);                      /* a in r0 -> d0 */
v8si f_big(v8si a, int x);                        /* sret r0; a r2-r3 | stack; x on the stack */
struct hva2 f_hva(struct hva2 s, float k);
v4si f_var(int n, ...);                           /* va_arg of a vector; returned in r0-r3 */
__attribute__((pcs("aapcs"))) v4si f_base(v2si a, v4si b);   /* a r0:r1 | b r2-r3 + stack; r0-r3 */
float f_spill(struct hvq q, v4si c, v4si d, v2si e, float f);   /* q0-q3 taken: e and f go to the stack */
