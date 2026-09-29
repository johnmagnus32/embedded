/* stringizing macro-expanded arguments: spacing is what GCC produces */
#define str(x) #x
#define xstr(x) str(x)
#define x 2
#define f(a) f(x * (a))
#define g f
#define w 0,1
#define E
#define P(a) [a]
#define Q(a,b) a b
#define NEG -1
xstr(x+1) xstr(x + 1) xstr(g(x+(3,4)-w)) xstr(E x E) xstr( E+E ) xstr(P(x)P(x)) xstr(Q(x,x)) xstr(-NEG) xstr(a E b) xstr(1 E) xstr(( x )) xstr(f(1)f(2))
