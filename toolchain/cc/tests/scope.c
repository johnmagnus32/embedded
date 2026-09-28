// expect: 42
/* C scopes (C11 6.2.1): a block-scope typedef / struct tag / enum constant shadows the outer one and ends with
 * its block; a local object hides a typedef name. */
typedef int T;
struct s { int a; };
typedef char U;
enum { B = 1 };
int f(void) { int T = 3; return T + 1; }                              /* 4: a local hides the typedef */
int g(void) { struct s { char c[8]; } x; return sizeof x; }          /* 8: inner tag shadows */
int h(void) { struct s y; return sizeof y; }                           /* 4: outer tag again */
int k(void) { typedef long long U; return sizeof(U); }                 /* 8 */
int m(void) { return sizeof(U); }                                      /* 1 */
int p(void) { enum { B = 2 }; return B; }                              /* 2 */
int q(void) { return B; }                                              /* 1 */
int r(void) { { typedef int V; V v = 5; (void)v; } int V = 6; return V; }   /* 6: V's typedef ended */
int main(void) { return f()==4 && g()==8 && h()==4 && k()==8 && m()==1 && p()==2 && q()==1 && r()==6 ? 42 : 1; }
