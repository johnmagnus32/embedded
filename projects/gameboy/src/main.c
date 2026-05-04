/*
 * main.c — Application entry point (setup only)
 *
 * Zero register addresses. All hardware access through OS driver APIs.
 */

#include "config.h"
#include "devicetree.h"
#include "device.h"
#include "drivers/uart.h"
#include "drivers/display.h"
#include "drivers/audio.h"
#include "drivers/adc.h"
#include "drivers/gpio.h"
#include "sched.h"
#include "heap.h"
#include "app.h"

DEVICE_DT_DECLARE(usart2);
DEVICE_DT_DECLARE(ili9341);
DEVICE_DT_DECLARE(i2s2);
DEVICE_DT_DECLARE(adc1);
DEVICE_DT_DECLARE(gpiob);

const struct device *uart;
const struct device *display;
const struct device *audio_dev;
const struct device *adc_dev;
const struct device *dev_gpiob;

void uart_print(const char *s)
{
    while (*s) uart_poll_out(uart, *s++);
}

void print_int(int n)
{
    if (n < 0) { uart_poll_out(uart, '-'); n = -n; }
    if (n >= 10) print_int(n / 10);
    uart_poll_out(uart, '0' + (n % 10));
}

extern char _heap_start;
extern char _heap_size;

void main(void)
{
    uart      = DEVICE_DT_GET(usart2);
    display   = DEVICE_DT_GET(ili9341);
    audio_dev = DEVICE_DT_GET(i2s2);
    adc_dev   = DEVICE_DT_GET(adc1);
    dev_gpiob = DEVICE_DT_GET(gpiob);

    heap_init(&_heap_start, (size_t)&_heap_size);

    /* Initialize all device drivers (iterates device_area section) */
    device_init_all();

    /* Configure button pins and register EXTI callbacks */
    buttons_init();

    uart_print("start\n");

    /* Start DMA audio — fill_audio runs in ISR context */
    //extern void start_audio(void);
    //start_audio();

    //sched_create_task(task_a,     "task_a", 1);
    //sched_create_task(task_b,     "task_b", 1);
    sched_create_task(task_game,  "game",   1);
    sched_create_task(idle_task,  "idle",   255);

    extern void systick_init(uint32_t cpu_hz, uint32_t tick_hz);
    systick_init(DT_SYSCLK_HZ, 1000);

    sched_start();
}
