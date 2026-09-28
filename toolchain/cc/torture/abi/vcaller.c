#include "vabi.h"
int main(void) {
	int r = 0;
	v4si a = { 1, 2, 3, 4 }, c = { 5, 6, 7, 8 }; v2si b = { 9, 3 };
	r += f_q(4.0f, a, 5, b, c) == 2 + 30 + 600 + 5000 + 40000;
	v4sf x = { 1.5f, 2, 3, 4 }, y = { 2, 3, 4, 0.5f }, z = f_mul(x, y);
	r += (z[0] == 3 && z[1] == 6 && z[2] == 12 && z[3] == 2) << 1;
	v2hi h = { -3, 7 }; v2si s = f_small(h, 10); r += (s[0] == 7 && s[1] == 17) << 2;
	v8si big = { 1, 2, 3, 4, 5, 6, 7, 8 }, bb = f_big(big, 100); r += (bb[0] == 101 && bb[3] == 4 && bb[7] == -92) << 3;
	struct hva2 hv = { { 1, 2 }, { 3, 4 } }, hr = f_hva(hv, 10.0f); r += (hr.a[0] == 3 && hr.a[1] == 4 && hr.b[0] == 11 && hr.b[1] == 12) << 4;
	v4si va = f_var(3, a, c, a); r += (va[0] == 7 && va[3] == 16) << 5;
	v4si bs = f_base(b, c); r += (bs[0] == 14 && bs[1] == 9 && bs[2] == 14 && bs[3] == 16) << 6;
	struct hvq q = { { 1, 0, 0, 0 }, { 0, 0, 0, 2 } }; v2si e = { 3, 4 };
	r += (f_spill(q, c, c, e, 0.5f) == 1 + 2 + 6 + 7 + 30 + 400 + 0.5f) << 7;
	return r;
}
