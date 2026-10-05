// expect: 3
// A qualifier on an array typedef qualifies its elements (C11 6.7.3p9): `const tbl_t x` is a read-only object, placed
// in .rodata like `const int x[]` (checked in the Makefile: the .s) — the kernel's `const match_table_t pm_modes`.
typedef struct { int k; const char *s; } ent;
typedef ent tbl_t[];
static const tbl_t table = { { 1, "a" }, { 2, "b" } };
int main(void) { return table[0].k + table[1].k; }
