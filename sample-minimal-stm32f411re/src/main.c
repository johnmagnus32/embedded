/*
 * main.c — RTOS demo: three tasks print and sleep
 */

#include "config.h"
#include "devicetree.h"
#include "device.h"
#include "drivers/uart.h"
#include "sched.h"

DEVICE_DT_DECLARE(usart2);

static const struct device *uart;

static void uart_print(const char *s)
{
    while (*s) {
        if (*s == '\n')
            uart_poll_out(uart, '\r');
        uart_poll_out(uart, *s++);
    }
}

static void __attribute__((noinline)) deep_work(const char *task)
{
    volatile char buf[100];
    buf[0] = task[0];  /* touch the array so compiler doesn't optimize it out */
    buf[99] = task[0];
    uart_print("  deep: ");
    uart_print(task);
    uart_print("\n");
}

static void __attribute__((noinline)) do_work(const char *task)
{
    uart_print(task);
    uart_print(": do_work\n");
    deep_work(task);
}

static void task_a(void)
{
    while (1) {
        do_work("task_a");
        sched_sleep_ms(100);
    }
}

static void task_b(void)
{
    while (1) {
        do_work("task_b");
        sched_sleep_ms(100);
    }
}

static void task_c(void)
{
    while (1) {
        do_work("task_c");
        sched_sleep_ms(100);
    }
}

static void idle_task(void)
{
    while (1) {}
}

void main(void)
{
    uart = DEVICE_DT_GET(usart2);

    uart_print("RTOS demo — 3 tasks printing\n\n");

    sched_create_task(task_a,    "task_a", 1);
    sched_create_task(task_b,    "task_b", 1);
    sched_create_task(task_c,    "task_c", 1);
    sched_create_task(idle_task, "idle",   255);

    extern void systick_init(uint32_t cpu_hz, uint32_t tick_hz);
    systick_init(DT_SYSCLK_HZ, 1000);

    sched_start();
}
