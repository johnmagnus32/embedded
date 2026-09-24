// expect: 0
typedef unsigned long size_t;
int accumulate(int *arr, size_t n) {
	int s = 0;
	for (size_t i = 0; i < n; i++) s += arr[i];
	return s;
}
/* EXPORT_SYMBOL-style: a redundant redeclaration + an address-taken global. This is what put the
 * function name into `globals` and made the call below compile to an INDIRECT call through garbage. */
extern typeof(accumulate) accumulate;
void *__addr_accumulate = (void *)&accumulate;
int caller(int *a, size_t n) { return accumulate(a, n); }   /* must be a DIRECT bl */
int main(void) {
	int a[4]; a[0] = 1; a[1] = 2; a[2] = 3; a[3] = 4;
	if (caller(a, 4) != 10) return 1;
	if (__addr_accumulate != (void *)&accumulate) return 2;
	return 0;
}
