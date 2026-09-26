// expect: 31
/* Declaration attributes: aligned(N) on a member / typedef changes the layout (as GCC lays it out);
 * weak definitions and weak prototypes link and run; section() objects are real, addressable data (the
 * kernel builds initcall tables this way); alias() names the same object. */
struct m { char c; int x __attribute__((aligned(16))); };
typedef int aint __attribute__((aligned(8)));
struct n { char c; aint y; };
struct s1 { int __attribute__((aligned(8))) a; };
struct { char c; struct s1 m; } outer;
int wv __attribute__((weak)) = 7;
int wf(void) __attribute__((weak));
int wf(void) { return 3; }
static int (*const table[])(void) __attribute__((section(".mytable"), used)) = { wf, wf };
int base[4] = { 1, 2, 3, 4 };
extern int same[4] __attribute__((alias("base")));
int main(void) {
	int r = 0;
	r += __builtin_offsetof(struct m, x) == 16 && sizeof(struct m) == 32;             /* 1 */
	r += (__builtin_offsetof(struct n, y) == 8 && _Alignof(aint) == 8) << 1;          /* 2 */
	r += (((int)&outer.m & 7) == 0 && __builtin_offsetof(struct s1, a) == 0) << 2;    /* 4 */
	r += (wv == 7 && wf() == 3 && table[1]() == 3) << 3;                              /* 8 */
	same[2] = 9; r += (base[2] == 9) << 4;                                            /* 16 */
	return r;
}
