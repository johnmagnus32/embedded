// expect: 42
// GNU `a ?: b` evaluates a ONCE (side effects, an inlined call's labels): a ? a : b through a temporary.
int cnt;
static int bump(void) { return ++cnt; }
static int zero(void) { cnt += 10; return 0; }
static inline __attribute__((always_inline)) int g(int x) { if (x > 3) return x; return 0; }
int main(void) {
	int a = bump() ?: 9;                       // 1, cnt 1
	int b = zero() ?: 7;                       // 7, cnt 11
	int c = g(cnt) ?: 5;                       // 11
	int d = g(2) ?: 5;                         // 5
	return a == 1 && b == 7 && c == 11 && d == 5 && cnt == 11 ? 42 : 1;
}
