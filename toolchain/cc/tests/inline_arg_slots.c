// expect: 42
// An inlined call's parameters never share a frame slot with its arguments' own code: `copy(dst, pick(i))` stores
// dst, then runs the inlined pick() (its locals) to get the second argument — parsed, and its slots freed, before.
int g4 = 4, g1 = 1, g50 = 50, g8 = 8;
static inline __attribute__((always_inline)) int pick(int i) { int t = i * 3; t -= 1; return t; }
static inline __attribute__((always_inline)) int copy(int d, int s) { return d * 100 + s; }
static inline __attribute__((always_inline)) int two(int a, int b) { int x = a, y = b; return x - y; }
int main(void) {
	int r = copy(g4, pick(g1));                                      /* 402 */
	int q = two(({ int u = g50; u; }), ({ int v = g8; v; }));       /* 42 */
	return r == 402 ? q : 1;
}
