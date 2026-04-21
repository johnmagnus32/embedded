/*
 * main.c — Demonstrates all RTOS features + QSPI NOR flash
 *
 * Tasks:
 *   input_task  — polls buttons, signals via sem + msgq
 *   led_task    — flashes LEDs on button events (memslab)
 *   log_task    — prints messages to UART (heap + mutex)
 *   flash_task  — reads/writes NOR flash, logs results
 *   idle_task   — WFI
 */

#include "config.h"
#include "devicetree.h"
#include "device.h"

#ifdef CONFIG_UART
#include "drivers/uart.h"
#endif
#ifdef CONFIG_GPIO_KEYS
#include "drivers/gpio_keys.h"
#endif
#ifdef CONFIG_GPIO_LEDS
#include "drivers/gpio_leds.h"
#endif
#ifdef CONFIG_FLASH
#include "drivers/flash.h"
#endif
#ifdef CONFIG_SCHED
#include "sched.h"
#endif
#ifdef CONFIG_SYNC
#include "sync.h"
#endif
#ifdef CONFIG_MSGQ
#include "msgq.h"
#endif
#ifdef CONFIG_HEAP
#include "heap.h"
#endif
#ifdef CONFIG_MEMSLAB
#include "memslab.h"
#endif
#ifdef CONFIG_FS
#include "fs.h"
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
DEVICE_DT_DECLARE(rcc);
#ifdef CONFIG_GPIO_KEYS
DEVICE_DT_DECLARE(buttons);
#endif
#ifdef CONFIG_GPIO_LEDS
DEVICE_DT_DECLARE(leds);
#endif
#ifdef CONFIG_SPI
DEVICE_DT_DECLARE(spi1);
#endif
#ifdef CONFIG_FLASH
DEVICE_DT_DECLARE(flash0);
#endif

/* Sync primitives */
#ifdef CONFIG_SYNC
static struct mutex uart_mutex = MUTEX_INIT;
static struct semaphore btn_event = SEM_INIT(0);
#endif

#ifdef CONFIG_MSGQ
struct btn_msg { char name[16]; };
static struct msgq btn_msgq = MSGQ_INIT(sizeof(struct btn_msg));
#endif

#ifdef CONFIG_MEMSLAB
struct led_event { uint8_t led_index; uint8_t on; };
static uint8_t led_slab_buf[8 * sizeof(struct led_event)];
static struct memslab led_slab;
#endif

#ifdef CONFIG_SCHED
static const struct { int code; const char *name; } keymap[] = {
    { KEY_UP, "UP" }, { KEY_DOWN, "DOWN" }, { KEY_LEFT, "LEFT" },
    { KEY_RIGHT, "RIGHT" }, { KEY_A, "A" }, { KEY_B, "B" },
};
#endif

#ifdef CONFIG_SCHED
/* --- Input task --- */

static void input_task(void)
{
    const struct device *keys = DEVICE_DT_GET(buttons);
    uint8_t prev[6] = {0};

    while (1) {
        for (int i = 0; i < 6; i++) {
            int pressed = gpio_keys_is_pressed(keys, keymap[i].code);
            if (pressed && !prev[i]) {
                sem_give(&btn_event);
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

/* --- LED task --- */

static void led_task(void)
{
    const struct device *led_dev = DEVICE_DT_GET(leds);

    while (1) {
        sem_take(&btn_event);
        struct led_event *evt = memslab_alloc(&led_slab);
        if (evt) {
            for (int i = 0; i < 6; i++) gpio_leds_set(led_dev, i, 1);
            sched_sleep_ms(100);
            for (int i = 0; i < 6; i++) gpio_leds_set(led_dev, i, 0);
            memslab_free(&led_slab, evt);
        }
    }
}

/* --- Log task --- */

static void log_msg_task(void)
{
    const struct device *console = DEVICE_DT_GET(DT_CHOSEN_CONSOLE);
    struct btn_msg msg;

    while (1) {
        msgq_get(&btn_msgq, &msg);
        char *buf = heap_alloc(48);
        if (buf) {
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

/* --- FS list callback --- */

static const struct device *_ls_console;
static void ls_print(const char *name, uint32_t size)
{
    mutex_lock(&uart_mutex);
    uart_puts(_ls_console, "  ");
    uart_puts(_ls_console, name);
    uart_puts(_ls_console, "\n");
    mutex_unlock(&uart_mutex);
}

/* --- Flash task: mount filesystem, create and read files --- */

static struct fs_mount mnt;

static void flash_task(void)
{
    const struct device *console = DEVICE_DT_GET(DT_CHOSEN_CONSOLE);
    const struct device *flash = DEVICE_DT_GET(flash0);
    _ls_console = console;

    sched_sleep_ms(100);

    /* Mount filesystem on NOR flash */
    mutex_lock(&uart_mutex);
    uart_puts(console, "[fs] mounting filesystem on flash...\n");
    mutex_unlock(&uart_mutex);

    fs_mount(&mnt, flash, &tinyfs_ops);

    /* Create and write a file */
    struct fs_file f;
    fs_open(&f, &mnt, "hello.txt");
    fs_write(&f, "Hello from TinyFS!", 18);
    fs_close(&f);

    /* Create another file */
    fs_open(&f, &mnt, "count.txt");
    fs_write(&f, "42", 2);
    fs_close(&f);

    /* List files */
    mutex_lock(&uart_mutex);
    uart_puts(console, "[fs] files:\n");
    mutex_unlock(&uart_mutex);
    fs_ls(&mnt, ls_print);

    /* Read back hello.txt */
    char buf[32] = {0};
    fs_open(&f, &mnt, "hello.txt");
    fs_read(&f, buf, sizeof(buf) - 1);
    fs_close(&f);

    mutex_lock(&uart_mutex);
    uart_puts(console, "[fs] read hello.txt: ");
    uart_puts(console, buf);
    uart_puts(console, "\n");
    mutex_unlock(&uart_mutex);

    while (1) sched_sleep_ms(10000);
}

/* --- Idle task --- */

static void idle_task(void)
{
    while (1) { __asm volatile("wfi"); }
}
#endif /* CONFIG_SCHED */

/* --- Boot --- */

int main(void)
{
    const struct device *console = DEVICE_DT_GET(DT_CHOSEN_CONSOLE);
    console->init(console);

#ifdef CONFIG_GPIO_KEYS
    DEVICE_DT_GET(buttons)->init(DEVICE_DT_GET(buttons));
#endif
#ifdef CONFIG_GPIO_LEDS
    DEVICE_DT_GET(leds)->init(DEVICE_DT_GET(leds));
#endif
#ifdef CONFIG_SPI
    DEVICE_DT_GET(spi1)->init(DEVICE_DT_GET(spi1));
#endif
#ifdef CONFIG_FLASH
    DEVICE_DT_GET(flash0)->init(DEVICE_DT_GET(flash0));
#endif

    uart_puts(console, "Booting...\n");

#ifdef CONFIG_HEAP
    heap_init(_heap_start, (size_t)&_heap_size);
#endif
#ifdef CONFIG_MEMSLAB
    memslab_init(&led_slab, led_slab_buf, sizeof(struct led_event), 8);
#endif

#ifdef CONFIG_SCHED
    sched_create_task(input_task, "input");
    sched_create_task(led_task, "led");
    sched_create_task(log_msg_task, "log");
#ifdef CONFIG_FS
    sched_create_task(flash_task, "flash");
#endif
#ifdef CONFIG_SHELL
    sched_create_task(shell_task, "shell");
#endif
    sched_create_task(idle_task, "idle");

    uart_puts(console, "Starting scheduler.\n");
#ifdef CONFIG_SYSTICK
    systick_init(DT_SYSCLK_HZ, 1000);
#endif
    sched_start();
#else
    /* No scheduler — just loop */
    uart_puts(console, "No scheduler, halting.\n");
    while (1) {}
#endif
}
