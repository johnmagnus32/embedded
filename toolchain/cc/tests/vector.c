// expect: 42
/* GCC generic vectors: lane-wise ops, a scalar broadcast, a comparison mask, op=, lanes, a reinterpreting cast and
 * a shuffle (the full differential check against GCC is torture/diff/vector.c). */
typedef int v4si __attribute__((vector_size(16)));
typedef short v4hi __attribute__((vector_size(8)));
typedef long long v1di __attribute__((vector_size(8)));
static v4si twice(v4si a) { return a + a; }
int main(void) {
	v4si a = { 1, 2, 3, 4 }, b = twice(a) * 3 - 1;           /* 5 11 17 23 */
	v4si m = b > 10;                                        /* 0 -1 -1 -1 */
	b += m & 100;                                           /* 5 111 117 123 */
	v4hi h = { -1, 2, -3, 4 }; h >>= 1;                     /* -1 1 -2 2 */
	long long x = (long long)(v4hi){ 1, 0, 0, 0 };          /* 1 */
	v4si s = __builtin_shuffle(b, (v4si){ 3, 2, 1, 0 });    /* 123 117 111 5 */
	b[0] = 7;
	if (s[0] != 123 || s[3] != 5 || b[0] != 7 || b[1] != 111 || h[2] != -2 || h[3] != 2 || x != 1) return 1;
	return s[3] + h[1] + b[0] * 5 + 1;                     /* 5 + 1 + 35 + 1 */
}
