/*
 * test_ldst_w.c — Wide load/store byte/halfword addressing modes
 *
 * STRB.W/LDRB.W pre/post index and register offset
 * LDRSH.W register offset and pre/post index
 * STRH.W pre/post index
 */
#include "test.h"

static unsigned char bbuf[16];
static unsigned short hbuf[8];

static void test_strb_pre_index(void)
{
    TEST("strb_w_pre");
    bbuf[3] = 0;
    unsigned char *p = &bbuf[2];
    __asm volatile(
        "mov r0, #0xAB\n"
        "strb.w r0, [%0, #1]!\n"
        : "+r"(p) :: "r0", "memory"
    );
    CHECK(bbuf[3] == 0xAB);
    CHECK(p == &bbuf[3]);
}

static void test_strb_post_index(void)
{
    TEST("strb_w_post");
    bbuf[0] = 0;
    unsigned char *p = bbuf;
    __asm volatile(
        "mov r0, #0xCD\n"
        "strb.w r0, [%0], #1\n"
        : "+r"(p) :: "r0", "memory"
    );
    CHECK(bbuf[0] == 0xCD);
    CHECK(p == &bbuf[1]);
}

static void test_strb_reg_offset(void)
{
    TEST("strb_w_reg");
    bbuf[5] = 0;
    __asm volatile(
        "mov r0, #0xEF\n"
        "mov r1, #5\n"
        "strb.w r0, [%0, r1]\n"
        : : "r"(bbuf) : "r0", "r1", "memory"
    );
    CHECK(bbuf[5] == 0xEF);
}

static void test_ldrb_pre_index(void)
{
    TEST("ldrb_w_pre");
    bbuf[4] = 0x42;
    unsigned char *p = &bbuf[3];
    unsigned int out;
    __asm volatile(
        "ldrb.w %0, [%1, #1]!\n"
        : "=r"(out), "+r"(p) :: "memory"
    );
    CHECK(out == 0x42);
    CHECK(p == &bbuf[4]);
}

static void test_ldrb_post_index(void)
{
    TEST("ldrb_w_post");
    bbuf[0] = 0x99;
    unsigned char *p = bbuf;
    unsigned int out;
    __asm volatile(
        "ldrb.w %0, [%1], #1\n"
        : "=r"(out), "+r"(p) :: "memory"
    );
    CHECK(out == 0x99);
    CHECK(p == &bbuf[1]);
}

static void test_ldrsh_w_reg(void)
{
    TEST("ldrsh_w_reg");
    hbuf[2] = 0x8001;  /* negative when sign-extended */
    int out;
    __asm volatile(
        "mov r1, #2\n"
        "ldrsh.w %0, [%1, r1, lsl #1]\n"  /* hbuf + 2*2 */
        : "=r"(out) : "r"(hbuf) : "r1"
    );
    CHECK(out == (int)(short)0x8001);
}

static void test_ldrsh_w_pre(void)
{
    TEST("ldrsh_w_pre");
    hbuf[3] = 0xFFFE;  /* -2 as signed short */
    unsigned short *p = &hbuf[2];
    int out;
    __asm volatile(
        "ldrsh.w %0, [%1, #2]!\n"
        : "=r"(out), "+r"(p) :: "memory"
    );
    CHECK(out == -2);
    CHECK(p == &hbuf[3]);
}

static void test_strh_pre_index(void)
{
    TEST("strh_w_pre");
    hbuf[5] = 0;
    unsigned short *p = &hbuf[4];
    __asm volatile(
        "mov r0, #0x1234\n"
        "strh.w r0, [%0, #2]!\n"
        : "+r"(p) :: "r0", "memory"
    );
    CHECK(hbuf[5] == 0x1234);
    CHECK(p == &hbuf[5]);
}

static void test_strh_post_index(void)
{
    TEST("strh_w_post");
    hbuf[0] = 0;
    unsigned short *p = hbuf;
    __asm volatile(
        "mov r0, #0x5678\n"
        "strh.w r0, [%0], #2\n"
        : "+r"(p) :: "r0", "memory"
    );
    CHECK(hbuf[0] == 0x5678);
    CHECK(p == &hbuf[1]);
}

void test_main(void)
{
    test_strb_pre_index();
    test_strb_post_index();
    test_strb_reg_offset();
    test_ldrb_pre_index();
    test_ldrb_post_index();
    test_ldrsh_w_reg();
    test_ldrsh_w_pre();
    test_strh_pre_index();
    test_strh_post_index();
    TEST_DONE("ldst_w");
}
