// expect: 0
/* GNU `__label__` is block-scoped: two statement expressions in one function that each declare `__label__ out;`
 * get distinct labels (kernel wait_event's `__out:`, hand-expanded here since the suite has no cpp).
 * Was: the same asm label emitted twice. */
int main(void) {
	int a = ({ __label__ out; int r_ = 1; if (r_ > 5) goto out; r_ += 100; out: r_; });
	int b = ({ __label__ out; int r_ = 9; if (r_ > 5) goto out; r_ += 100; out: r_; });
	if (a != 101 || b != 9) return 1;
	{ __label__ out; goto out; return 2; out:; }
	return 0;
}
