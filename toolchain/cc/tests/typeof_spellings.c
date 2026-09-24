// expect: 0
/* All three GNU typeof spellings as a type specifier, incl. in a parameter (the SYSCALL_DEFINE / __se_sys_*
 * expansion uses `__typeof(...)` for each argument). */
static int gi = 5;
typeof(gi) a = 10;
__typeof__(gi) b = 20;
__typeof(gi) c = 30;
static int use(__typeof(gi) v) { return v + 1; }
int main(void) {
	if (a != 10) return 1;
	if (b != 20) return 2;
	if (c != 30) return 3;
	if (use(41) != 42) return 4;
	return 0;
}
