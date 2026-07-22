/*
 * test_arith.c — UDIV, SDIV, MLA, MLS, SMULL, UMULL,
 *                Thumb modified immediate, shifted register data processing,
 *                wide register shifts (LSL.W, LSR.W, ASR.W, ROR.W)
 */
#include "test.h"

static void test_udiv(void)
{
    unsigned int out;

    TEST("udiv_exact");
    __asm volatile("mov r0, #100\n mov r1, #10\n udiv %0, r0, r1" : "=r"(out) :: "r0", "r1");
    CHECK(out == 10);

    TEST("udiv_truncate");
    __asm volatile("mov r0, #100\n mov r1, #7\n udiv %0, r0, r1" : "=r"(out) :: "r0", "r1");
    CHECK(out == 14);

    TEST("udiv_by_zero");
    __asm volatile("mov r0, #42\n mov r1, #0\n udiv %0, r0, r1" : "=r"(out) :: "r0", "r1");
    CHECK(out == 0);

    TEST("udiv_large");
    __asm volatile("ldr r0, =0xFFFFFFFF\n mov r1, #2\n udiv %0, r0, r1" : "=r"(out) :: "r0", "r1");
    CHECK(out == 0x7FFFFFFF);
}

static void test_sdiv(void)
{
    int out;

    TEST("sdiv_pos");
    __asm volatile("mov r0, #100\n mov r1, #10\n sdiv %0, r0, r1" : "=r"(out) :: "r0", "r1");
    CHECK(out == 10);

    TEST("sdiv_neg_num");
    __asm volatile("mvn r0, #99\n mov r1, #10\n sdiv %0, r0, r1" : "=r"(out) :: "r0", "r1");
    CHECK(out == -10);

    TEST("sdiv_neg_den");
    __asm volatile("mov r0, #100\n mvn r1, #9\n sdiv %0, r0, r1" : "=r"(out) :: "r0", "r1");
    CHECK(out == -10);

    TEST("sdiv_both_neg");
    __asm volatile("mvn r0, #99\n mvn r1, #9\n sdiv %0, r0, r1" : "=r"(out) :: "r0", "r1");
    CHECK(out == 10);

    TEST("sdiv_by_zero");
    __asm volatile("mov r0, #42\n mov r1, #0\n sdiv %0, r0, r1" : "=r"(out) :: "r0", "r1");
    CHECK(out == 0);
}

static void test_mla_mls(void)
{
    unsigned int out;

    TEST("mla");
    /* MLA rd, rn, rm, ra: rd = rn * rm + ra */
    __asm volatile(
        "mov r0, #5\n mov r1, #6\n mov r2, #10\n"
        "mla %0, r0, r1, r2\n"
        : "=r"(out) :: "r0", "r1", "r2"
    );
    CHECK(out == 40);  /* 5*6 + 10 */

    TEST("mls");
    /* MLS rd, rn, rm, ra: rd = ra - rn * rm */
    __asm volatile(
        "mov r0, #5\n mov r1, #6\n mov r2, #100\n"
        "mls %0, r0, r1, r2\n"
        : "=r"(out) :: "r0", "r1", "r2"
    );
    CHECK(out == 70);  /* 100 - 5*6 */
}

static void test_umull(void)
{
    unsigned int lo, hi;

    TEST("umull_small");
    __asm volatile(
        "mov r0, #100\n mov r1, #200\n"
        "umull %0, %1, r0, r1\n"
        : "=r"(lo), "=r"(hi) :: "r0", "r1"
    );
    CHECK(lo == 20000);
    CHECK(hi == 0);

    TEST("umull_overflow");
    __asm volatile(
        "ldr r0, =0xFFFFFFFF\n ldr r1, =0xFFFFFFFF\n"
        "umull %0, %1, r0, r1\n"
        : "=r"(lo), "=r"(hi) :: "r0", "r1"
    );
    CHECK(lo == 1);
    CHECK(hi == 0xFFFFFFFE);
}

static void test_smull(void)
{
    unsigned int lo, hi;

    TEST("smull_pos");
    __asm volatile(
        "mov r0, #100\n mov r1, #200\n"
        "smull %0, %1, r0, r1\n"
        : "=r"(lo), "=r"(hi) :: "r0", "r1"
    );
    CHECK(lo == 20000);
    CHECK(hi == 0);

    TEST("smull_neg");
    __asm volatile(
        "mvn r0, #99\n mov r1, #200\n"  /* r0 = -100 */
        "smull %0, %1, r0, r1\n"
        : "=r"(lo), "=r"(hi) :: "r0", "r1"
    );
    /* -100 * 200 = -20000 = 0xFFFFFFFF FFFFB1E0 */
    CHECK(lo == 0xFFFFB1E0);
    CHECK(hi == 0xFFFFFFFF);
}

static void test_modified_immediate(void)
{
    unsigned int out;

    TEST("mod_imm_plain");
    /* imm8 with no rotation */
    __asm volatile("mov.w %0, #0xFF" : "=r"(out));
    CHECK(out == 0xFF);

    TEST("mod_imm_pattern1");
    /* 0x00XY00XY pattern */
    __asm volatile("mov.w %0, #0x00AB00AB" : "=r"(out));
    CHECK(out == 0x00AB00AB);

    TEST("mod_imm_pattern2");
    /* 0xXY00XY00 pattern */
    __asm volatile("mov.w %0, #0xCD00CD00" : "=r"(out));
    CHECK(out == 0xCD00CD00);

    TEST("mod_imm_pattern3");
    /* 0xXYXYXYXY pattern */
    __asm volatile("mov.w %0, #0x3F3F3F3F" : "=r"(out));
    CHECK(out == 0x3F3F3F3F);

    TEST("and_w_imm");
    __asm volatile("ldr r0, =0xFFFF00FF\n and.w %0, r0, #0xFF00FF00" : "=r"(out) :: "r0");
    CHECK(out == 0xFF000000);

    TEST("orr_w_imm");
    __asm volatile("mov r0, #0\n orr.w %0, r0, #0x00FF0000" : "=r"(out) :: "r0");
    CHECK(out == 0x00FF0000);

    TEST("eor_w_imm");
    __asm volatile("ldr r0, =0xFFFFFFFF\n eor.w %0, r0, #0xFF" : "=r"(out) :: "r0");
    CHECK(out == 0xFFFFFF00);

    TEST("bic_w_imm");
    __asm volatile("ldr r0, =0xFFFFFFFF\n bic.w %0, r0, #0xFF" : "=r"(out) :: "r0");
    CHECK(out == 0xFFFFFF00);

    TEST("sub_w_imm");
    __asm volatile("mov r0, #100\n sub.w %0, r0, #30" : "=r"(out) :: "r0");
    CHECK(out == 70);

    TEST("add_w_imm");
    __asm volatile("mov r0, #100\n add.w %0, r0, #200" : "=r"(out) :: "r0");
    CHECK(out == 300);

    TEST("rsb_w_imm");
    __asm volatile("mov r0, #10\n rsb.w %0, r0, #100" : "=r"(out) :: "r0");
    CHECK(out == 90);
}

static void test_shifted_register(void)
{
    unsigned int out;

    TEST("add_w_lsl");
    __asm volatile("mov r0, #1\n mov r1, #1\n add.w %0, r0, r1, lsl #4" : "=r"(out) :: "r0", "r1");
    CHECK(out == 17);  /* 1 + (1 << 4) */

    TEST("sub_w_lsr");
    __asm volatile("mov r0, #100\n mov r1, #64\n sub.w %0, r0, r1, lsr #1" : "=r"(out) :: "r0", "r1");
    CHECK(out == 68);  /* 100 - 32 */

    TEST("and_w_shifted");
    __asm volatile("ldr r0, =0xFFFF0000\n mov r1, #0xFF\n and.w %0, r0, r1, lsl #16" : "=r"(out) :: "r0", "r1");
    CHECK(out == 0x00FF0000);  /* 0xFFFF0000 & 0x00FF0000 */

    TEST("orr_w_shifted");
    __asm volatile("mov r0, #0\n mov r1, #1\n orr.w %0, r0, r1, lsl #31" : "=r"(out) :: "r0", "r1");
    CHECK(out == 0x80000000);

    TEST("mov_w_lsl");
    /* MOV.W Rd, Rm, LSL #n — encoded as ORR.W Rd, R15, Rm, LSL #n */
    __asm volatile("mov r0, #3\n lsl.w %0, r0, #8" : "=r"(out) :: "r0");
    CHECK(out == 0x300);

    TEST("mvn_w_shifted");
    __asm volatile("mov r0, #0xFF\n mvn.w %0, r0, lsl #24" : "=r"(out) :: "r0");
    CHECK(out == 0x00FFFFFF);
}

static void test_wide_shifts(void)
{
    unsigned int out;

    TEST("lsl_w_reg");
    __asm volatile("mov r0, #1\n mov r1, #16\n lsl.w %0, r0, r1" : "=r"(out) :: "r0", "r1");
    CHECK(out == 0x10000);

    TEST("lsr_w_reg");
    __asm volatile("ldr r0, =0x80000000\n mov r1, #16\n lsr.w %0, r0, r1" : "=r"(out) :: "r0", "r1");
    CHECK(out == 0x8000);

    TEST("asr_w_reg");
    __asm volatile("ldr r0, =0x80000000\n mov r1, #16\n asr.w %0, r0, r1" : "=r"(out) :: "r0", "r1");
    CHECK(out == 0xFFFF8000);  /* sign-extended */

    TEST("ror_w_reg");
    __asm volatile("ldr r0, =0x00000001\n mov r1, #4\n ror.w %0, r0, r1" : "=r"(out) :: "r0", "r1");
    CHECK(out == 0x10000000);
}

void test_main(void)
{
    test_udiv();
    test_sdiv();
    test_mla_mls();
    test_umull();
    test_smull();
    test_modified_immediate();
    test_shifted_register();
    test_wide_shifts();
    TEST_DONE("arith");
}
