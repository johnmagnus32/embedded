// expect: 7
// Attributes on a function's earlier declaration apply to its definition (GCC merges them): a prototype's
// section() places the body there (checked in the Makefile: the .s), a prototype's weak makes it weak.
void in_init(void) __attribute__((section(".text.proto_init")));
void in_init(void) {}
int maybe(void) __attribute__((weak));
int maybe(void) { return 2; }
int main(void) {
	int r = 0;
	in_init(); r += 1;
	r += maybe();   /* 2 */
	r += 4;
	return r;
}
