// expect: 42
// A cleanup variable whose initializer declares its own locals (a ({...}), an array initializer's): the attribute
// survives them — the cleanup still runs at scope exit.
int ran;
static void done(int *p) { ran += *p; }
int g2 = 2;
int main(void) {
	{ int x __attribute__((cleanup(done))) = ({ int t = 20; t + g2; }); (void)x; }       /* ran 22 */
	{ int y __attribute__((cleanup(done))) = ({ int u = g2; u * 10; }); (void)y; }       /* ran 42 */
	return ran;
}
