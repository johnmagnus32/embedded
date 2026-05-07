/*
 * test_context.c — Context switch correctness tests
 *
 * Tests that R4-R11 are preserved across yields and that
 * task stacks don't corrupt each other.
 */
#include "sched.h"
#include "test.h"
#include <stdint.h>

/* --- Test: R4-R11 preserved across yield --- */

static volatile int reg_test_done;
static volatile int reg_test_ok;

static void reg_check_task(void)
{
    /* Set R4-R11 to known values and yield multiple times */
    register uint32_t r4  __asm__("r4")  = 0xDEAD0004;
    register uint32_t r5  __asm__("r5")  = 0xDEAD0005;
    register uint32_t r6  __asm__("r6")  = 0xDEAD0006;
    register uint32_t r7  __asm__("r7")  = 0xDEAD0007;
    register uint32_t r8  __asm__("r8")  = 0xDEAD0008;
    register uint32_t r9  __asm__("r9")  = 0xDEAD0009;
    register uint32_t r10 __asm__("r10") = 0xDEAD000A;
    register uint32_t r11 __asm__("r11") = 0xDEAD000B;

    for (int i = 0; i < 20; i++)
        sched_yield();

    /* Check all registers still have the right values */
    reg_test_ok = (r4  == 0xDEAD0004 && r5  == 0xDEAD0005 &&
                   r6  == 0xDEAD0006 && r7  == 0xDEAD0007 &&
                   r8  == 0xDEAD0008 && r9  == 0xDEAD0009 &&
                   r10 == 0xDEAD000A && r11 == 0xDEAD000B);
    reg_test_done = 1;
    while (1) sched_sleep_ms(60000);
}

/* A "noisy" task that clobbers R4-R11 with different values */
static void reg_clobber_task(void)
{
    while (1) {
        register uint32_t r4  __asm__("r4")  = 0xBAAD0004;
        register uint32_t r5  __asm__("r5")  = 0xBAAD0005;
        register uint32_t r6  __asm__("r6")  = 0xBAAD0006;
        register uint32_t r7  __asm__("r7")  = 0xBAAD0007;
        register uint32_t r8  __asm__("r8")  = 0xBAAD0008;
        register uint32_t r9  __asm__("r9")  = 0xBAAD0009;
        register uint32_t r10 __asm__("r10") = 0xBAAD000A;
        register uint32_t r11 __asm__("r11") = 0xBAAD000B;
        (void)r4; (void)r5; (void)r6; (void)r7;
        (void)r8; (void)r9; (void)r10; (void)r11;
        sched_yield();
    }
}

/* --- Test: stack isolation --- */

static volatile int stack_a_ok;
static volatile int stack_b_ok;

static void stack_fill_a(void)
{
    volatile uint32_t canary[32];
    for (int i = 0; i < 32; i++) canary[i] = 0xAAAAAAAA;
    sched_yield();
    sched_yield();
    sched_yield();
    stack_a_ok = 1;
    for (int i = 0; i < 32; i++) {
        if (canary[i] != 0xAAAAAAAA) { stack_a_ok = 0; break; }
    }
    while (1) sched_sleep_ms(60000);
}

static void stack_fill_b(void)
{
    volatile uint32_t canary[32];
    for (int i = 0; i < 32; i++) canary[i] = 0xBBBBBBBB;
    sched_yield();
    sched_yield();
    sched_yield();
    stack_b_ok = 1;
    for (int i = 0; i < 32; i++) {
        if (canary[i] != 0xBBBBBBBB) { stack_b_ok = 0; break; }
    }
    while (1) sched_sleep_ms(60000);
}

void test_context_run(void)
{
    TEST_SUITE("context");

    /* Test 1: R4-R11 preserved across context switches */
    reg_test_done = 0;
    reg_test_ok = 0;
    sched_create_task(reg_check_task, "rchk", 1);
    sched_create_task(reg_clobber_task, "rclb", 1);
    sched_sleep_ms(200);
    TEST_ASSERT(reg_test_done, "register check task should complete");
    TEST_ASSERT(reg_test_ok, "R4-R11 should be preserved across yields");

    /* Test 2: stack isolation between tasks */
    stack_a_ok = stack_b_ok = 0;
    sched_create_task(stack_fill_a, "stka", 1);
    sched_create_task(stack_fill_b, "stkb", 1);
    sched_sleep_ms(200);
    TEST_ASSERT(stack_a_ok, "task A stack canaries should be intact");
    TEST_ASSERT(stack_b_ok, "task B stack canaries should be intact");

    uart_print("  context tests done\n");
}
