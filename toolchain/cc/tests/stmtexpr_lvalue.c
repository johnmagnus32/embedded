// expect: 42
// A statement-expression ({...}) used as an lvalue (address-taken or assigned). Kernel container_of and
// min/max-style macros yield an lvalue through a ({...}); gen_addr must take the last expr's address.
int g;
int main(void)
{
	int *p = &({ g; });   /* address of the stmt-expr's lvalue result */
	*p = 42;
	return g;
}
