// expect: 0
/* Address of an array element as a global constant: `&arr[i]` -> symbol + i*elemsize (SYSCTL_ZERO pattern). */
static int tbl[4] = { 10, 20, 30, 40 };
static int *p1 = &tbl[2];
static void *p2 = (void *)&tbl[1];
int main(void) {
	if (p1 != &tbl[2]) return 1;
	if (*p1 != 30) return 2;
	if (p2 != (void *)&tbl[1]) return 3;
	if (*(int *)p2 != 20) return 4;
	return 0;
}
