/* asmgoto.c — asm goto: jumps to C labels (%l<N> and %l[name]), with outputs (valid on every path) and inputs. */
int printf(const char *, ...);
static int pick(int x) {
	asm goto ("cmp %0, #0\n\tbeq %l[zero]\n\tcmp %0, #1\n\tbeq %l2" : : "r" (x) : "cc" : zero, one);
	return 2;
zero: return 0;
one: return 1;
}
static int out(int x) {
	int y;
	asm goto ("add %0, %1, #10\n\tcmp %1, #5\n\tbgt %l2" : "=&r" (y) : "r" (x) : "cc" : big);   /* early clobber: y and x in different registers */
	return y;
big: return -y;
}
int main(void) { printf("%d %d %d | %d %d\n", pick(0), pick(1), pick(7), out(3), out(9)); return 0; }
