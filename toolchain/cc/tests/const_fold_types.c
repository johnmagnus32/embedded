// expect: 0
/* Constant folding is TYPE-directed (static inits, case labels, array sizes, folded ifs). Was: every constant
 * evaluated as signed 64-bit, so e.g. `-1 < 1U` folded true and `0xFFFFFFFFu + 1` folded to 2^32. */
static unsigned g1 = 0xFFFFFFFFu + 1;                 /* 0 */
static int g2 = (unsigned char)0x1ff;                 /* 255 */
static int g3 = (-1 < 1U);                            /* 0: unsigned compare */
static int g4 = (int)0x80000000 < 0;                  /* 1 */
static unsigned g5 = 0xF0000000u >> 28;               /* 15: logical shift */
static int g6 = -16 >> 2;                             /* -4: arithmetic shift */
static unsigned g7 = 7u / 2 - 4u / 3;                 /* 2 */
static char arr[(unsigned char)0x104];                /* size 4 */
int main(void) {
	if (g1 != 0 || g2 != 255 || g3 != 0 || g4 != 1 || g5 != 15 || g6 != -4 || g7 != 2) return 1;
	if (sizeof(arr) != 4) return 2;
	switch (0x100000003ULL & 0xff) { case 3: break; default: return 3; }
	if (-1 < 1U) return 4;                              /* folded if: must take the unsigned result */
	return 0;
}
