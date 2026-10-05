// expect: 42
// Addresses compare UNSIGNED — a kernel's are >= 0x80000000, where a signed compare puts them below NULL (the
// kernel's unflatten_dt_nodes: `if (pa < ps)` with pa NULL); a pointer DIFFERENCE is still a signed ptrdiff_t.
int main(void) {
	int r = 0;
	char *hi = (char *)0xc6f01000u, *lo = (char *)0x40000000u, *nul = 0;
	volatile unsigned int *vp = (unsigned int *)0xff87b260u;
	r += nul < hi;                                  /* 1 */
	r += (hi > lo) * 2;                             /* 3 */
	r += (lo <= hi) * 4;                            /* 7 */
	r += ((void *)vp >= (void *)hi) * 8;            /* 15 */
	int a[8]; int *p = &a[6], *q = &a[1];
	r += ((q - p) == -5) * 16;                      /* 31: signed difference */
	r += ((q - p) / 5 == -1) * 11;                  /* 42 */
	return r;
}
