/*
 * main.c — Minimal RTOS demo: two tasks printing to UART
 *
 * Uses the OS scheduler, SysTick, and UART driver.
 */

#include "config.h"
#include "devicetree.h"
#include "device.h"
#include "drivers/uart.h"
#include "sched.h"

/* Declare the UART device (defined by the driver) */
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

/* ---- Tasks ---- */

static void sensor_task(void)
{
    while (1) {
        uart_print("sensor: reading\n");
        for (volatile int i = 0; i < 100; i++) ;
    }
}

static void comms_task(void)
{
    while (1) {
        uart_print("comms: sending\n");
        for (volatile int i = 0; i < 100; i++) ;
    }
}

static void idle_task(void)
{
    while (1) {
        __asm volatile("wfi");
    }
}

/* ---- Boot ---- */

void main(void)
{
    uart = DEVICE_DT_GET(usart2);

    uart_print("Mini RTOS booting...\n");

    sched_create_task(sensor_task, "sensor", 1);
    sched_create_task(comms_task,  "comms",  1);
    sched_create_task(idle_task,   "idle",   0);

    uart_print("Starting scheduler\n");

    extern void systick_init(uint32_t cpu_hz, uint32_t tick_hz);
    systick_init(DT_SYSCLK_HZ, 1000);

    sched_start();
}
