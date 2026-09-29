/* GNU variadics and token spacing: comma elision (omitted vs empty), named `args...`, results that must not
 * merge with their neighbours (-neg is not --1), pasted-token spacing, stringizing expanded arguments */
#define LOG(fmt, ...) pr(fmt, ##__VA_ARGS__)
#define V(...) f(0, ##__VA_ARGS__)
#define W(a, b...) g(a, ##b)
LOG("c",) LOG("c") V() V(1) W(1) W(1,) W(1,2)
#define neg -1
#define plus +
-neg plus+ x plus plus y
#define cat(a,b) a##b
cat(x,y)z cat(1,e)+2
#define E
a E b -E- +E+
#define str(x) #x
#define xstr(x) str(x)
#define CAT3(a,b,c) a##b##c
#define F(fmt, args...) g(fmt, args)
#define G(...) h(__VA_ARGS__)
F(x) F(x, 1) G() G(1,2)
xstr(CAT3(a b, c d, e f)) CAT3(x,,) CAT3(,y,) CAT3(a b, c d, e f)
#define __inline
#define M(func, op) static __inline void __CPU_##func##_S(int x) { return x op 1; }
M(AND, &) M(OR, |)
