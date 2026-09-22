// expect: 42
// Unsigned operations: logical shift (lsr), unsigned divide/modulo (udiv), unsigned comparisons
// (hi/hs/lo/ls), and signedness propagating through arithmetic — while signed ops stay signed.
unsigned int gu = 0x80000000;                    // unsigned global

int main(void) {
	int r = 0;
	unsigned int a = 0x80000000;
	if ((a >> 28) == 8)          r += 1;         // lsr, not asr (asr -> 0xfffffff8)          -> 1
	unsigned int b = 0xffffffff;
	if (b > 1)                   r += 2;         // unsigned hi (signed would see -1 > 1 false)-> 3
	if (b / 2 == 0x7fffffff)     r += 4;         // udiv (sdiv of -1/2 = 0)                     -> 7
	if (b % 10 == 5)             r += 8;         // udiv modulo: 4294967295 % 10 == 5           -> 15
	unsigned int c = 3;
	if ((c - 5) > 0)             r += 16;        // 3-5 wraps huge; sum stays unsigned -> hi    -> 31

	int s = -8;                                  // signed paths unchanged:
	if ((s >> 1) == -4)          r += 2;         // asr keeps the sign                          -> 33
	if (-3 / 2 == -1)            r += 4;         // sdiv truncates toward zero                  -> 37
	if (-5 < 1)                  r += 4;         // signed lt                                   -> 41

	if (((gu + 0) >> 28) == 8)   r += 1;         // ADD of unsigned+int folds to unsigned -> lsr-> 42
	return r;
}
