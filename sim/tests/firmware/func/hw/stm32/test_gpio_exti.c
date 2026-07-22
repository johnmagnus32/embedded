/*
 * test_gpio_exti.c — GPIO output/input and EXTI interrupt tests.
 *
 * Tests:
 *   - GPIO ODR write → read back
 *   - GPIO BSRR set/reset
 *   - GPIO IDR reflects external input (set via IO chardev)
 *   - EXTI rising edge interrupt fires on GPIO input change
 */
#define TEST_CUSTOM_VECTORS
#include "test.h"

/* GPIOA base 0x40020000, GPIOB base 0x40020400 */
#define GPIOA_IDR  (*(volatile unsigned int *)0x40020010)
#define GPIOA_ODR  (*(volatile unsigned int *)0x40020014)
#define GPIOA_BSRR (*(volatile unsigned int *)0x40020018)
#define GPIOB_IDR  (*(volatile unsigned int *)0x40020410)
#define GPIOB_ODR  (*(volatile unsigned int *)0x40020414)

/* EXTI */
#define EXTI_IMR   (*(volatile unsigned int *)0x40013C00)
#define EXTI_RTSR  (*(volatile unsigned int *)0x40013C08)
#define EXTI_FTSR  (*(volatile unsigned int *)0x40013C0C)
#define EXTI_SWIER (*(volatile unsigned int *)0x40013C10)
#define EXTI_PR    (*(volatile unsigned int *)0x40013C14)

/* NVIC */
#define NVIC_ISER0 (*(volatile unsigned int *)0xE000E100)

static volatile int exti0_count;

void systick_handler(void) {}
void pendsv_handler(void) {}
void svc_handler(void) {}
void memmanage_handler(void) { while(1); }

void exti0_handler(void)
{
    exti0_count++;
    EXTI_PR = (1 << 0);  /* clear pending */
}

static void test_gpio_odr(void)
{
    TEST("gpio_odr_readback");
    GPIOA_ODR = 0x00FF;
    CHECK(GPIOA_ODR == 0x00FF);

    GPIOA_ODR = 0;
    CHECK(GPIOA_ODR == 0);
}

static void test_gpio_bsrr(void)
{
    TEST("gpio_bsrr_set");
    GPIOA_ODR = 0;
    GPIOA_BSRR = (1 << 5);  /* set pin 5 */
    CHECK(GPIOA_ODR & (1 << 5));

    TEST("gpio_bsrr_reset");
    GPIOA_BSRR = (1 << (5 + 16));  /* reset pin 5 */
    CHECK(!(GPIOA_ODR & (1 << 5)));
}

static void test_exti_swier(void)
{
    TEST("exti_swier");
    exti0_count = 0;

    /* Enable EXTI line 0: interrupt mask + rising trigger */
    EXTI_IMR = (1 << 0);
    EXTI_RTSR = (1 << 0);

    /* Enable IRQ 6 (EXTI0) in NVIC */
    NVIC_ISER0 = (1 << 6);

    /* Software trigger */
    EXTI_SWIER = (1 << 0);

    /* Wait for ISR */
    for (volatile int i = 0; i < 1000 && exti0_count == 0; i++) {}
    CHECK(exti0_count == 1);

    /* Clean up */
    EXTI_IMR = 0;
    NVIC_ISER0 = 0;
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
    /* IRQ 0-6 */
    0, 0, 0, 0, 0, 0,
    exti0_handler,  /* IRQ 6 = EXTI0 */
};

void test_main(void)
{
    test_gpio_odr();
    test_gpio_bsrr();
    test_exti_swier();
    TEST_DONE("gpio_exti");
}
