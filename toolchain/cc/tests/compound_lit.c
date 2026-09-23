// expect: 42
// Compound literals (type){init} (kernel spinlock/ratelimit init), whole-struct copy (memcpy, was a
// 4-byte scalar store), and struct/array lvalues decaying to their address in value context.
struct L { int a; int b[2]; };

int main(void)
{
	struct L x;
	struct L *p = &x;
	*p = (struct L){ .a = 10, .b = { 6, 0 } };   /* compound literal assigned to a struct lvalue */
	struct L y;
	y = x;                                        /* whole-struct copy (struct has an array member) */
	return y.a + y.b[0] * 2 + 20;                 /* 10 + 12 + 20 = 42 */
}
