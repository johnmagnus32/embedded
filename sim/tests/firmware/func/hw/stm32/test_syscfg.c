/*
 * test_syscfg.c — SYSCFG EXTICR register and EXTI routing test.
 *
 * Tests:
 *   - SYSCFG_EXTICR register read/write
 *   - EXTI interrupt still fires after EXTICR port selection
 *   - Multiple EXTICR registers (pins 0-3 in EXTICR1, pin 4 in EXTICR2)
 */
#define TEST_CUSTOM_VECTORS
#include "test.h"

/* SYSCFG */
#define SYSCFG_EXTICR1 (*(volatile unsigned int *)0x40013808)
#define SYSCFG_EXTICR2 (*(volatile unsigned int *)0x4001380C)
#define SYSCFG_EXTICR3 (*(volatile unsigned int *)0x40013810)
#define SYSCFG_EXTICR4 (*(volatile unsigned int *)0x40013814)

/* EXTI */
#define EXTI_IMR   (*(volatile unsigned int *)0x40013C00)
#define EXTI_RTSR  (*(volatile unsigned int *)0x40013C08)
#define EXTI_SWIER (*(volatile unsigned int *)0x40013C10)
#define EXTI_PR    (*(volatile unsigned int *)0x40013C14)

/* NVIC */
#define NVIC_ISER0 (*(volatile unsigned int *)0xE000E100)

static volatile int exti0_count;
static volatile int exti4_count;

void systick_handler(void) {}
void pendsv_handler(void) {}
void svc_handler(void) {}
void memmanage_handler(void) { while(1); }

void exti0_handler(void)
{
    exti0_count++;
    EXTI_PR = (1 << 0);
}

void exti4_handler(void)
{
    exti4_count++;
    EXTI_PR = (1 << 4);
}

static void test_exticr_readback(void)
{
    TEST("exticr_readback");

    /* Default after reset: all zeros (GPIOA selected) */
    CHECK(SYSCFG_EXTICR1 == 0);
    CHECK(SYSCFG_EXTICR2 == 0);

    /* Write port 1 (GPIOB) for pin 0 */
    SYSCFG_EXTICR1 = 0x0001;
    CHECK(SYSCFG_EXTICR1 == 0x0001);

    /* Write port 1 for pins 0-3 */
    SYSCFG_EXTICR1 = 0x1111;
    CHECK(SYSCFG_EXTICR1 == 0x1111);

    /* Write port 1 for pin 4 (in EXTICR2) */
    SYSCFG_EXTICR2 = 0x0001;
    CHECK(SYSCFG_EXTICR2 == 0x0001);

    /* EXTICR1 unchanged */
    CHECK(SYSCFG_EXTICR1 == 0x1111);

    /* Reset */
    SYSCFG_EXTICR1 = 0;
    SYSCFG_EXTICR2 = 0;
}

static void test_exti_after_exticr(void)
{
    TEST("exti_after_exticr");

    /* Select GPIOB (port 1) for EXTI line 0 */
    SYSCFG_EXTICR1 = 0x0001;

    /* Configure EXTI line 0: rising edge, interrupt enabled */
    EXTI_IMR = (1 << 0);
    EXTI_RTSR = (1 << 0);
    NVIC_ISER0 = (1 << 6);  /* IRQ 6 = EXTI0 */

    /* Software trigger — should still work regardless of EXTICR */
    exti0_count = 0;
    EXTI_SWIER = (1 << 0);
    for (volatile int i = 0; i < 1000 && exti0_count == 0; i++) {}
    CHECK(exti0_count == 1);

    /* Clean up */
    EXTI_IMR = 0;
    SYSCFG_EXTICR1 = 0;
}

static void test_exticr2_pin4(void)
{
    TEST("exticr2_pin4");

    /* Select GPIOB (port 1) for EXTI line 4 */
    SYSCFG_EXTICR2 = 0x0001;
    CHECK(SYSCFG_EXTICR2 == 0x0001);

    /* Configure EXTI line 4 */
    EXTI_IMR = (1 << 4);
    EXTI_RTSR = (1 << 4);
    NVIC_ISER0 = (1 << 10);  /* IRQ 10 = EXTI4 */

    exti4_count = 0;
    EXTI_SWIER = (1 << 4);
    for (volatile int i = 0; i < 1000 && exti4_count == 0; i++) {}
    CHECK(exti4_count == 1);

    /* Clean up */
    EXTI_IMR = 0;
    SYSCFG_EXTICR2 = 0;
}

extern unsigned int _stack_top;
void test_main(void);

void __attribute__((naked)) reset_handler(void)
{
    __asm volatile("bl test_main\n b .\n");
}

__attribute__((section(".vector_table"), used))
void (* const vectors[])(void) = {
    (void (*)(void))&_stack_top,
    reset_handler, 0, 0,
    memmanage_handler, 0, 0, 0, 0, 0, 0,
    svc_handler, 0, 0,
    pendsv_handler,
    systick_handler,
    /* IRQ 0-10 */
    0, 0, 0, 0, 0, 0,
    exti0_handler,  /* IRQ 6 = EXTI0 */
    0, 0, 0,
    exti4_handler,  /* IRQ 10 = EXTI4 */
};

void test_main(void)
{
    test_exticr_readback();
    test_exti_after_exticr();
    test_exticr2_pin4();
    TEST_DONE("syscfg");
}
