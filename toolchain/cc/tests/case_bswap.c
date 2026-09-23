// expect: 42
// case label as a constant EXPRESSION (a cast) — `case (blk_status_t)1:` (blk_types.h); and
// __builtin_bswap16/32/64 folding in a constant context (crc32table.h generated table).
typedef int bs_t;
static const unsigned x[] = { __builtin_bswap32(0x000000ffL) };
int main(void)
{
	int r = 0;
	switch ((bs_t)1) { case (bs_t)1: r = 42; break; default: r = 1; }
	return (x[0] == 0xff000000u) ? r : 2;
}
