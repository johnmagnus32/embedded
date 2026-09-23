// expect: 42
// A block-scope _Static_assert (as expanded inside the kernel's container_of statement-expression)
// must be skipped, not parsed as an expression.
int main(void)
{
	int x = ({ _Static_assert(sizeof(int) == 4, "int must be 4"); 42; });
	return x;
}
