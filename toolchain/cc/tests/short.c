// expect: 42
// 2-byte integer types + signedness: load width (ldrh/ldrsh), narrowing casts (uxth/sxth),
// struct member layout (a short aligns to 2), and correctly-sized/-signed global initializers.
struct S { int a; char b; short c; };          // a@0, b@4, c@6  -> sizeof 8

short          gs = -1;                          // signed short global   -> .hword 0xffff
unsigned short gu = 0xffff;                      // unsigned short global -> .hword 0xffff

int main(void) {
	int r = 0;
	if (sizeof(short) == 2)      r += 1;         // a 2-byte scalar exists                 -> 1
	if (sizeof(struct S) == 8)   r += 1;         // short member is 2-aligned, not 4       -> 2

	short s = -3;                                // signed: round-trips through strh/ldrsh
	if (s == -3)                 r += 4;         //                                         -> 6
	int wide = s;                                // load sign-extends
	if (wide == -3)              r += 8;         //                                         -> 14

	unsigned short u = 0xffff;                   // unsigned: ldrh zero-extends (not -1)
	if (u == 65535)              r += 16;        //                                         -> 30

	short t = (short)0x12345;                    // narrowing cast keeps the low 16 bits
	if (t == 0x2345)             r += 2;         //                                         -> 32

	if (gs == -1)                r += 4;         // signed global loads as -1               -> 36
	if (gu == 65535)             r += 4;         // unsigned global loads as 65535          -> 40

	struct S x; x.c = -7;                        // signed short struct member
	if (x.c == -7)               r += 2;         //                                         -> 42
	return r;
}
