/*
 * main.c — Multi-task application
 *
 * Tasks:
 *   input_task  — polls buttons, sets LEDs, logs presses
 *   log_task    — drains log ring buffer to UART
 *   idle_task   — runs when nothing else needs the CPU
 */

#include "devicetree.h"
#include "device.h"
#include "drivers/uart.h"
#include "drivers/gpio_keys.h"
#include "drivers/gpio_leds.h"
#include "sched.h"
#include "log.h"

DEVICE_DT_DECLARE(DT_CHOSEN_CONSOLE);
DEVICE_DT_DECLARE(buttons);
DEVICE_DT_DECLARE(leds);

/* --- Input task: poll buttons, drive LEDs, log changes --- */

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

            /* Log on press (edge detection) */
            if (pressed && !prev_state[i]) {
                log_printf("button %s pressed", keymap[i].name);
            }
            prev_state[i] = pressed;
        }
        sched_yield();
    }
}

/* --- Idle task: runs when nothing else is ready --- */

static void idle_task(void)
{
    while (1) {
        /* Could do WFI (wait for interrupt) here to save power */
        __asm volatile("nop");
        sched_yield();
    }
}

/* --- Boot: init hardware, create tasks, start scheduler --- */

int main(void)
{
    /* Init devices */
    const struct device *console = DEVICE_DT_GET(DT_CHOSEN_CONSOLE);
    const struct device *keys = DEVICE_DT_GET(buttons);
    const struct device *led_dev = DEVICE_DT_GET(leds);

    console->init(console);
    keys->init(keys);
    led_dev->init(led_dev);

    /* Direct print before scheduler starts (blocking) */
    uart_puts(console, "Booting...\n");

    /* Init logging subsystem */
    log_init();

    /* Create tasks */
    sched_create_task(input_task, "input");
    sched_create_task(log_task, "log");
    sched_create_task(idle_task, "idle");

    log_printf("system ready, %d tasks created", 3);

    /* Start scheduler — never returns */
    uart_puts(console, "Starting scheduler.\n");
    sched_start();
}
