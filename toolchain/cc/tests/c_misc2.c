// expect: 255
/* Wide literals (wchar_t = unsigned int, UTF-8 decoded, \400 > 255); typedef with several declarators;
 * postfix on a compound literal; GNU `member: value` designators; file-scope compound literals and address
 * differences in static data; &a[i][j] static addresses; __builtin_setjmp/longjmp; GNU keyword spellings. */
typedef unsigned int wchar_t;
wchar_t ws[] = L"a\x3b1";
typedef struct { int x, y; } pt, *ppt;
struct q { int a, b; } oq = { b: 5, a: 4 };
struct pt2 { int x, y; } *gp = &(struct pt2){ 7, 8 };
int m2[4][5], *mp = &m2[2][3];
static void *jb[5];
static void jump(void) { __builtin_longjmp(jb, 1); }
static int labdiff(int k) { static const int d[] = { &&l1 - &&l0, &&l2 - &&l0 }; goto *(&&l0 + d[k]); l0: return 0; l1: return 1; l2: return 2; }
int main(void) {
	int r = 0;
	r += (sizeof ws == 12 && ws[1] == 0x3b1 && L'\400' == 256 && sizeof(L"ab") == 12);   /* 1 */
	pt p = { 1, 2 }; ppt pp = &p; r += (pp->y == 2) << 1;                            /* 2 */
	r += (((int[]){ 5, 6, 7 })[2] == 7) << 2;                                        /* 4 */
	r += (oq.a == 4 && oq.b == 5) << 3;                                              /* 8 */
	r += (gp->y == 8 && mp == &m2[2][3]) << 4;                                       /* 16 */
	volatile int hits = 0;
	if (__builtin_setjmp(jb) == 0) { hits++; jump(); } else hits += 10;
	r += (hits == 11) << 5;                                                          /* 32 */
	r += (labdiff(0) == 1 && labdiff(1) == 2) << 6;                                  /* 64: d[k] = l(k+1) - l0 */
	__volatile int vv = 3; __asm volatile("" : "+r"(vv)); r += (vv == 3) << 7;       /* 128 */
	return r;
}
