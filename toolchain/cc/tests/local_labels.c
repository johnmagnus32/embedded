// expect: 111
/* GAS numeric local labels: `Nf` binds to the NEXT definition (not the last), labels may be reused, and N may
 * be multi-digit (kernel `9998:` / `9998b`). Old bug: the first `b 1f` resolved to the LAST `1:` -> 110. */
asm(".text\n.globl lbl_test\nlbl_test:\n"
    "  mov r0, #0\n"
    "  b 1f\n  mov r0, #99\n"
    "1: add r0, r0, #1\n"
    "  b 1f\n  mov r0, #99\n"
    "1: add r0, r0, #10\n"
    "9998: add r0, r0, #100\n"
    "  b 9999f\n  mov r0, #99\n"
    "9999: bx lr\n"
    ".data\n.globl lbl_ref\nlbl_ref: .word 9998b, 9999b\n");
int lbl_test(void);
extern unsigned lbl_ref[2];
int main(void) { if (lbl_ref[1] - lbl_ref[0] != 12) return 1; return lbl_test(); }
