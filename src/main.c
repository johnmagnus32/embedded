/*
 * main.c — Preemptive multi-task application
 *
 * Tasks:
 *   input_task  — polls buttons, sets LEDs, logs presses (10ms period)
 *   log_task    — drains log ring buffer to UART
 *   idle_task   — runs when nothing else needs the CPU
 *
 * SysTick fires every 1ms and preempts the running task via PendSV.
 * Tasks can also sleep with sched_sleep_ms().
 */

#include "devicetree.h"
#include "device.h"
#include "drivers/uart.h"
#include "drivers/gpio_keys.h"
#include "drivers/gpio_leds.h"
#include "sched.h"
#include "log.h"

/* systick.c */
extern void systick_init(uint32_t cpu_hz, uint32_t tick_hz);

DEVICE_DT_DECLARE(DT_CHOSEN_CONSOLE);
DEVICE_DT_DECLARE(buttons);
DEVICE_DT_DECLARE(leds);

/* --- Input task --- */

static const struct { int code; const char *name; } keymap[] = {
    { KEY_UP, "UP" }, { KEY_DOWN, "DOWN" }, { KEY_LEFT, "LEFT" },
    { KEY_RIGHT, "RIGHT" }, { KEY_A, "A" }, { KEY_B, "B" },
};

static void input_task(void)
{
    const struct device *keys = DEVICE_DT_GET(buttons);
    const struct device *led_dev = DEVICE_DT_GET(leds);
    uint8_t prev_state[6] = {0};

    while (1) {
        for (int i = 0; i < 6; i++) {
            int pressed = gpio_keys_is_pressed(keys, keymap[i].code);
            gpio_leds_set(led_dev, i, pressed);

            if (pressed && !prev_state[i]) {
                log_printf("button %s pressed", keymap[i].name);
            }
            prev_state[i] = pressed;
        }
        sched_sleep_ms(10);  /* poll at 100Hz */
    }
}

/* --- Log task --- */

/* log_task is defined in log.c, but it uses sched_yield() internally.
 * With preemption it also gets timesliced automatically. */

/* --- Idle task --- */

static void idle_task(void)
{
    while (1) {
        __asm volatile("wfi");  /* wait for interrupt — saves power */
    }
}

/* --- Boot --- */

int main(void)
{
    const struct device *console = DEVICE_DT_GET(DT_CHOSEN_CONSOLE);
    const struct device *keys = DEVICE_DT_GET(buttons);
    const struct device *led_dev = DEVICE_DT_GET(leds);

    console->init(console);
    keys->init(keys);
    led_dev->init(led_dev);

    uart_puts(console, "Booting...\n");

    log_init();

    sched_create_task(input_task, "input");
    sched_create_task(log_task, "log");
    sched_create_task(idle_task, "idle");

    uart_puts(console, "Starting scheduler (preemptive, 1ms tick).\n");

    /* Start SysTick: 16MHz CPU, 1000Hz tick = 1ms */
    systick_init(DT_SYSCLK_HZ, 1000);

    /* Start scheduler — never returns */
    sched_start();
}
