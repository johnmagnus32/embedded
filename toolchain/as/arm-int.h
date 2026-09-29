/* arm-int.h — the ARM backend's internals shared by its two instruction sets: arm.c (A32) and thumb.c (T32). The
 * operand parsers live in arm.c and read the statement md_assemble is on. */
#ifndef ARM_INT_H
#define ARM_INT_H
#include "as.h"

typedef struct { int rn, P, U, W, isreg, rm; long imm; u32 shift; int grp, gsym; } ArmAddr;   /* see parse_addr */
int arm_ntok(void);
const char *arm_tok(int i);
int arm_reg(const char *t);                 /* r0-r15 / aliases / .req; -1 if not a register */
long arm_imm(const char *t);                /* #<constant expression> */
u32 arm_reglist(int start, int *caret);     /* {..} register-list operand from token start on */
ArmAddr arm_addr(int first);                /* [Rn, ...] address from token first on */
int arm_cond(const char *s, u32 *code);     /* a condition-code suffix */
int arm_unified(void);                      /* .syntax unified */
void thumb_assemble(void);                  /* thumb.c: one Thumb instruction (toks[0] lowercased) */
#endif
