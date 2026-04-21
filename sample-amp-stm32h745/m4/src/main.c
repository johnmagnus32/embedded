/*
 * STM32H745 — Cortex-M4 core firmware
 *
 * This is the "real-time" core. It:
 *   - Boots when M7 releases it from reset
 *   - Waits for commands from M7 via shared memory
 *   - Processes them (reads sensors, controls motors)
 *   - Sends results back to M7
 *
 * This is a COMPLETELY SEPARATE FIRMWARE — different .elf,
 * different linker script, different memory region.
 * It shares NOTHING with M7 except the shared RAM at 0x38000000.
 */

#include "config.h"
#include "devicetree.h"
#include "device.h"
#include "drivers/uart.h"
#include "ipc_shm.h"

#ifdef CONFIG_SCHED
#include "sched.h"
#endif

#ifdef CONFIG_SYSTICK
extern void systick_init(uint32_t cpu_hz, uint32_t tick_hz);
#endif

DEVICE_DT_DECLARE(DT_CHOSEN_CONSOLE);

/* Simulate reading a sensor */
static int fake_sensor_value = 25;

#ifdef CONFIG_SCHED

static void sensor_task(void)
{
    const struct device *console = DEVICE_DT_GET(DT_CHOSEN_CONSOLE);

    while (1) {
        /* Check for command from M7 */
        char cmd[64] = {0};
        int len = ipc_recv(&IPC->m7_to_m4, cmd, sizeof(cmd) - 1);

        if (len > 0) {
            cmd[len] = '\0';
            uart_puts(console, "[M4] received: ");
            uart_puts(console, cmd);
            uart_puts(console, "\n");

            /* "Read sensor" and send back result */
            fake_sensor_value += 1;  /* simulate changing value */

            char reply[32];
            char *p = reply;
            const char *prefix = "temp=";
            while (*prefix) *p++ = *prefix++;
            int v = fake_sensor_value;
            char tmp[8]; int n = 0;
            while (v > 0) { tmp[n++] = '0' + v % 10; v /= 10; }
            while (n > 0) *p++ = tmp[--n];
            *p++ = 'C';
            *p = '\0';

            ipc_send(&IPC->m4_to_m7, reply, (uint32_t)(p - reply));

            uart_puts(console, "[M4] replied: ");
            uart_puts(console, reply);
            uart_puts(console, "\n");
        }

        sched_sleep_ms(50);  /* poll at 20Hz */
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

    uart_puts(console, "[M4] Core booted (240MHz)\n");
    uart_puts(console, "[M4] Waiting for commands from M7...\n\n");

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
