// expect: 42
// A function-pointer member with a trailing __attribute__ after its param list (arch/arm proc-fns.h:
// void (*reset)(unsigned long addr, bool hvc) __attribute__((noreturn));).
struct ops { void (*reset)(unsigned long a, int b) __attribute__((noreturn)); int x; };
int main(void)
{
	struct ops o;
	o.x = 42;
	return o.x;
}
