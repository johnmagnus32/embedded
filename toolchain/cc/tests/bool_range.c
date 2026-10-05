// expect: 42
// Converting to _Bool is `!= 0`, not a truncation to bit 0 — also for what the value ranges know: `flags & 2` has bit 0
// known clear, yet `bool b = flags & 2` is true when bit 1 is set (the kernel's reset_control optional flag).
int gflags = 3;
static inline __attribute__((always_inline)) int is_opt(int flags) { _Bool optional = flags & 2; return optional ? 40 : 1; }
int main(void) {
	int f = gflags;
	_Bool a = f & 2, b = (f & 4) != 0, c = (unsigned char)(f << 7);   /* 1, 0, 1 */
	return is_opt(f) + a + !b + (c ? 0 : 100) == 42 ? 42 : 1;
}
