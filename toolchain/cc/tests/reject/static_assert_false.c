// error: static assertion failed: "sizeof is not what this code assumes"
/* It used to be skipped unparsed, so this compiled clean — and with it every layout and ABI assertion the
 * kernel relies on (~1200 of them), which exist to catch exactly the struct-offset and initializer mistakes
 * a from-scratch compiler makes. The wording matches GCC's. */
_Static_assert(sizeof(int) == 9, "sizeof is not what this code assumes");
int main(void) { return 0; }
