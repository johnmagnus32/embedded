/*
 * STM32H745 — Cortex-M7 core firmware
 *
 * Sends commands to M4 via IPC driver, receives responses.
 * No hardcoded shared memory addresses — everything through the driver.
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

static void boot_m4_core(void)
{
    volatile uint32_t *rcc_gcr = (volatile uint32_t *)(0x58024400 + 0xA0);
    *rcc_gcr |= (1 << 3);
}

#ifdef CONFIG_SCHED

static void app_task(void)
{
    const struct device *console = DEVICE_DT_GET(DT_CHOSEN_CONSOLE);
    const struct device *mbox = DEVICE_DT_GET(mailbox);
    int count = 0;

    while (1) {
        /* Send command to M4 via IPC driver */
        char msg[32] = "read_sensor";
        ipc_send(mbox, msg, 11);

        uart_puts(console, "[M7] sent: read_sensor\n");

        sched_sleep_ms(100);

        /* Check for response */
        char reply[64] = {0};
        int len = ipc_recv(mbox, reply, sizeof(reply) - 1);
        if (len > 0) {
            reply[len] = '\0';
            uart_puts(console, "[M7] M4 says: ");
            uart_puts(console, reply);
            uart_puts(console, "\n");
        }

        count++;
        sched_sleep_ms(900);
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

    uart_puts(console, "\n=== STM32H745 M7 Core ===\n");
    uart_puts(console, "AMP: IPC via device tree mailbox driver\n\n");

    uart_puts(console, "[M7] Booting M4 core...\n");
    boot_m4_core();

#ifdef CONFIG_HEAP
    heap_init(_heap_start, (size_t)&_heap_size);
#endif

#ifdef CONFIG_SCHED
    sched_create_task(app_task, "app", 3);
    sched_create_task(idle_task, "idle", 7);
#ifdef CONFIG_SYSTICK
    systick_init(DT_SYSCLK_HZ, 1000);
#endif
    sched_start();
#else
    while (1) {}
#endif
}
