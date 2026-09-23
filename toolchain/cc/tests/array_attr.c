// expect: 42
// A declarator that is an array with a trailing __attribute__ AFTER the [N] suffix (kernel module.h:
// struct module_memory mem[MOD_MEM_NUM_TYPES] __attribute__((__aligned__((1 << 6))));).
struct S { int mem[4] __attribute__((__aligned__((1 << 6)))); int x; };
int main(void)
{
	struct S s;
	s.x = 42;
	return s.x;
}
