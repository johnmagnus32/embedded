// expect: 16
// Returns-twice calls (setjmp, by name or __attribute__((returns_twice))): the second return — a longjmp after the
// code went on and reused the stack below its frame (clobber) — must find the frame and the assignment target intact.
// A minimal setjmp/longjmp pair in musl's ARM layout (r4-r11, sp, lr): the test has no libc.
__asm__(".text\n.global setjmp\n.type setjmp,%function\nsetjmp:\n.global snap\n.type snap,%function\nsnap:\n"
        "\tmov ip, r0\n\tstmia ip!, {r4-r11}\n\tmov r2, sp\n\tstmia ip!, {r2, lr}\n\tmov r0, #0\n\tbx lr\n"
        ".global longjmp\n.type longjmp,%function\nlongjmp:\n"
        "\tmov ip, r0\n\tmovs r0, r1\n\tmoveq r0, #1\n\tldmia ip!, {r4-r11}\n\tldmia ip!, {r2, lr}\n\tmov sp, r2\n\tbx lr\n");
typedef int jmp_buf[10];
int setjmp(int *b);
void longjmp(int *b, int v) __attribute__((noreturn));
__attribute__((returns_twice)) int snap(int *b);
static jmp_buf jb;
static __attribute__((noinline)) void jump(int v) { longjmp(jb, v); }
static __attribute__((noinline)) int clobber(int a, int b, int c, int d, int e, int f, int g, int h) {   /* overwrites the stack below */
	volatile int big[64]; for (int i = 0; i < 64; i++) big[i] = -1;
	return a + b + c + d + e + f + g + h + big[3];
}
struct S { int pad, v; };
static __attribute__((noinline)) int pick(int a, int b, int c) { return a * 100 + b * 10 + c; }
static __attribute__((noinline)) long long pick8(int a, int b, int c, int d, int e, int f, long long g, int h) { return a + b + c + d + e + f + g * 1000 + h; }
static void *bb[5];
static __attribute__((noinline)) void bjump(void) { __builtin_longjmp(bb, 1); }

static int t_assign(void) { volatile int n = 0; int i; i = setjmp(jb); clobber(1, 2, 3, 4, 5, 6, 7, 8); if (!i) { n++; jump(7); } return i == 7 && n == 1; }
static int t_member(struct S *p) { volatile int n = 0; p->v = setjmp(jb); clobber(1, 2, 3, 4, 5, 6, 7, 8); if (!p->v) { n++; jump(9); } return p->v == 9 && p->pad == 42 && n == 1; }
static int t_cmp(void) { volatile int n = 0; if (setjmp(jb) == 0) { clobber(1, 2, 3, 4, 5, 6, 7, 8); n++; jump(3); } return n == 1; }
static int t_switch(void) { volatile int r = 0; switch (setjmp(jb)) { case 0: clobber(1, 2, 3, 4, 5, 6, 7, 8); jump(5); case 5: r = 1; break; default: r = 2; } return r == 1; }
static int t_wide(void) { long long w; w = setjmp(jb); clobber(1, 2, 3, 4, 5, 6, 7, 8); if (!w) jump(-2); return w == -2; }
static int t_attr(void) { signed char c; c = snap(jb); clobber(1, 2, 3, 4, 5, 6, 7, 8); if (!c) jump(0x105); return c == 5; }
static int t_loop(void) { volatile int k = 0; while (setjmp(jb) < 3) { k++; clobber(1, 2, 3, 4, 5, 6, 7, 8); jump(k); } return k == 3; }
static int t_init(void) { volatile int n = 0; int r = setjmp(jb); clobber(1, 2, 3, 4, 5, 6, 7, 8); if (!r) { n++; jump(4); } return r == 4 && n == 1; }
static int t_not(void) { volatile int n = 0; if (!setjmp(jb)) { n++; clobber(1, 2, 3, 4, 5, 6, 7, 8); jump(1); } return n == 1; }
static int t_cast(void) { volatile int n = 0; (void)setjmp(jb); if (n++ == 0) { clobber(1, 2, 3, 4, 5, 6, 7, 8); jump(1); } return n == 2; }
static int t_nested(void) { volatile int n = 0; int x; if ((x = setjmp(jb)) == 0) { clobber(1, 2, 3, 4, 5, 6, 7, 8); n++; jump(6); } return x == 6 && n == 1; }
static int t_stmtexpr(void) { volatile int n = 0; int x; x = ({ int p = setjmp(jb); clobber(1, 2, 3, 4, 5, 6, 7, 8); p; }); if (!x) { n++; jump(8); } return x == 8 && n == 1; }   /* BusyBox's xvfork() */

static int t_arg(void) { volatile int n = 0; int x = pick(1, setjmp(jb), 3); clobber(1, 2, 3, 4, 5, 6, 7, 8); if (x == 103) { n++; jump(7); } return x == 173 && n == 1; }   /* gcc pr89280 */
static int t_stackarg(void) { volatile int n = 0; long long x = pick8(1, 2, 3, 4, 5, 6, setjmp(jb), 8); clobber(1, 2, 3, 4, 5, 6, 7, 8); if (x == 29) { n++; jump(5); } return x == 5029 && n == 1; }
static int t_builtin_arg(void) { volatile int n = 0; int x = pick(1, __builtin_setjmp(bb), 3); clobber(1, 2, 3, 4, 5, 6, 7, 8); if (x == 103) { n++; bjump(); } return x == 113 && n == 1; }
static int t_builtin_assign(void) { volatile int n = 0; struct S s = { 0, 0 }, *p = &s; p->v = __builtin_setjmp(bb); clobber(1, 2, 3, 4, 5, 6, 7, 8); if (!p->v) { n++; bjump(); } return p->v == 1 && n == 1; }

int main(void) {
	struct S s = { 42, -1 };
	return t_assign() + t_member(&s) + t_cmp() + t_switch() + t_wide() + t_attr() + t_loop() + t_init() + t_not() + t_cast() + t_nested() + t_stmtexpr()
	     + t_arg() + t_stackarg() + t_builtin_arg() + t_builtin_assign();
}
