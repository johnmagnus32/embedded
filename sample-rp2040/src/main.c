/*
 * main.c — RP2040 Pico sample (dual-core Cortex-M0+)
 *
 * Demonstrates the same OS running on a completely different chip.
 * Same kernel, same driver API, different drivers underneath.
 */

#include "config.h"
#include "devicetree.h"
#include "device.h"

#ifdef CONFIG_UART
#include "drivers/uart.h"
#endif
#ifdef CONFIG_GPIO_LEDS
#include "drivers/gpio_leds.h"
#endif
#ifdef CONFIG_SCHED
#include "sched.h"
#endif
#ifdef CONFIG_SYNC
#include "sync.h"
#endif
#ifdef CONFIG_HEAP
#include "heap.h"
#endif
#ifdef CONFIG_SHELL
#include "shell.h"
#endif

#ifdef CONFIG_SYSTICK
extern void systick_init(uint32_t cpu_hz, uint32_t tick_hz);
#endif

#ifdef CONFIG_HEAP
extern uint8_t _heap_start[];
extern uint32_t _heap_size;
#endif

DEVICE_DT_DECLARE(DT_CHOSEN_CONSOLE);

#ifdef CONFIG_SCHED

static void blink_task(void)
{
    const struct device *console = DEVICE_DT_GET(DT_CHOSEN_CONSOLE);
    int count = 0;

    while (1) {
        uart_puts(console, "blink ");

        /* Simple number print */
        char buf[8];
        int n = count;
        int i = 0;
        if (n == 0) buf[i++] = '0';
        else {
            char tmp[8]; int j = 0;
            while (n > 0) { tmp[j++] = '0' + n % 10; n /= 10; }
            while (j > 0) buf[i++] = tmp[--j];
        }
        buf[i] = '\0';
        uart_puts(console, buf);
        uart_puts(console, "\n");

        count++;
        sched_sleep_ms(1000);
    }
}

static void idle_task(void)
{
    while (1) { __asm volatile("wfi"); }
}

#endif /* CONFIG_SCHED */

int main(void)
{
    const struct device *console = DEVICE_DT_GET(DT_CHOSEN_CONSOLE);
    console->init(console);

    uart_puts(console, "\n=== RP2040 Pico ===\n");
    uart_puts(console, "Dual Cortex-M0+, 264KB RAM\n");
    uart_puts(console, "Same OS, different chip.\n\n");

#ifdef CONFIG_HEAP
    heap_init(_heap_start, (size_t)&_heap_size);
#endif

#ifdef CONFIG_SCHED
    sched_create_task(blink_task, "blink");
#ifdef CONFIG_SHELL
    sched_create_task(shell_task, "shell");
#endif
    sched_create_task(idle_task, "idle");

#ifdef CONFIG_SYSTICK
    systick_init(DT_SYSCLK_HZ, 1000);
#endif
    sched_start();
#else
    uart_puts(console, "No scheduler. Halting.\n");
    while (1) {}
#endif
}
