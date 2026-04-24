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

static void task_a(void)
{
    while (1) {
        uart_print("running task_a\n");
        sched_sleep_ms(1000);
    }
}

static void task_b(void)
{
    while (1) {
        uart_print("running task_b\n");
        sched_sleep_ms(1000);
    }
}

static void task_c(void)
{
    while (1) {
        uart_print("running task_c\n");
        sched_sleep_ms(1000);
    }
}

static void idle_task(void)
{
    while (1) {
        sched_yield();
    }
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
