// expect: 0
/* as data-directive expressions (kernel BUG/ex_table/alternatives asm): `.word 1b, 2b` = local labels in
 * ANOTHER section (section-symbol relocs), `((0xe7f001f2) & 0xFFFFFFFF)` = constant expression, `1b - .` =
 * PC-relative cross-section word (R_ARM_REL32), and a same-section `. - x` difference. */
asm(".text\n"
    "1: nop\n"
    "2: nop\n"
    ".data\n"
    ".globl dtbl\n"
    "dtbl: .word 1b, 2b\n"
    "  .long ((0xe7f001f2) & 0xFFFFFFFF)\n"
    "  .long 1b - .\n"
    "  .word (3 << 4) | 1, 10 - 3, ~0 & 0xff\n"
    "  .word dtbl + 8\n");
extern unsigned dtbl[8];
int main(void) {
	if (dtbl[1] - dtbl[0] != 4) return 1;                                  /* 2b is 4 bytes after 1b */
	if (dtbl[2] != 0xe7f001f2u) return 2;
	if ((unsigned)&dtbl[3] + dtbl[3] != dtbl[0]) return 3;                   /* PC-relative back to 1b */
	if (dtbl[4] != 49 || dtbl[5] != 7 || dtbl[6] != 255) return 4;
	if (dtbl[7] != (unsigned)&dtbl[2]) return 5;
	return 0;
}
