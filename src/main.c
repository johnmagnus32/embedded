/*
 * main.c — Demonstrates sync primitives and IPC
 *
 * Mutex:     protects UART (only one task prints at a time)
 * Semaphore: input_task signals button events to led_task
 * Message Q: input_task sends button names to log_task
 */

#include "devicetree.h"
#include "device.h"
#include "drivers/uart.h"
#include "drivers/gpio_keys.h"
#include "drivers/gpio_leds.h"
#include "sched.h"
#include "log.h"
#include "sync.h"
#include "msgq.h"

extern void systick_init(uint32_t cpu_hz, uint32_t tick_hz);

DEVICE_DT_DECLARE(DT_CHOSEN_CONSOLE);
DEVICE_DT_DECLARE(buttons);
DEVICE_DT_DECLARE(leds);

/* Mutex protecting direct UART access */
static struct mutex uart_mutex = MUTEX_INIT;

/* Semaphore: input_task gives when button pressed, led_task takes */
static struct semaphore btn_event = SEM_INIT(0);

/* Message queue: input_task sends button name strings to log_task */
struct btn_msg {
    char name[16];
};
static struct msgq btn_msgq = MSGQ_INIT(sizeof(struct btn_msg));

/* --- Key map --- */

static const struct { int code; const char *name; } keymap[] = {
    { KEY_UP, "UP" }, { KEY_DOWN, "DOWN" }, { KEY_LEFT, "LEFT" },
    { KEY_RIGHT, "RIGHT" }, { KEY_A, "A" }, { KEY_B, "B" },
};

/* --- Input task: detect presses, signal via sem + msgq --- */

static void input_task(void)
{
    const struct device *keys = DEVICE_DT_GET(buttons);
    uint8_t prev[6] = {0};

    while (1) {
        for (int i = 0; i < 6; i++) {
            int pressed = gpio_keys_is_pressed(keys, keymap[i].code);

            if (pressed && !prev[i]) {
                /* Signal the LED task via semaphore */
                sem_give(&btn_event);

                /* Send button name to log task via message queue */
                struct btn_msg msg = {0};
                const char *s = keymap[i].name;
                int j = 0;
                while (*s && j < 15) msg.name[j++] = *s++;
                msgq_put(&btn_msgq, &msg);
            }
            prev[i] = pressed;
        }
        sched_sleep_ms(10);
    }
}

/* --- LED task: waits on semaphore, blinks all LEDs --- */

static void led_task(void)
{
    const struct device *led_dev = DEVICE_DT_GET(leds);

    while (1) {
        /* Block until a button event arrives */
        sem_take(&btn_event);

        /* Flash all LEDs */
        for (int i = 0; i < 6; i++)
            gpio_leds_set(led_dev, i, 1);
        sched_sleep_ms(100);
        for (int i = 0; i < 6; i++)
            gpio_leds_set(led_dev, i, 0);
    }
}

/* --- Log task: reads from msgq, prints to UART with mutex --- */

static void log_msg_task(void)
{
    const struct device *console = DEVICE_DT_GET(DT_CHOSEN_CONSOLE);
    struct btn_msg msg;

    while (1) {
        /* Block until a message arrives */
        msgq_get(&btn_msgq, &msg);

        /* Lock UART mutex so our output isn't interleaved */
        mutex_lock(&uart_mutex);
        uart_puts(console, "[log] button pressed: ");
        uart_puts(console, msg.name);
        uart_puts(console, "\n");
        mutex_unlock(&uart_mutex);
    }
}

/* --- Idle task --- */

static void idle_task(void)
{
    while (1) {
        __asm volatile("wfi");
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

    uart_puts(console, "Booting with sync primitives...\n");

    sched_create_task(input_task, "input");
    sched_create_task(led_task, "led");
    sched_create_task(log_msg_task, "log");
    sched_create_task(idle_task, "idle");

    uart_puts(console, "Starting scheduler.\n");

    systick_init(DT_SYSCLK_HZ, 1000);
    sched_start();
}
