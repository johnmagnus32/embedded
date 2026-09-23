// expect: 42
// Designated array initializers [idx] = value (kernel lookup tables, e.g. asn1_op_lengths[]).
enum { A, B, C, D, NR };
static const unsigned char t[NR] = { [A] = 10, [B] = 2, [C] = 0, [D] = 30 };
int main(void)
{
	return t[A] + t[B] + t[D];   /* 10 + 2 + 30 = 42 */
}
