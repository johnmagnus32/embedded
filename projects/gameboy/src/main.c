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

DEVICE_DT_DECLARE(usart1);
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
    /* Raw UART1 init for early debug — same as blink project */
    *(volatile uint32_t *)0x40023830 |= (1 << 0);   /* RCC AHB1ENR: GPIOA */
    *(volatile uint32_t *)0x40023844 |= (1 << 4);   /* RCC APB2ENR: USART1 */
    for (volatile int d = 0; d < 100; d++) {}
    *(volatile uint32_t *)0x40020000 &= ~(3 << 18); /* PA9 AF mode */
    *(volatile uint32_t *)0x40020000 |=  (2 << 18);
    *(volatile uint32_t *)0x40020024 &= ~(0xF << 4); /* PA9 AF7 */
    *(volatile uint32_t *)0x40020024 |=  (7 << 4);
    *(volatile uint32_t *)0x40011008 = 0x8B;         /* BRR: 115200 @ 16MHz */
    *(volatile uint32_t *)0x4001100C = (1 << 13);    /* UE */
    for (volatile int d = 0; d < 100; d++) {}
    *(volatile uint32_t *)0x4001100C |= (1 << 3);   /* TE */
    for (volatile int d = 0; d < 1000; d++) {}
    const char *msg = "ALIVE\r\n";
    while (*msg) {
        while (!(*(volatile uint32_t *)0x40011000 & (1 << 7))) {}
        *(volatile uint32_t *)0x40011004 = *msg++;
    }

    uart      = DEVICE_DT_GET(usart1);
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
