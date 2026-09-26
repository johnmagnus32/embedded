// expect: 255
/* VLAs: 1-D / 2-D objects, sizeof at runtime, a VLA typedef sized where it is declared, a VLA parameter
 * using an earlier parameter (its bound is evaluated at entry, side effects included), and a VLA in a loop
 * reusing its storage (re-running the declaration frees the previous instance). K&R definitions. */
static int sum2(int n, int m, int a[n][m]) { int s = 0; for (int i = 0; i < n; i++) for (int j = 0; j < m; j++) s += a[i][j]; return s; }
static int bump(int i, int a[i++]) { return i; }   /* the bound's i++ runs at entry */
static int knr(a, b, c) int a; char *b; long long c; { return a + *b + (int)(c >> 32); }
int main(void) {
	int r = 0, n = 3, m = 4;
	int v[n]; for (int i = 0; i < n; i++) v[i] = i + 1;
	r += (sizeof v == 12 && v[2] == 3);                                              /* 1 */
	int a[n][m]; for (int i = 0; i < n; i++) for (int j = 0; j < m; j++) a[i][j] = i * m + j;
	r += (sizeof a == 48 && sizeof a[1] == 16 && a[2][3] == 11 && sum2(n, m, a) == 66) << 1;   /* 2 */
	typedef int row[m]; m = 100; r += (sizeof(row) == 16) << 2;                      /* 4: sized at the typedef */
	r += (bump(5, v) == 6) << 3;                                                     /* 8 */
	char *first = 0, *p = 0; int same = 1;
	for (int k = 0; k < 1000; k++) { char buf[n * 100]; buf[0] = (char)k; p = buf; if (!first) first = p; same &= p == first; }
	r += same << 4;                                                                  /* 16: storage reused */
	char c7 = 7; r += (knr(1, &c7, 3LL << 32) == 11) << 5;                           /* 32 */
	r += (sizeof(int[n]) == 12) << 6;                                                /* 64 */
	int (*rp)[m]; m = 4; int (*rq)[m] = a; rp = 0; (void)rp; r += (rq[2][1] == 9) << 7;   /* 128 */
	return r;
}
