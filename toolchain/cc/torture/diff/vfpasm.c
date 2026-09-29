/* vfpasm.c — VFP register operands in extended asm (ARM "w" = a double in a d register, "t" = a float in an s
 * register; %P prints the double name): musl's src/math/arm/{fabs,fabsf,sqrt,sqrtf,fma,fmaf}.c are exactly these.
 * Covers =w/w, =t/t, +w and +t (a tied output), several operands at once, and the values round-tripping through
 * the core registers our codegen keeps them in. Must match GCC. */
static double myfabs(double x) { __asm__ ("vabs.f64 %P0, %P1" : "=w"(x) : "w"(x)); return x; }
static float myfabsf(float x) { __asm__ ("vabs.f32 %0, %1" : "=t"(x) : "t"(x)); return x; }
static double mysqrt(double x) { __asm__ ("vsqrt.f64 %P0, %P1" : "=w"(x) : "w"(x)); return x; }
static float mysqrtf(float x) { __asm__ ("vsqrt.f32 %0, %1" : "=t"(x) : "t"(x)); return x; }
static double myfma(double x, double y, double z) { __asm__ ("vfma.f64 %P0, %P1, %P2" : "+w"(z) : "w"(x), "w"(y)); return z; }
static float myfmaf(float x, float y, float z) { __asm__ ("vfma.f32 %0, %1, %2" : "+t"(z) : "t"(x), "t"(y)); return z; }
int main(void) {
	printf("%d %d %d %d %d %d\n", (int)(myfabs(-2.5) * 2), (int)(myfabsf(-3.5f) * 2), (int)mysqrt(144.0),
	       (int)mysqrtf(81.0f), (int)myfma(3.0, 4.0, 5.0), (int)myfmaf(2.0f, 3.0f, 1.0f));
	return 0;
}
