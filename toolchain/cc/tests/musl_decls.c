// expect: 0
// Declarator and initializer forms musl uses (each once failed): a function defined through a grouped name with
// named parameters, `int (*(sel)(int))(char)` returning a function pointer, C99 array-parameter qualifiers
// ([static N], [const], [restrict static n], [*]), a braced string initializer, compound literals that are operands
// (not whole elements) of an initializer, and offsetof by pointer difference through an array member.
int (g)(int x) { return x + 1; }
static int k(char c) { return c * 2; }
int (*(sel)(int x))(char c) { return x ? k : 0; }
static int sum(const int a[static 3], int n, int b[restrict static n]) { return a[0] + a[2] + b[n - 1]; }
void proto(int n, int a[*]);
static int first(int a[const]) { return a[0]; }
static const unsigned char protos[] = { "\001ab\0" "\002c\0" };
struct S { int a; int b; };
struct T { int pad; int arr[4]; };
static unsigned off = (unsigned)((char *)&(((struct T *)0)->arr[2]) - (char *)0);
int main(void)
{
	int a[3] = { 1, 2, 3 }, b[2] = { 10, 20 };
	struct S s = { ((struct S){ 4, 5 }).b + 1, (struct S){ 7, 8 }.a };
	if (g(41) != 42 || sel(1)('a') != 194 || sel(0) != 0) return 1;
	if (sum(a, 2, b) != 24 || first(b) != 10) return 2;
	if (sizeof protos != 8 || protos[0] != 1 || protos[1] != 'a' || protos[4] != 2 || protos[7] != 0) return 3;
	if (s.a != 6 || s.b != 7) return 4;
	if (off != 12) return 5;
	return 0;
}
