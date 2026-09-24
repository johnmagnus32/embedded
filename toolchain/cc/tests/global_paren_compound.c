// expect: 0
/* Parenthesized compound literal as a global initializer value: ((T){...}) (kernel kuid_t/kernel_cap_t). */
struct cap { unsigned long long val; };
static struct cap a = ((struct cap){ 5 });
static struct cap b = (struct cap){ 9 };
struct kid { int v; };
struct outer { struct kid u; int x; };
static struct outer o = { .u = ((struct kid){ 3 }), .x = 7 };
int main(void) {
	if (a.val != 5) return 1;
	if (b.val != 9) return 2;
	if (o.u.v != 3) return 3;
	if (o.x != 7) return 4;
	return 0;
}
