// expect: 42
// GNU address-of-label (&&label) + __label__ local-label declaration, as in the kernel's
// _THIS_IP_ macro:  ({ __label__ __here; __here: (unsigned long)&&__here; })  (irq/bh/lock paths).
static unsigned long ip(void)
{
	return ({ __label__ __here; __here: (unsigned long)&&__here; });
}

int main(void)
{
	return ip() != 0 ? 42 : 2;   /* a label's code address is always nonzero */
}
