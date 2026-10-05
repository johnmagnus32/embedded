// expect: 63
// sizeof, _Alignof and offsetof are size_t (unsigned int on ARM), not int: a signed operand compared or combined
// with one converts to unsigned. (The kernel's min()/max() signedness check reads typeof(sizeof(x)).)
struct s { char c; int v; };

int main(void) {
	int r = 0, neg = -1;
	char buf[4];
	if (!(neg < sizeof(buf)))                        r += 1;    // -1 -> UINT_MAX: not less  -> 1
	if ((typeof(sizeof(buf)))-1 > 0)                 r += 2;    // the type is unsigned      -> 3
	__auto_type n = sizeof(buf) - 1;
	if ((typeof(n))-1 > 0 && n == 3)                 r += 4;    // __auto_type keeps it       -> 7
	if ((typeof(_Alignof(int)))-1 > 0)               r += 8;    // _Alignof too              -> 15
	if ((typeof(__builtin_offsetof(struct s, v)))-1 > 0) r += 16; // and offsetof            -> 31
	if (neg / sizeof(int) == 0x3fffffff)             r += 32;   // unsigned division         -> 63
	return r;
}
