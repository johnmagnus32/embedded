/*
 * STM32H745 — Cortex-M4 core firmware
 *
 * Receives commands from M7 via IPC driver, sends responses.
 */

#include "config.h"
#include "devicetree.h"
#include "device.h"
#include "drivers/uart.h"
#include "ipc.h"

#ifdef CONFIG_SCHED
#include "sched.h"
#endif
#ifdef CONFIG_HEAP
#include "heap.h"
#endif
#ifdef CONFIG_SYSTICK
extern void systick_init(uint32_t cpu_hz, uint32_t tick_hz);
#endif
#ifdef CONFIG_HEAP
extern uint8_t _heap_start[];
extern uint32_t _heap_size;
#endif

DEVICE_DT_DECLARE(DT_CHOSEN_CONSOLE);
DEVICE_DT_DECLARE(mailbox);

static int sensor_value = 25;

#ifdef CONFIG_SCHED

static void sensor_task(void)
{
    const struct device *console = DEVICE_DT_GET(DT_CHOSEN_CONSOLE);
    const struct device *mbox = DEVICE_DT_GET(mailbox);

    while (1) {
        char cmd[64] = {0};
        int len = ipc_recv(mbox, cmd, sizeof(cmd) - 1);

        if (len > 0) {
            cmd[len] = '\0';
            uart_puts(console, "[M4] received: ");
            uart_puts(console, cmd);
            uart_puts(console, "\n");

            sensor_value++;
            char reply[32] = "temp=";
            /* append number */
            char *p = reply + 5;
            int v = sensor_value;
            char tmp[8]; int n = 0;
            while (v > 0) { tmp[n++] = '0' + v % 10; v /= 10; }
            while (n > 0) *p++ = tmp[--n];
            *p++ = 'C';
            *p = '\0';

            ipc_send(mbox, reply, (uint32_t)(p - reply));

            uart_puts(console, "[M4] replied: ");
            uart_puts(console, reply);
            uart_puts(console, "\n");
        }

        sched_sleep_ms(50);
    }
}

static void idle_task(void)
{
    while (1) { __asm volatile("wfi"); }
}

#endif

int main(void)
{
    const struct device *console = DEVICE_DT_GET(DT_CHOSEN_CONSOLE);
    console->init(console);

    uart_puts(console, "[M4] Core booted\n");
    uart_puts(console, "[M4] Waiting for IPC commands...\n\n");

#ifdef CONFIG_HEAP
    heap_init(_heap_start, (size_t)&_heap_size);
#endif

#ifdef CONFIG_SCHED
    sched_create_task(sensor_task, "sensor");
    sched_create_task(idle_task, "idle");
#ifdef CONFIG_SYSTICK
    systick_init(DT_SYSCLK_HZ, 1000);
#endif
    sched_start();
#else
    while (1) {}
#endif
}
