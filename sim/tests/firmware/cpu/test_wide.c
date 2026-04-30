/*
 * test_wide.c — Wide branches, wide load/store addressing modes, LDRD/STRD
 */
#include "test.h"

static unsigned int wbuf[8];
static unsigned char bbuf[16];
static unsigned short hbuf[8];

/* Force a wide branch by putting target far away */
__attribute__((noinline, section(".text.far")))
static int far_func(void) { return 0xBEEF; }

static void test_b_wide(void)
{
    volatile int result = 0;

    TEST("b_w");
    /* B.W — compiler generates wide branch for distant targets */
    result = far_func();
    CHECK(result == 0xBEEF);
}

static void test_bcond_wide(void)
{
    volatile int result = 0;

    TEST("bcond_w_taken");
    __asm volatile(
        "movs r0, #1\n"
        "cmp r0, #1\n"
        "beq.w 1f\n"
        "movs %0, #0\n"
        "b 2f\n"
        "1: movs %0, #1\n"
        "2:\n"
        : "=r"(result) :: "r0", "cc"
    );
    CHECK(result == 1);

    TEST("bcond_w_not_taken");
    result = 99;
    __asm volatile(
        "movs r0, #1\n"
        "cmp r0, #2\n"
        "beq.w 1f\n"
        "b 2f\n"
        "1: movs %0, #0\n"
        "2:\n"
        : "+r"(result) :: "r0", "cc"
    );
    CHECK(result == 99);
}

static void test_ldr_pre_index(void)
{
    unsigned int out;

    TEST("ldr_pre_index");
    wbuf[0] = 0xAA; wbuf[1] = 0xBB;
    unsigned int *p = wbuf;
    __asm volatile(
        "ldr.w %0, [%1, #4]!\n"  /* pre-index: addr = p+4, then p = p+4 */
        : "=r"(out), "+r"(p) :: "memory"
    );
    CHECK(out == 0xBB);
    CHECK(p == &wbuf[1]);
}

static void test_ldr_post_index(void)
{
    unsigned int out;

    TEST("ldr_post_index");
    wbuf[0] = 0xCC; wbuf[1] = 0xDD;
    unsigned int *p = wbuf;
    __asm volatile(
        "ldr.w %0, [%1], #4\n"  /* post-index: load from p, then p = p+4 */
        : "=r"(out), "+r"(p) :: "memory"
    );
    CHECK(out == 0xCC);
    CHECK(p == &wbuf[1]);
}

static void test_str_pre_index(void)
{
    TEST("str_pre_index");
    wbuf[0] = 0; wbuf[1] = 0;
    unsigned int *p = wbuf;
    __asm volatile(
        "mov r0, #0xEE\n"
        "str.w r0, [%0, #4]!\n"
        : "+r"(p) :: "r0", "memory"
    );
    CHECK(wbuf[1] == 0xEE);
    CHECK(p == &wbuf[1]);
}

static void test_ldr_reg_offset(void)
{
    unsigned int out;

    TEST("ldr_w_reg_offset");
    wbuf[3] = 0x12345678;
    __asm volatile(
        "mov r0, %1\n"
        "mov r1, #3\n"
        "ldr.w %0, [r0, r1, lsl #2]\n"  /* wbuf + 3*4 */
        : "=r"(out) : "r"(wbuf) : "r0", "r1"
    );
    CHECK(out == 0x12345678);
}

static void test_strb_ldrb_wide(void)
{
    unsigned int out;

    TEST("strb_w_imm12");
    bbuf[10] = 0;
    __asm volatile(
        "mov r0, #0xAB\n"
        "strb.w r0, [%0, #10]\n"
        : : "r"(bbuf) : "r0", "memory"
    );
    CHECK(bbuf[10] == 0xAB);

    TEST("ldrb_w_imm12");
    bbuf[5] = 0xCD;
    __asm volatile(
        "ldrb.w %0, [%1, #5]\n"
        : "=r"(out) : "r"(bbuf)
    );
    CHECK(out == 0xCD);

    TEST("ldrb_w_reg_offset");
    bbuf[7] = 0xEF;
    __asm volatile(
        "mov r1, #7\n"
        "ldrb.w %0, [%1, r1]\n"
        : "=r"(out) : "r"(bbuf) : "r1"
    );
    CHECK(out == 0xEF);
}

static void test_strh_ldrh_wide(void)
{
    unsigned int out;

    TEST("ldrh_w_imm12");
    hbuf[2] = 0xBEEF;
    __asm volatile(
        "ldrh.w %0, [%1, #4]\n"  /* hbuf + 2*2 */
        : "=r"(out) : "r"(hbuf)
    );
    CHECK(out == 0xBEEF);

    TEST("ldrsh_w_imm12");
    hbuf[3] = 0x8000;
    int sout;
    __asm volatile(
        "ldrsh.w %0, [%1, #6]\n"
        : "=r"(sout) : "r"(hbuf)
    );
    CHECK(sout == -32768);

    TEST("strh_w_reg_offset");
    hbuf[4] = 0;
    __asm volatile(
        "mov r0, #0x1234\n"
        "mov r1, #4\n"
        "strh.w r0, [%0, r1, lsl #1]\n"  /* hbuf + 4*2 */
        : : "r"(hbuf) : "r0", "r1", "memory"
    );
    CHECK(hbuf[4] == 0x1234);
}

static void test_ldrd_strd(void)
{
    TEST("strd");
    wbuf[0] = 0; wbuf[1] = 0;
    __asm volatile(
        "mov r0, #0xAA\n"
        "mov r1, #0xBB\n"
        "strd r0, r1, [%0]\n"
        : : "r"(wbuf) : "r0", "r1", "memory"
    );
    CHECK(wbuf[0] == 0xAA);
    CHECK(wbuf[1] == 0xBB);

    TEST("ldrd");
    wbuf[2] = 0x11111111;
    wbuf[3] = 0x22222222;
    unsigned int lo, hi;
    __asm volatile(
        "ldrd %0, %1, [%2, #8]\n"
        : "=r"(lo), "=r"(hi) : "r"(wbuf)
    );
    CHECK(lo == 0x11111111);
    CHECK(hi == 0x22222222);
}

void test_main(void)
{
    test_b_wide();
    test_bcond_wide();
    test_ldr_pre_index();
    test_ldr_post_index();
    test_str_pre_index();
    test_ldr_reg_offset();
    test_strb_ldrb_wide();
    test_strh_ldrh_wide();
    test_ldrd_strd();
    TEST_DONE("wide");
}
