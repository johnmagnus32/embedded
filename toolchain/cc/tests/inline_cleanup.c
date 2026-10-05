// expect: 42
// A function with cleanup variables (the kernel's guard()/scoped_guard in inline helpers) is never expanded inline:
// an expansion's `return` jumps to its end, past the scope's cleanups. Called with constants — which would inline it —
// each cleanup still runs exactly once, at the right time.
int locks, unlocks;
static void unlock(int *p) { (void)p; unlocks++; }
static inline __attribute__((always_inline)) int guarded(int k) { int g __attribute__((cleanup(unlock))) = locks++; if (k) return 10; return 20; }
static int plain(int k) { int g __attribute__((cleanup(unlock))) = locks++; if (k == 1) return 5; { int h __attribute__((cleanup(unlock))) = 0; (void)h; } return 7; }
int main(void) {
	int r = guarded(1) + guarded(0) + plain(1) + plain(2);   /* 10 + 20 + 5 + 7 */
	return r == 42 && locks == 4 && unlocks == 5 ? 42 : 1;
}
