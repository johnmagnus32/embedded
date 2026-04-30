/*
 * main.c — OS test runner
 *
 * Boots on QEMU lm3s6965evb, starts the scheduler, runs test suites
 * as a high-priority task, prints results via UART0.
 */
#include "sched.h"
#include "test.h"
#include <stdint.h>

extern void uart_print(const char *s);
extern void print_int(int n);

int test_pass_count;
int test_fail_count;

extern void test_sched_run(void);
extern void test_sleep_run(void);
extern void test_sync_run(void);

static void idle_task(void)
{
    while (1) {}
}

static void test_runner(void)
{
    uart_print("=== OS Test Suite ===\n");

    test_sched_run();
    test_sleep_run();
    test_sync_run();

    uart_print("\nDONE: ");
    print_int(test_pass_count);
    uart_print(" passed, ");
    print_int(test_fail_count);
    uart_print(" failed\n");

    /* Halt — QEMU will be killed by timeout */
    while (1) {}
}

void main(void)
{
    /* Start systick for timing (16 MHz, 1000 Hz tick) */
    extern void systick_init(uint32_t cpu_hz, uint32_t tick_hz);
    systick_init(16000000, 1000);

    /* Test runner at priority 0 (highest) so it controls execution */
    sched_create_task(test_runner, "test", 0);
    sched_create_task(idle_task, "idle", 255);

    sched_start();
}
