// expect: 0
/* GAS section model (kernel EXPORT_SYMBOL: `.section ".export_symbol","a" ; ... ; .previous`): quoted names
 * lose their quotes, `.previous` returns to the prior section (code after it must land back in .text and run),
 * and `.ascii "" "\0"` emits every operand. */
asm(".text\n.globl sm_fn\n"
    ".section \".sm_tbl\",\"a\"\n.globl sm_tbl\nsm_tbl: .asciz \"GPL\"\n.ascii \"\" \"\\0\"\n.balign 4\n.long sm_fn\n.previous\n"
    "sm_fn: mov r0, #42\n bx lr\n");
int sm_fn(void);
extern const unsigned char sm_tbl[];
int main(void) {
	if (sm_fn() != 42) return 1;                         /* .previous went back to .text: sm_fn is code */
	if (sm_tbl[0] != 'G' || sm_tbl[3] != 0 || sm_tbl[4] != 0) return 2;   /* "GPL\0" then the "\0" operand */
	if (*(const unsigned *)(sm_tbl + 8) != (unsigned)sm_fn) return 3;
	return 0;
}
