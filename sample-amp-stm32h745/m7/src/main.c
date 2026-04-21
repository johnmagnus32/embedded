/*
 * STM32H745 — Cortex-M7 core firmware
 *
 * This is the "application" core. It:
 *   - Boots first (M7 is the primary core)
 *   - Releases M4 from reset
 *   - Sends commands to M4 via shared memory
 *   - Receives sensor data back from M4
 *
 * In a real system: M7 runs the UI/networking/logic,
 * M4 runs real-time motor control or sensor processing.
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

/* Release M4 core from reset (STM32H745-specific) */
static void boot_m4_core(void)
{
    /*
     * On STM32H745, M4 is held in reset by RCC.
     * Set RCC_GCR.BOOT_C2 to release it.
     * RCC base = 0x58024400, GCR offset = 0xA0
     */
    volatile uint32_t *rcc_gcr = (volatile uint32_t *)(0x58024400 + 0xA0);
    *rcc_gcr |= (1 << 3);  /* BOOT_C2 = 1 → release M4 */
}

/* Clear shared memory */
static void ipc_init(void)
{
    IPC->m7_to_m4.flag = 0;
    IPC->m4_to_m7.flag = 0;
}

#ifdef CONFIG_SCHED

static void app_task(void)
{
    const struct device *console = DEVICE_DT_GET(DT_CHOSEN_CONSOLE);
    int count = 0;

    while (1) {
        /* Send a command to M4 */
        char msg[32];
        char *p = msg;
        const char *prefix = "read_sensor:";
        while (*prefix) *p++ = *prefix++;
        /* append count */
        if (count == 0) *p++ = '0';
        else {
            char tmp[8]; int n = 0; int v = count;
            while (v > 0) { tmp[n++] = '0' + v % 10; v /= 10; }
            while (n > 0) *p++ = tmp[--n];
        }
        *p = '\0';

        ipc_send(&IPC->m7_to_m4, msg, (uint32_t)(p - msg));

        uart_puts(console, "[M7] sent: ");
        uart_puts(console, msg);
        uart_puts(console, "\n");

        /* Check for response from M4 */
        sched_sleep_ms(100);

        char reply[64] = {0};
        int len = ipc_recv(&IPC->m4_to_m7, reply, sizeof(reply) - 1);
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

    uart_puts(console, "\n=== STM32H745 M7 Core (480MHz) ===\n");
    uart_puts(console, "AMP: two cores, two firmwares, shared memory IPC\n\n");

    ipc_init();

    uart_puts(console, "[M7] Booting M4 core...\n");
    boot_m4_core();
    uart_puts(console, "[M7] M4 released from reset.\n\n");

#ifdef CONFIG_SCHED
    sched_create_task(app_task, "app");
    sched_create_task(idle_task, "idle");

#ifdef CONFIG_SYSTICK
    systick_init(DT_SYSCLK_HZ, 1000);
#endif
    sched_start();
#else
    while (1) {}
#endif
}
