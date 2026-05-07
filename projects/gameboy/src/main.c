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
DEVICE_DT_DECLARE(gpioc);

const struct device *uart;
const struct device *display;
const struct device *audio_dev;
const struct device *adc_dev;
const struct device *dev_gpiob;
const struct device *dev_gpioc;

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
    /* Early clock enables — needed before device_init_all */
    *(volatile uint32_t *)0x40023830 |= (1 << 0);  /* GPIOA clock */
    *(volatile uint32_t *)0x40023830 |= (1 << 1);  /* GPIOB clock */
    *(volatile uint32_t *)0x40023830 |= (1 << 2);  /* GPIOC clock */
    *(volatile uint32_t *)0x40023844 |= (1 << 4);  /* USART1 clock */
    *(volatile uint32_t *)0x40023844 |= (1 << 12); /* SPI1 clock */
    *(volatile uint32_t *)0x40023844 |= (1 << 14); /* SYSCFG clock (needed for EXTI routing) */
    *(volatile uint32_t *)0x40023840 |= (1 << 14); /* SPI2/I2S2 clock (APB1) */
    /* Enable PLLI2S with defaults (N=192, R=2 → I2SCLK≈96MHz) */
    *(volatile uint32_t *)0x40023844 &= ~(1 << 22); /* I2SSRC=0 (PLLI2S) */
    *(volatile uint32_t *)0x40023800 |= (1 << 26); /* CR: PLLI2SON */
    while (!(*(volatile uint32_t *)0x40023800 & (1 << 27))) {} /* wait PLLI2SRDY */
    /* SPI1 pins: PA5=SCK(AF5), PA6=MISO(AF5), PA7=MOSI(AF5) */
    *(volatile uint32_t *)0x40020000 = (*(volatile uint32_t *)0x40020000 & ~((3<<10)|(3<<12)|(3<<14)))
                                       | (2<<10)|(2<<12)|(2<<14);  /* AF mode */
    *(volatile uint32_t *)0x40020020 = (*(volatile uint32_t *)0x40020020 & ~((0xF<<20)|(0xF<<24)|(0xF<<28)))
                                       | (5<<20)|(5<<24)|(5<<28);  /* AF5 */
    /* PA4=CS output, high */
    *(volatile uint32_t *)0x40020000 = (*(volatile uint32_t *)0x40020000 & ~(3<<8)) | (1<<8);
    *(volatile uint32_t *)0x40020018 = (1<<4);
    /* PB5=DC output, high */
    *(volatile uint32_t *)0x40020400 = (*(volatile uint32_t *)0x40020400 & ~(3<<10)) | (1<<10);
    *(volatile uint32_t *)0x40020418 = (1<<5);
    /* SPI1: master, /2, SSM+SSI, enable */
    *(volatile uint32_t *)0x40013000 = (1<<2)|(1<<8)|(1<<9);
    *(volatile uint32_t *)0x40013000 |= (1<<6);
    /* I2S2 pins: PB12=WS(AF5), PB13=SCK(AF5), PB15=SD(AF5) */
    *(volatile uint32_t *)0x40020400 = (*(volatile uint32_t *)0x40020400 & ~((3<<24)|(3<<26)|(3<<30)))
                                       | (2<<24)|(2<<26)|(2<<30);  /* AF mode */
    *(volatile uint32_t *)0x40020424 = (*(volatile uint32_t *)0x40020424 & ~((0xF<<16)|(0xF<<20)|(0xF<<28)))
                                       | (5<<16)|(5<<20)|(5<<28);  /* AF5 */
    *(volatile uint32_t *)0x40020000 &= ~(3 << 18); *(volatile uint32_t *)0x40020000 |= (2 << 18);
    *(volatile uint32_t *)0x40020024 &= ~(0xF << 4); *(volatile uint32_t *)0x40020024 |= (7 << 4);
    *(volatile uint32_t *)0x40011008 = 0x8B;
    *(volatile uint32_t *)0x4001100C = (1<<13)|(1<<3);
    for (volatile int d=0;d<1000;d++){}
    const char *m = "OK\r\n"; while(*m){while(!(*(volatile uint32_t*)0x40011000&(1<<7))){} *(volatile uint32_t*)0x40011004=*m++;}

    uart      = DEVICE_DT_GET(usart1);
    display   = DEVICE_DT_GET(ili9341);
    audio_dev = DEVICE_DT_GET(i2s2);
    adc_dev   = DEVICE_DT_GET(adc1);
    dev_gpiob = DEVICE_DT_GET(gpiob);
    dev_gpioc = DEVICE_DT_GET(gpioc);

    heap_init(&_heap_start, (size_t)&_heap_size);

    /* Initialize all device drivers (iterates device_area section) */
    device_init_all();

    /* Configure button pins and register EXTI callbacks */
    buttons_init();

    uart_print("start\n");
    /* Debug: print PLLCFGR PLLM value */
    uart_print("PLLM=");
    print_int(*(volatile uint32_t *)0x40023804 & 0x3F);
    uart_print(" I2SCFGR=");
    print_int(*(volatile uint32_t *)0x40023884);
    uart_print("\n");

    /* Start DMA audio — fill_audio runs in ISR context */
    extern void start_audio(void);
    start_audio();

    //sched_create_task(task_a,     "task_a", 1);
    //sched_create_task(task_b,     "task_b", 1);
    sched_create_task(task_game,  "game",   1);
    sched_create_task(idle_task,  "idle",   255);

    extern void systick_init(uint32_t cpu_hz, uint32_t tick_hz);
    systick_init(DT_SYSCLK_HZ, 1000);

    sched_start();
}
