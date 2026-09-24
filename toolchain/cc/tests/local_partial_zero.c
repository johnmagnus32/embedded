// expect: 0
/* Partially-initialized LOCAL aggregates must zero-fill the rest (C semantics). Regression for the old
 * init_of "leave the rest as-is" bug — now fixed by the unified initializer path (parse_init + lower_local
 * zeroes the gaps). dirty() smears the stack first so a missing zero-fill would surface as garbage. */
struct S { int a, b, c; };
static void dirty(void) { volatile int j[16]; for (int i = 0; i < 16; i++) j[i] = 0x55555555; }
int main(void) {
	dirty();
	struct S s = { .a = 1 };        /* b, c must be 0 even though the stack slot was dirtied */
	if (s.a != 1) return 1;
	if (s.b != 0) return 2;
	if (s.c != 0) return 3;
	dirty();
	int arr[5] = { 9, 8 };          /* arr[2..4] must be 0 */
	if (arr[0] != 9 || arr[1] != 8) return 4;
	if (arr[2] || arr[3] || arr[4]) return 5;
	return 0;
}
