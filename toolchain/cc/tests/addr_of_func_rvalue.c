// expect: 0
/* &function as a RUNTIME rvalue: a function designator is already an address, so &f == f. */
static void f(void) {}
int main(void) {
	void (*p)(void) = &f;
	void (*q)(void) = f;
	if (p != q) return 1;
	if ((void *)p != (void *)&f) return 2;
	return 0;
}
