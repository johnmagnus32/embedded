/* hide sets: a function-like invocation whose ')' comes from past the expansion that produced the macro name
 * re-enables the macros that expansion hid (Prosser; GCC) — U-Boot's CONFIG_IS_ENABLED nests __concat this way. */
#define __concat(a, b) ___concat(a, b)
#define ___concat(a, b) a ## b
#define F1(x) __concat(G, x) (9)
#define G1(x) x
int a = __concat(F, 1) (1);
/* C11 6.10.3.4 EXAMPLE: unspecified by the standard; GCC's answer */
#define f(a) a*g
#define g(a) f(a)
int b = f(2)(9);
/* the ')' inside the expansion: still hidden */
#define h(x) x h
#define k h(1)(2)
int c = k;
#define AA BB
#define BB(x) AA x
int d = AA(1)(2);
#define ID(x) x
#define CALL(m, x) m(x)
int e = CALL(ID, ID(CALL)(ID, 3));
