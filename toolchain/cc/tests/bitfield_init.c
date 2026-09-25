// expect: 127
/* Bitfields + initializers: static init packs several fields into one unit; unnamed fields take no
 * initializer; promotion is by WIDTH (unsigned :7 -> int, so `u - 8 < 0`); 64-bit-unit fields read/write
 * across the word boundary; an assignment's value is the field as stored (truncated); an unsized compound
 * literal / local array is sized from its braces (not a 0-byte slot on top of the frame record). */
struct s { int a:12, b:20; };
struct s gx = { -123, -456 };
struct u { int a:4; int :4; int b:4, c:4; } gu = { 2, 3, 4 };
struct w { unsigned long long lo:30, mid:20, hi:14; };
struct p { unsigned int u:7; int i:5; };
int sum(int *a, int n) { int s = 0; for (int i = 0; i < n; i++) s += a[i]; return s; }
int main(void) {
	int r = 0;
	r += gx.a == -123 && gx.b == -456;                                          /* 1 */
	r += (gu.a == 2 && gu.b == 3 && gu.c == 4) << 1;                            /* 2 */
	struct w w = { 0, 0, 0 }; w.mid = 0xabcde; w.hi = 0x3fff; w.lo = 5;         /* mid straddles bits 30..49 */
	r += (w.mid == 0xabcde && w.hi == 0x3fff && w.lo == 5) << 2;                /* 4 */
	struct p p; p.u = 3; p.i = 0;
	r += (p.u - 8 < 0) << 3;                                                    /* 8: promoted to signed int */
	r += ((p.i = 31) == -1) << 4;                                               /* 16: value as stored in 5 bits */
	r += (sum((int[]){ 1, 2, 3, 4 }, 4) == 10) << 5;                            /* 32 */
	int arr[] = { [3] = 7, 1 };                                                 /* designator sizes it to 5 */
	r += (sizeof arr == 20 && arr[3] == 7 && arr[4] == 1) << 6;                 /* 64 */
	return r;
}
