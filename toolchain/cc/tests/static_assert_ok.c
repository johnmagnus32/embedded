// expect: 42
/* The condition is evaluated, so a TRUE one must stay silent — including the forms real headers use:
 * sizeof arithmetic, enum constants, and a message-less C23 form. */
enum { E_A = 6, E_B = 7 };
struct S { int a, b; };
_Static_assert(sizeof(struct S) == 2 * sizeof(int), "struct S is two ints");
_Static_assert(E_A * E_B == 42, "enum arithmetic folds");
_Static_assert(sizeof(char) == 1, "char is a byte");
int main(void) {
	_Static_assert(E_B - E_A == 1, "block scope works too");
	return E_A * E_B;
}
