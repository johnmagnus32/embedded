// expect: 42
// What only the whole unit shows: a static scalar nothing writes (or takes the address of) reads as its initializer
// (0 without one); a written one, or one whose address escapes, doesn't; a constant stub defined after its callers
// folds at them (the branch it guards goes, and with it the only call of `unused_after`, never emitted).
static int ro = 7, rw = 1, esc = 3, bss;
static int *p = &esc;
static unsigned long long div_by = 1000000000ULL / 125;   /* read-only: its divisions fold (no 64-bit divide) */
static int stub(int x);
static int unused_after(int x);
static void bump(void) { rw++; }
int main(void) {
	bump(); *p = 9;
	int r = ro == 7 && rw == 2 && esc == 9 && bss == 0 && div_by / 1000000L == 8;
	if (stub(r)) { if (unused_after(r)) return 1; }
	return r ? 42 : 2;
}
static int stub(int x) { (void)x; return 0; }
static int unused_after(int x) { return x + 1; }
