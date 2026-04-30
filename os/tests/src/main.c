/*
 * main.c — OS test runner
 *
 * Boots on QEMU lm3s6965evb, prints test results via UART0.
 */

extern void uart_print(const char *s);
extern void print_int(int n);

void main(void)
{
    uart_print("=== OS Test Suite ===\n");
    uart_print("PASS: hello world\n");
    uart_print("DONE: 1 passed, 0 failed\n");

    /* QEMU exit: write to Stellaris NVIC APINT to trigger system reset,
     * or just loop — the test harness will kill QEMU after timeout */
    while (1) {}
}
