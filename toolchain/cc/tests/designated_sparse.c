// expect: 42
// Out-of-order / gapped designated array initializers via an index-keyed model — kernel asn1_op_lengths[]
// lists [ASN1_OP_RETURN]=0x28 before [ASN1_OP_END_SEQ]=0x20, so a sequential model exits early.
enum { A = 0, B = 5, C = 2 };
static const unsigned char t[6] = { [A] = 10, [B] = 30, [C] = 2 };
int main(void)
{
	return t[0] + t[5] + t[2];   /* 10 + 30 + 2 = 42 */
}
