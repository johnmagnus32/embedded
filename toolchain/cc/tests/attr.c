// expect: 42
// __attribute__((packed)) / ((aligned(N))) struct layout + struct bitfields (read, signed extract, RMW write).
struct P { char a; int b; } __attribute__((packed));       // no padding: sizeof 5, b at offset 1
struct A { char a; int b; } __attribute__((aligned(16)));  // alignment raised: sizeof 16
struct BF { unsigned a : 3; unsigned b : 5; int c : 4; };  // three bitfields packed into one int

int main(void) {
	int r = 0;
	if (sizeof(struct P) == 5)    r += 1;      // packed removes the 3 padding bytes       -> 1
	if (sizeof(struct A) == 16)   r += 2;      // aligned(16) rounds the size up            -> 3
	struct P p; p.a = 1; p.b = 0x11223344;
	if (p.b == 0x11223344)        r += 4;      // unaligned int member round-trips          -> 7
	struct BF bf;
	bf.a = 5; bf.b = 20; bf.c = -3;
	if (bf.a == 5)                r += 4;       // unsigned 3-bit                            -> 11
	if (bf.b == 20)               r += 4;       // unsigned 5-bit                            -> 15
	if (bf.c == -3)               r += 8;       // signed 4-bit sign-extends                 -> 23
	bf.a = 9;                                   // 9 & 7 == 1: write truncates to the field
	if (bf.a == 1)                r += 8;       // write masks to width                      -> 31
	if (bf.b == 20 && bf.c == -3) r += 2;       // RMW left the neighbouring fields alone    -> 33
	if (sizeof(struct BF) == 4)   r += 8;       // 3+5+4 bits share one int unit             -> 41
	unsigned all = bf.b;
	if (all == 20)                r += 1;       // read into a wider type                    -> 42
	return r;
}
