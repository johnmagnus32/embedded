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
#include "sync.h"
#include "msgq.h"
#include "heap.h"
#include "memslab.h"
#include "net.h"

extern void systick_init(uint32_t cpu_hz, uint32_t tick_hz);

/* Linker-provided heap boundaries */
extern uint8_t _heap_start[];
extern uint32_t _heap_size;

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

/* Memory slab for LED event objects (fixed-size, O(1) alloc) */
struct led_event {
    uint8_t led_index;
    uint8_t on;
};
static uint8_t led_slab_buf[8 * sizeof(struct led_event)];
static struct memslab led_slab;

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

/* --- LED task: waits on semaphore, processes LED events from slab --- */

static void led_task(void)
{
    const struct device *led_dev = DEVICE_DT_GET(leds);

    while (1) {
        sem_take(&btn_event);

        /* Allocate an event from the slab (demonstrates memslab) */
        struct led_event *evt = memslab_alloc(&led_slab);
        if (evt) {
            /* Flash all LEDs */
            for (int i = 0; i < 6; i++)
                gpio_leds_set(led_dev, i, 1);
            sched_sleep_ms(100);
            for (int i = 0; i < 6; i++)
                gpio_leds_set(led_dev, i, 0);

            memslab_free(&led_slab, evt);
        }
    }
}

/* --- Log task: reads from msgq, prints to UART (uses heap for buffer) --- */

static void log_msg_task(void)
{
    const struct device *console = DEVICE_DT_GET(DT_CHOSEN_CONSOLE);
    struct btn_msg msg;

    while (1) {
        msgq_get(&btn_msgq, &msg);

        /* Allocate a format buffer from the heap (demonstrates heap_alloc) */
        char *buf = heap_alloc(48);
        if (buf) {
            /* Build string: "[log] pressed: NAME" */
            char *p = buf;
            const char *prefix = "[log] pressed: ";
            while (*prefix) *p++ = *prefix++;
            char *n = msg.name;
            while (*n) *p++ = *n++;
            *p = '\0';

            mutex_lock(&uart_mutex);
            uart_puts(console, buf);
            uart_puts(console, "\n");
            mutex_unlock(&uart_mutex);

            heap_free(buf);
        }
    }
}

/* --- Net sender task: sends a message every 500ms on port 1000 --- */

static void net_sender_task(void)
{
    int sock = net_socket_open(1000);
    int seq = 0;

    while (1) {
        char msg[32];
        /* Build "ping #N" */
        char *p = msg;
        const char *prefix = "ping #";
        while (*prefix) *p++ = *prefix++;
        /* itoa for seq */
        if (seq == 0) { *p++ = '0'; }
        else {
            char digits[10]; int n = 0; int v = seq;
            while (v > 0) { digits[n++] = '0' + v % 10; v /= 10; }
            while (n > 0) *p++ = digits[--n];
        }
        *p = '\0';

        net_send(sock, 2000, msg, (size_t)(p - msg));
        seq++;
        sched_sleep_ms(500);
    }
}

/* --- Net receiver task: listens on port 2000, logs received messages --- */

static void net_receiver_task(void)
{
    const struct device *console = DEVICE_DT_GET(DT_CHOSEN_CONSOLE);
    int sock = net_socket_open(2000);
    char buf[NET_BUF_SIZE];

    while (1) {
        int len = net_recv(sock, buf, sizeof(buf) - 1);
        if (len > 0) {
            buf[len] = '\0';
            mutex_lock(&uart_mutex);
            uart_puts(console, "[net] received: ");
            uart_puts(console, buf);
            uart_puts(console, "\n");
            mutex_unlock(&uart_mutex);
        }
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

    /* Init memory allocators */
    heap_init(_heap_start, (size_t)&_heap_size);
    memslab_init(&led_slab, led_slab_buf,
                 sizeof(struct led_event), 8);

    /* Init network stack */
    net_init();

    sched_create_task(input_task, "input");
    sched_create_task(led_task, "led");
    sched_create_task(log_msg_task, "log");
    sched_create_task(net_sender_task, "netsend");
    sched_create_task(net_receiver_task, "netrecv");
    sched_create_task(idle_task, "idle");

    uart_puts(console, "Starting scheduler.\n");

    systick_init(DT_SYSCLK_HZ, 1000);
    sched_start();
}
