// expect: 42
// Front-end features surfaced by compiling real kernel code: octal/hex char escapes, _Bool, GCC case
// ranges, C11 _Generic, anonymous unions/structs (member promotion) with union layout, designated inits.
struct pt { int x, y; };

int main(void) {
	int r = 0;
	if ('\001' == 1 && '\x7f' == 127)                r += 1;    // char escapes            -> 1
	_Bool b = 5; if (b)                              r += 2;    // _Bool (nonzero is true) -> 3
	int c = 5, hit = 0;
	switch (c) { case 0 ... 9: hit = 1; break; default: hit = 0; }
	if (hit)                                         r += 4;    // case range              -> 7
	int gi = _Generic(c, int: 8, long: 99, default: 0);
	if (gi == 8)                                     r += 8;    // _Generic picks `int`    -> 15
	struct { union { int i; struct { short lo, hi; }; }; } u;   // anonymous union + struct
	u.i = 0x00070000;
	if (u.hi == 7 && u.lo == 0)                      r += 16;   // promoted members overlap-> 31
	struct pt p = { .y = 9, .x = 2 };                           // designated initializer
	if (p.x == 2 && p.y == 9)                        r += 8;    //                         -> 39
	union { int w; char b0; } un; un.w = 0x41;
	if (un.b0 == 0x41)                               r += 2;    // union: low byte aliases -> 41
	if (sizeof(union { char a; int b; }) == 4)       r += 1;    // union size = widest     -> 42
	return r;
}
