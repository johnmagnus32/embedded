// expect: 0
// C11 6.7.4p7: a file-scope definition all of whose declarations say `inline` and none `extern` is an INLINE
// definition — no external definition here, the call goes to another unit's (which is why GCC never even checks such
// a body). Any `extern` or non-inline declaration of the name, or `static`, makes it a real definition here.
// This file only needs the ones that ARE emitted to work; the inline-only one is declared and called from a
// definition provided below under its asm label, exactly as another unit would provide it.
static inline int si(int x) { return x + 1; }            // static inline: emitted, callable
extern int ei(int);
inline int ei(int x) { return x + 2; }                   // `extern` declared -> a real definition here
inline int io(int);                                      // inline-only: NOT defined here...
int io_impl(int x) __asm__("io");                        // ...so this provides the symbol, as another unit would
int io_impl(int x) { return x + 3; }
extern __inline __attribute__((__gnu_inline__)) int gi(int x) { return x + 4; }   // GNU inline: not emitted
int gi_impl(int x) __asm__("gi");
int gi_impl(int x) { return x + 40; }
int main(void)
{
	if (si(1) != 2) return 1;
	if (ei(1) != 3) return 2;
	if (io(1) != 4) return 3;
	if (gi(1) != 41) return 4;   // the external definition, not the inline body
	return 0;
}
