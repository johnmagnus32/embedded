#include "aabi.h"
int main(void) {
	int r = 0; a8i p = 1, q = 2, w = 3;
	r += f_va(3, p, q, w) == 123;
	struct SA sa = { 4, 5 }; r += (f_sa(3, sa) == 345) << 1;
	struct SL sl = { 4, 5 }; r += (f_sl(3, sl) == 345) << 2;
	struct SM sm = { 4, 5 }; r += (f_sm(3, sm) == 345) << 3;
	struct SP sp = { 4, 5 }; r += (f_sp(3, sp) == 345) << 4;
	r += (f_vsa(1, sa, 6) == 1456) << 5;
	r += (f_cd(3, 4.0 + 5.0i) == 345.0) << 6;
	a16d d = 7.0; r += (f_ad(3, d) == 37.0) << 7;
	return r;
}
