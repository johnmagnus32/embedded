// expect: 42
// GNU C extensions: statement expressions ({ ...; expr; }) and typeof(expr|type). These underpin the
// kernel's min/max/container_of macros. (Written inline: the cc test harness runs cc1 directly, no cpp.)
int main(void) {
	int r = 0;
	int a = ({ int t = 20; t + 1; });                              // stmt-expr value = last expr
	if (a == 21)                    r += 1;                        // -> 1
	int x = 5; typeof(x) y = x * 2;                                // typeof(expr) -> int
	if (y == 10)                    r += 2;                        // -> 3
	int p = 3, q = 7;                                              // min/max via typeof + ?: in a stmt-expr
	int lo = ({ typeof(p) _x = p; typeof(q) _y = q; _x < _y ? _x : _y; });
	if (lo == 3)                    r += 4;                        // -> 7
	int hi = ({ typeof(p) _x = p; typeof(q) _y = q; _x > _y ? _x : _y; });
	if (hi == 7)                    r += 8;                        // -> 15
	long long big = 0x100000000LL;
	typeof(big) z = big + 1;                                       // typeof preserves 64-bit
	if (z == 0x100000001LL)         r += 16;                       // -> 31
	long long lo2 = ({ typeof(big) _x = big; long long _y = 0x50; _x < _y ? _x : _y; });
	if (lo2 == 0x50)                r += 8;                        // -> 39
	int w = ({ int s = 0; for (int i = 1; i <= 3; i = i + 1) s += i; s; });   // statement before value
	if (w == 6)                     r += 2;                        // -> 41
	if (sizeof(typeof(big)) == 8)   r += 1;                        // typeof inside sizeof -> 42
	return r;
}
