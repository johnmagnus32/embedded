// expect: 42
// A compound literal (and an unsized array) is placed above the slots its own initializer's code used: that code runs
// while it's being filled — bio_for_each_segment's `(struct bio_vec){ .bv_page = p, .bv_len = min(...), ... }`.
struct bv { int *page; unsigned len, off; };
int g7 = 7, g30 = 30, g5 = 5, gp;
int main(void) {
	struct bv b; int n = ((b = (struct bv){ .page = &gp, .len = ({ unsigned x = g30, y = g7; x < y ? x : y; }), .off = ({ unsigned big[16] = { 0 }; big[3] = g5; big[3]; }) }), 1);   /* (a big one: over all of it) */
	int a[] = { ({ int u = g30; u; }), ({ int w = g7; w; }), g5 };
	return n == 1 && b.page == &gp && b.len == 7 && b.off == 5 && a[0] == 30 && a[1] == 7 && a[2] == 5 && sizeof a == 12 ? 42 : 1;
}
