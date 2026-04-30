/*
 * test_uart_chardev.c — Verify UART TX reaches the chardev TCP socket.
 *
 * Writes a known string to USART2 DR, then exits via semihosting.
 * The test runner connects to the chardev port and verifies the bytes arrive.
 */
#define TEST_CUSTOM_VECTORS
#include "test.h"

#define USART2_DR (*(volatile unsigned int *)0x40004404)

extern unsigned int _stack_top;
void test_main(void);

void __attribute__((naked)) reset_handler(void)
{
    __asm volatile("bl test_main\n b .\n");
}

__attribute__((section(".vector_table"), used))
void (* const vectors[])(void) = {
    (void (*)(void))&_stack_top,
    reset_handler,
};

void test_main(void)
{
    /* Write a known string directly to UART DR */
    const char *msg = "UART_OK\n";
    while (*msg) USART2_DR = *msg++;

    /* Spin briefly to let the emulator flush chardev buffers */
    for (volatile int i = 0; i < 100000; i++) {}

    semi_exit(0);
}
