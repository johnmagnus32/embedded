// expect: 20
// Value ranges and known-zero bits: what they decide must hold at run time — through wraps, loops that change their
// counter, joins of conditional stores, signed/unsigned comparisons. `on` is 1 at run time, unknown to the compiler.
int on_ = 1;
static int flag(void) { return on_; }
static inline __attribute__((always_inline)) int below(unsigned idx, unsigned n) { return idx < n; }

int main(void) {
	int r = 0, on = flag(), off = !on;
	{ int s = 0; for (int i = 0; i < 10; i++) s += below(i, 10); r += s == 10; }                 // i in [0, 9]
	{ int s = 0; for (int i = 0; i < 10; i++) { s += i >= 10; i += 2; } r += s == 0; }            // changes i: no range
	{ int n = 0; for (int i = 0; i < 10; i++) { if (i == 9) n = 1; } r += n == 1; }               // the top of the range
	{ unsigned u = on ? 5 : 7; r += (u == 6) == 0 && (u == 5) == 1; }                            // in {5, 7}: not decided
	{ int s = on ? -1 : 1; r += (s > 0) == 0 && (s != 0) == 1; }                                 // [-1, 1]
	{ unsigned char c = on ? 250 : 5; c += 10; r += c == 4; }                                     // wraps in its type
	{ unsigned char c = off ? 250 : 5; c += 10; r += c == 15; }
	{ int f = on ? 1 : 0; if (off) f |= 2; r += f <= 3 && f == 1; }                               // a join: [0, 3]
	{ int f = off ? 1 : 0; if (on) f |= 2; r += f == 2; }
	{ unsigned m = (unsigned)flag() & 7; r += m < 8 && m == 1; }                                  // & mask
	{ int x = -flag(); unsigned y = (unsigned)x; r += y > 5; }                                    // negative as unsigned
	{ int x = on ? -3 : 2; r += (x < 0u) == 0; }                                                  // compared unsigned
	{ int i; for (i = 0; i < 5; i++) if (on && i == 2) break; r += i == 2; }                      // left early
	{ int i; for (i = 0; i < 5; i++) ; r += i == 5; }                                             // after the loop
	{ long long v = on ? 1LL << 40 : 0; r += (v >> 40) == 1; }                                    // 64-bit
	{ unsigned long long v = on ? ~0ULL : 0; r += (v >> 63) == 1; }                               // u64: bits only
	{ int k = on ? 4 : 8; r += ((k & 3) == 0) && ((k & 4) != 0); }                                // known-0 bits
	{ int s = 0; for (unsigned i = 3; i <= 5; i++) s += i; r += s == 12; }                        // <=
	{ int s = 0; for (int i = 0; i != 4; i++) s += below(i, 4); r += s == 4; }                    // !=
	{ int b = (on ? 0x4 : 0) | 0x1 | 0x10; r += (b & 0x8000) == 0 && (b & 0x4) == 4; }           // bits of a ?:
	return r;
}
