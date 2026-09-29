/* musl-math.c — musl's math against OUR toolchain: the functions it implements in ARM inline asm with VFP register
 * operands (sqrt/fabs/fma and their float forms — "w"/"t" constraints, %P), plus software ones (pow/log/sin) and
 * printf's %f formatting. One line, so the harness can compare it exactly. */
#include <math.h>
#include <stdio.h>
int main(void)
{
	volatile double x = 2.0, y = -1.5;
	volatile float fx = 2.0f, fy = -3.5f;
	printf("musl-math %.6f %.6f %.6f %.6f %.6f %.5f %.5f %.5f\n",
	       sqrt(x), fabs(y), fma(3.0, 4.0, 5.0), pow(x, 10.0), log(x),
	       (double)sqrtf(fx), (double)fabsf(fy), (double)fmaf(2.0f, 3.0f, 1.0f));
	return 0;
}
