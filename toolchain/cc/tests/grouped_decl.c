// expect: 42
// A parenthesized (grouped) declarator without '*': function-type typedef `T (name)(params)`, as in
// arch/arm/include/asm/probes.h (probes_insn_handler_t). The fn-pointer form (*name) still works too.
typedef void (handler_t)(int);
typedef int (*fp)(int);
static int hit;
static void h(int x) { hit = x; }
static int dbl(int x) { return x * 2; }

int main(void)
{
	handler_t *hp = h; hp(20);
	fp g = dbl;
	return hit + g(11);   /* 20 + 22 = 42 */
}
