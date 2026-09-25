// expect: 0
/* A name declared with a FUNCTION typedef is a function, not a variable (kernel fs_parser:
 * `typedef int fs_param_type(...); fs_param_type fs_param_is_bool, ...;`). Was: an int OBJECT of the same
 * name -> duplicate symbol with the real function. As a parameter it becomes a function pointer. */
typedef int binop(int, int);
binop add2, mul2;                          /* two prototypes */
int add2(int a, int b) { return a + b; }
int mul2(int a, int b) { return a * b; }
static int apply(binop f, int a, int b) { return f(a, b); }
int main(void) {
	if (add2(2, 3) != 5 || mul2(2, 3) != 6) return 1;
	if (apply(add2, 4, 5) != 9 || apply(mul2, 4, 5) != 20) return 2;
	binop *fp = mul2; if (fp(3, 3) != 9) return 3;
	return 0;
}
