// expect: 127
/* Function pointers are typed: *fp is the function (no load) and an indirect call has the real return type
 * (64-bit / struct); arrays of function pointers; alloca inside call arguments (the live temporaries slide);
 * brace elision; GNU cast-to-union; asm "+r" evaluates its lvalue once and "0" matches output 0; the stack
 * is 8-aligned at calls (a doubleword va_arg after an odd number of pushed temporaries). */
typedef __builtin_va_list va_list;
struct big { int a[5]; };
struct pt { int x, y; };
union u { long long ll; int i; };
static long long ll(void) { return 0x100000002LL; }
static struct big mk(void) { struct big b = { { 1, 2, 3, 4, 5 } }; return b; }
static int g1(void) { return 1; }
static int (*tbl[])(void) = { g1, g1 };
static int sum3(int a, char *p, int c) { p[0] = 7; return a + p[0] + c; }
static int calls;
static int *once(int *p) { calls++; return p; }
static long long va64(int n, ...) { va_list ap; __builtin_va_start(ap, n); long long v = __builtin_va_arg(ap, long long); __builtin_va_end(ap); return v; }
int main(void) {
	int r = 0;
	long long (*p)(void) = ll; struct big (*m)(void) = mk;
	r += ((*p)() == 0x100000002LL && m().a[4] == 5 && (*tbl[1])() == 1);                  /* 1 */
	r += (sum3(1, __builtin_alloca(8), 2) == 10) << 1;                                    /* 2 */
	struct pt pts[] = { 1, 2, 3, 4 }; r += (sizeof pts == 16 && pts[1].x == 3) << 2;       /* 4 */
	union u uu = (union u) 5LL; r += (uu.ll == 5) << 3;                                   /* 8 */
	int v = 4; __asm__("add %0, %0, #1" : "+r"(*once(&v))); r += (v == 5 && calls == 1) << 4;   /* 16 */
	int o; long long in = 41; __asm__("add %0, %1, #1" : "=r"(o) : "0"(in)); r += (o == 42) << 5;   /* 32 */
	r += ((1 + va64(1, 0x300000004LL)) == 0x300000005LL) << 6;                             /* 64: called with a temp pushed */
	return r;
}
