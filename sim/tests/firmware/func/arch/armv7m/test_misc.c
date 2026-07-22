/*
 * test_misc.c — Miscellaneous instruction coverage
 *
 * REV16, NOP/WFI, ADD high, BLX register, LDRSH register offset,
 * ORN.W, STMIA.W, MOVW/MOVT, CONTROL register, DSB/DMB/ISB
 */
#include "test.h"

static void test_rev16(void)
{
    unsigned int out;
    TEST("rev16");
    __asm volatile("ldr r0, =0x12345678\n rev16 %0, r0" : "=r"(out) :: "r0");
    CHECK(out == 0x34127856);  /* swap bytes within each halfword */
}

static void test_nop_wfi(void)
{
    TEST("nop");
    /* NOP should execute without crashing */
    __asm volatile("nop\n nop\n nop");
    CHECK(1);  /* if we get here, it worked */

    TEST("wfi");
    /* WFI in thread mode with no pending interrupts should just continue */
    __asm volatile("wfi");
    CHECK(1);
}

static void test_add_high(void)
{
    unsigned int out;

    TEST("add_high_regs");
    __asm volatile(
        "mov r8, #100\n"
        "mov r9, #200\n"
        "add r8, r9\n"
        "mov %0, r8\n"
        : "=r"(out) :: "r8", "r9"
    );
    CHECK(out == 300);

    TEST("cmp_high_regs");
    unsigned int flags;
    __asm volatile(
        "mov r8, #42\n"
        "mov r9, #42\n"
        "cmp r8, r9\n"
        "mrs %0, apsr\n"
        : "=r"(flags) :: "r8", "r9", "cc"
    );
    CHECK(flags & (1u << 30));  /* Z flag set */

    TEST("mov_high_regs");
    __asm volatile(
        "mov r8, #0xAB\n"
        "mov %0, r8\n"
        : "=r"(out) :: "r8"
    );
    CHECK(out == 0xAB);
}

__attribute__((noinline))
static int blx_target(int x) { return x * 3; }

static void test_blx_register(void)
{
    TEST("blx_reg");
    int out;
    /* BLX via function pointer — compiler generates BLX Rm */
    int (*fp)(int) = blx_target;
    out = fp(7);
    CHECK(out == 21);
}

static void test_ldrsh_reg_offset(void)
{
    static short hbuf[3];
    hbuf[0] = 0x1234;
    hbuf[1] = (short)0x8000;
    hbuf[2] = 0x7FFF;
    int out;

    TEST("ldrsh_reg_16bit");
    __asm volatile(
        "mov r1, #2\n"
        "ldrsh %0, [%1, r1]\n"
        : "=r"(out) : "r"(hbuf) : "r1"
    );
    CHECK(out == (int)(short)0x8000);  /* -32768 sign-extended */
}

static void test_orn(void)
{
    unsigned int out;

    TEST("orn_w");
    /* ORN Rd, Rn, Rm: Rd = Rn | ~Rm */
    __asm volatile(
        "mov r0, #0\n"
        "mov r1, #0xFF\n"
        "orn %0, r0, r1\n"
        : "=r"(out) :: "r0", "r1"
    );
    CHECK(out == 0xFFFFFF00);  /* 0 | ~0xFF */
}

static unsigned int stm_buf[8];

static void test_stmia_wide(void)
{
    TEST("stmia_w");
    stm_buf[0] = stm_buf[1] = stm_buf[2] = 0;
    __asm volatile(
        "mov r0, %0\n"
        "mov r1, #0x11\n"
        "mov r2, #0x22\n"
        "mov r3, #0x33\n"
        "stmia.w r0!, {r1, r2, r3}\n"
        : : "r"(stm_buf) : "r0", "r1", "r2", "r3", "memory"
    );
    CHECK(stm_buf[0] == 0x11);
    CHECK(stm_buf[1] == 0x22);
    CHECK(stm_buf[2] == 0x33);
}

static void test_movw_movt(void)
{
    unsigned int out;

    TEST("movw");
    __asm volatile("movw %0, #0x1234" : "=r"(out));
    CHECK(out == 0x1234);

    TEST("movt");
    __asm volatile(
        "movw %0, #0x5678\n"
        "movt %0, #0xABCD\n"
        : "=r"(out)
    );
    CHECK(out == 0xABCD5678);
}

static void test_control_reg(void)
{
    unsigned int out;

    TEST("msr_mrs_control");
    /* Read default CONTROL (should be 0 after reset — MSP, privileged) */
    __asm volatile("mrs %0, control" : "=r"(out));
    CHECK((out & 3) == 0);

    /* Set SPSEL=1 (PSP), read back, restore */
    static unsigned int psp_stack[64];
    unsigned int psp_top = (unsigned int)&psp_stack[64];
    __asm volatile(
        "msr psp, %1\n"
        "mov r0, #2\n"       /* SPSEL=1 */
        "msr control, r0\n"
        "isb\n"
        "mrs %0, control\n"
        /* Restore MSP */
        "mov r0, #0\n"
        "msr control, r0\n"
        "isb\n"
        : "=r"(out) : "r"(psp_top) : "r0"
    );
    CHECK((out & 2) == 2);  /* SPSEL bit set */
}

static void test_barriers(void)
{
    TEST("dsb");
    __asm volatile("dsb sy");
    CHECK(1);

    TEST("dmb");
    __asm volatile("dmb sy");
    CHECK(1);

    TEST("isb");
    __asm volatile("isb sy");
    CHECK(1);
}

void test_main(void)
{
    test_rev16();
    test_nop_wfi();
    test_add_high();
    test_blx_register();
    test_ldrsh_reg_offset();
    test_orn();
    test_stmia_wide();
    test_movw_movt();
    test_control_reg();
    test_barriers();
    TEST_DONE("misc");
}
