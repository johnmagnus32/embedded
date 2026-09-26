#include "abi.h"
int main(void) {
	int r = 0;
	r += f_mix(1, 2.0, 3.0f, 4, 5.0f, 6.0) == 654321.0;
	r += (f_many(1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,3, 0.5, 1) == 1 + 4 + 12 + 4 + 16) << 1;
	struct hf2 p = { 1.5f, 2.0f }; struct hd3 q = { 0, 0, 0.5 };
	struct hf2 h = f_hfa(p, q, 3.0f); r += (h.x == 2.0f && h.y == 6.0f) << 2;
	struct hd3 t = f_hd3(1.25); r += (t.a == 1.25 && t.c == 3.75) << 3;
	r += (f_va(3, 1.0, 2.5, (double)3.5f) == 7.0) << 4;
	struct mix m = { 1.0f, 41 }; struct mix mm = f_mixs(m, 0.5); r += (mm.f == 1.5f && mm.i == 42) << 5;
	struct hd3 sp = { 1, 2, 3 };
	r += (f_big(1, 0, 0, 0, 0, 0, 0, 2, sp, 0.5f) == 1 + 4 + 4 + 24 + 8) << 6;
	struct s16 s; for (int i = 0; i < 16; i++) s.v[i] = i + 1;
	r += (f_split(1, s, 10000.0, 3) == 13136.0) << 7;
	return r;
}
