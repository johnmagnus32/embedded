// expect: 5
/* Basic asm (no `:`) takes `%` literally — kernel BUG() emits `.pushsection .rodata.str, "aMS", %progbits, 1`.
 * Extended asm still interprets %N / %% (checked alongside). */
static int v(void) { int r; __asm__("mov %0, #2" : "=r"(r)); return r; }
int main(void) {
	__asm__ volatile(".pushsection .rodata.str, \"aMS\", %progbits, 1\n.asciz \"x\"\n.popsection");
	int r; __asm__("mov %0, #3 @ 100%% sure" : "=r"(r));
	return r + v();
}
