/*
 * test_gpio_irq_chardev.c — GPIO interrupt via IO chardev, verified on UART.
 *
 * Full path: IO chardev → GPIO input → SYSCFG EXTICR → EXTI → NVIC → ISR → UART
 *
 * The test runner sends "gpio:1:0:1" via the IO chardev and checks that
 * "EXTI0_OK" appears on the UART chardev output.
 */
#define TEST_CUSTOM_VECTORS
#include "test.h"

/* SYSCFG */
#define SYSCFG_EXTICR1 (*(volatile unsigned int *)0x40013808)

/* GPIOB */
#define GPIOB_IDR (*(volatile unsigned int *)0x40020410)

/* EXTI */
#define EXTI_IMR   (*(volatile unsigned int *)0x40013C00)
#define EXTI_RTSR  (*(volatile unsigned int *)0x40013C08)
#define EXTI_PR    (*(volatile unsigned int *)0x40013C14)

/* USART2 */
#define USART2_DR  (*(volatile unsigned int *)0x40004404)

/* NVIC */
#define NVIC_ISER0 (*(volatile unsigned int *)0xE000E100)

static volatile int exti0_count;

void systick_handler(void) {}
void pendsv_handler(void) {}
void svc_handler(void) {}
void memmanage_handler(void) { while(1); }

static void uart_puts(const char *s)
{
    while (*s) USART2_DR = *s++;
}

void exti0_handler(void)
{
    exti0_count++;
    EXTI_PR = (1 << 0);
    uart_puts("EXTI0_OK\n");
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
    /* Route GPIOB to EXTI line 0 via SYSCFG */
    SYSCFG_EXTICR1 = 0x0001;

    /* Enable EXTI line 0: rising edge + interrupt */
    EXTI_IMR = (1 << 0);
    EXTI_RTSR = (1 << 0);
    NVIC_ISER0 = (1 << 6);

    exti0_count = 0;

    /* Spin waiting for IO chardev to send gpio:1:0:1 */
    for (volatile int i = 0; i < 100000000 && exti0_count == 0; i++) {}

    if (exti0_count >= 1 && (GPIOB_IDR & 1)) {
        semi_puts("PASS:gpio_irq_chardev\n");
        semi_exit(0);
    } else {
        semi_puts("FAIL:gpio_irq_chardev\n");
        semi_exit(1);
    }
}
