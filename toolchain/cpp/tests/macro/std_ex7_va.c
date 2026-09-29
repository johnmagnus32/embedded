/* C11 6.10.3.5 EXAMPLE 7 + GNU: ## chains with empty operands, __VA_ARGS__, `# ## #`, `, ## __VA_ARGS__` */
#define t2(x,y,z) x ## y ## z
int j[] = { t2(1,2,3), t2(,4,5), t2(6,,7), t2(8,9,),
 t2(10,,), t2(,11,), t2(,,12), t2(,,) };
#define OBJ_LIKE (1-1)
#define showlist(...) puts(#__VA_ARGS__)
#define report(test, ...) ((test)?puts(#test): printf(__VA_ARGS__))
showlist(The first, second, and third items.);
report(x>y, "x is %d but y is %d", x, y);
#define hash_hash # ## #
#define mkstr(a) # a
#define in_between(a) mkstr(a)
#define join(c, d) in_between(c hash_hash d)
char p[] = join(x, y);
#define LOG(fmt, ...) pr(fmt, ##__VA_ARGS__)
LOG("a"); LOG("b", 1, 2); LOG("c",);
#define CAT3(a,b,c) a##b##c
CAT3(x,,) CAT3(,y,) CAT3(a b, c d, e f)
