// expect: 42
/* -mno-unaligned-access: a packed member sits at an alignment narrower than its width, so under the flag
 * every access to it must be split into bytes. ARMv7 permits unaligned LDR/STR on Normal memory, which is
 * why gcc's default (-munaligned-access) and ours emit them — but with the MMU off, or on Device memory,
 * ARMv7 permits none at all, so code that runs there needs the flag to mean something.
 *
 * The values must come out the same either way, which is what this checks; ../Makefile additionally runs it
 * compiled WITH the flag and asserts the access really is byte-wise. */
struct __attribute__((packed)) P {
	char  c;
	int   i;        /* at offset 1 */
	short s;        /* at offset 5 */
	long long q;    /* at offset 7 */
};
static struct P p = { 1, 0, 0, 0 };
static struct P q2;

int main(void) {
	int r = 0;
	if (sizeof(struct P) != 15) return 1;      /* packed: 1 + 4 + 2 + 8, no padding */

	p.i = -123456789;
	if (p.i == -123456789) r += 6;             /* a word at offset 1 */

	p.s = -1234;
	if (p.s == -1234) r += 6;                  /* a halfword at offset 5, sign-extended on read */

	p.q = -1234567890123456789LL;
	if (p.q == -1234567890123456789LL) r += 6; /* a doubleword at offset 7 */

	if (p.c == 1) r += 6;                      /* the neighbours all survived */

	p.c = 0x7F;
	if (p.c == 0x7F && p.i == -123456789 && p.s == -1234) r += 6;

	q2 = p;                                    /* whole-struct copy of a packed aggregate */
	if (q2.i == -123456789 && q2.s == -1234 && q2.q == -1234567890123456789LL && q2.c == 0x7F) r += 6;

	unsigned u = (unsigned)p.i;                /* and an unsigned read of the same bytes */
	if (u == (unsigned)-123456789) r += 6;

	return r;                                  /* 7 * 6 = 42 */
}
