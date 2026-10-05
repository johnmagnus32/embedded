// expect: 35
// Flow-sensitive local constants: a known value must never survive into code a loop, a jump, a condition, an
// address or an asm can reach with another. `on` is 1 at run time, unknown to the compiler; each check scores 1.
int on_ = 1;
static int flag(void) { return on_; }

int main(void) {
	int r = 0, on = flag(), off = !on;

	{ int v = 0; for (int i = 0; i < 3; i++) v += i; r += v == 3; }                              // loop-carried
	{ int v = 1, n = 0; while (v < 100) { v *= 2; n++; } r += v == 128 && n == 7; }               // while
	{ int v = 0, k = 0; again: k++; if (k < 5) { v = k; goto again; } r += v == 4; }            // backward goto
	{ int v = 1; if (on) goto fwd; v = 2; fwd: r += v == 1; }                                     // forward goto
	{ int v = 1; switch (on) { case 1: v = 5; break; case 2: v = 6; } r += v == 5; }             // switch
	{ int v = 1; int *p = &v; *p = 7; r += v == 7; }                                              // address taken
	{ int v = 1; r += v == 1; int *p = &v; *p = 3; r += v == 3; }                                 // ... later
	{ int v = 1; if (on) goto into; if (0) { into: v = 9; } else { v = 3; } r += v == 9; }        // into a dead branch
	{ int v = 1; if (on) goto into2; if (1) { v = 2; } else { into2: v = 8; } r += v == 8; }      // into a dead else
	{ int v = 1; do { if (on) break; v = 2; } while (0); r += v == 1; }                           // do-while(0) + break
	{ int v = 1; do { v = 2; } while (0); r += v == 2; }                                          // do-while(0)
	{ int v = 1; if (on && (v = 5)) ; r += v == 5; }                                              // && arm
	{ int v = 1; if (off || (v = 6)) ; r += v == 6; }                                             // || arm
	{ int v = 1; int w = on ? (v = 2) : 3; r += v == 2 && w == 2; }                               // ?: arm
	{ int v = 1; int s = sizeof(v = 5); r += v == 1 && s == 4; }                                  // sizeof: unevaluated
	{ int v = 1; (void)__builtin_constant_p(v = 5); r += v == 1; }                                // unevaluated
	{ int v = 1; typeof(v = 5) w = 0; r += v == 1 && w == 0; }                                    // typeof: unevaluated
	{ int v = 5; int w = v++; r += w == 5 && v == 6; }                                            // postfix
	{ char *p = 0; p += 4; r += (long)p == 4; }                                                   // pointer step
	{ int v = 1; void g(void) { v = 10; } g(); r += v == 10; }                                    // nested function
	{ int v = 1; __asm__("mov %0, #7" : "=r"(v)); r += v == 7; }                                  // asm output
	{ int v = 0; for (;;) { v = 3; break; } r += v == 3; }                                        // for + break
	{ int v = 1; { int v = 2; (void)v; } r += v == 1; }                                           // shadowing
	{ int s = 0; for (int i = 0; i < 3; i++) { int t = i; t += 1; s += t; } r += s == 6; }        // per-iteration local
	{ int v = 1; int x = on ? ({ v = 4; 1; }) : 0; r += v == 4 && x == 1; }                       // ({}) in an arm
	{ unsigned char c = 250; c += 10; r += c == 4; }                                              // wraps in its type
	{ _Bool b = 0; b += 2; r += b == 1; }                                                         // _Bool
	{ int v = 1; do { for (;;) break; v = 2; } while (0); r += v == 2; }                          // inner break
	{ int v = 1; switch (on) { case 0: v = 2; if (0) { case 1: v = 3; } break; } r += v == 3; }   // case in a dead if
	{ void *t = &&l2; int v = 1; goto *t; v = 2; l2: r += v == 1; }                               // computed goto
	{ int v = 0, s = 0; for (int i = 0; i < 3; v = i, i++) s += v; r += s == 1; }                 // for increment
	{ int n = 3, c = 0; while (n--) c++; r += c == 3 && n == -1; }                                // while (n--)
	{ unsigned long long m = ~0ULL / 1000000000u; m += 7; m /= 3; r += m == (~0ULL / 1000000000u + 7) / 3; }   // u64
	{ int v = on; v = 4; r += __builtin_constant_p(v) && v == 4; }                                // a known value
	return r;
}
