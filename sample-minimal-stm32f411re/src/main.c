/*
 * main.c — RTOS demo: three tasks print and sleep, with heap tracing
 */

#include "config.h"
#include "devicetree.h"
#include "device.h"
#include "drivers/uart.h"
#include "sched.h"
#include "heap.h"
#include "trace.h"

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

/* Traced heap wrappers */
static void *talloc(const char *name, unsigned size)
{
    void *p = heap_alloc(size);
    if (p) trace_alloc(name, p, size);
    return p;
}

static void tfree(void *p)
{
    if (p) trace_free(p);
    heap_free(p);
}

static void task_a(void)
{
    void *sensor = talloc("sensor_ctx", 64);
    void *buf    = talloc("tx_buffer", 128);
    while (1) {
        uart_print("task_a: working\n");
        sched_sleep_ms(200);
    }
    (void)sensor; (void)buf;
}

static void task_b(void)
{
    void *cfg = talloc("config", 32);
    sched_sleep_ms(100);
    void *tmp = talloc("temp_data", 48);
    sched_sleep_ms(300);
    uart_print("task_b: freeing temp_data\n");
    tfree(tmp);
    while (1) {
        uart_print("task_b: working\n");
        sched_sleep_ms(200);
    }
    (void)cfg;
}

static void task_c(void)
{
    while (1) {
        void *pkt = talloc("packet", 80);
        uart_print("task_c: processing packet\n");
        sched_sleep_ms(150);
        tfree(pkt);
        sched_sleep_ms(150);
    }
}

static void idle_task(void)
{
    while (1) {}
}

extern char _heap_start;
extern char _heap_size;

void main(void)
{
    uart = DEVICE_DT_GET(usart2);

    heap_init(&_heap_start, (size_t)&_heap_size);

    uart_print("RTOS demo — heap tracing\n\n");

    sched_create_task(task_a,    "task_a", 1);
    sched_create_task(task_b,    "task_b", 1);
    sched_create_task(task_c,    "task_c", 1);
    sched_create_task(idle_task, "idle",   255);

    extern void systick_init(uint32_t cpu_hz, uint32_t tick_hz);
    systick_init(DT_SYSCLK_HZ, 1000);

    sched_start();
}
