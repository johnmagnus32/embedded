// expect: 0
/* Member access on a struct-returning call: `f(x).field` (kernel swp_offset(pte_to_swp_entry(pte))). */
struct pair { int a, b; };
static struct pair mk(int x) { struct pair p; p.a = x; p.b = x * 2; return p; }
int main(void) {
	if (mk(5).a != 5) return 1;
	if (mk(5).b != 10) return 2;
	int s = mk(7).a + mk(7).b;   /* 7 + 14 */
	if (s != 21) return 3;
	return 0;
}
