/*
 * main.c — RTOS demo: tasks with heap tracing and ILI9341 display
 */

#include "config.h"
#include "devicetree.h"
#include "device.h"
#include "drivers/uart.h"
#include "sched.h"
#include "heap.h"
#include "trace.h"
#include <stdint.h>

DEVICE_DT_DECLARE(usart2);

static const struct device *uart;

/* ── Minimal ILI9341 SPI driver ── */
#define SPI1_BASE 0x40013000
#define SPI_CR1   (*(volatile uint32_t *)(SPI1_BASE + 0x00))
#define SPI_SR    (*(volatile uint32_t *)(SPI1_BASE + 0x08))
#define SPI_DR    (*(volatile uint32_t *)(SPI1_BASE + 0x0C))

#define GPIOA_BSRR (*(volatile uint32_t *)0x40020018)
#define DC_PIN 3
#define DC_CMD()  GPIOA_BSRR = (1 << (DC_PIN + 16))  /* DC low = command */
#define DC_DATA() GPIOA_BSRR = (1 << DC_PIN)          /* DC high = data */

static void spi_send(uint8_t b) { SPI_DR = b; }

static void lcd_cmd(uint8_t cmd)  { DC_CMD();  spi_send(cmd); }
static void lcd_data(uint8_t d)   { DC_DATA(); spi_send(d); }

static void lcd_init(void)
{
    SPI_CR1 = (1 << 6) | (1 << 2); /* SPE + MSTR */
    lcd_cmd(0x11); /* Sleep out */
    lcd_cmd(0x29); /* Display on */
}

static void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    lcd_cmd(0x2A);
    lcd_data(x0 >> 8); lcd_data(x0 & 0xFF);
    lcd_data(x1 >> 8); lcd_data(x1 & 0xFF);
    lcd_cmd(0x2B);
    lcd_data(y0 >> 8); lcd_data(y0 & 0xFF);
    lcd_data(y1 >> 8); lcd_data(y1 & 0xFF);
    lcd_cmd(0x2C);
}

static void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    lcd_set_window(x, y, x + w - 1, y + h - 1);
    for (uint32_t i = 0; i < (uint32_t)w * h; i++) {
        lcd_data(color >> 8);
        lcd_data(color & 0xFF);
    }
}

/* RGB565 color helpers */
#define RGB565(r,g,b) ((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|(((b)&0xF8)>>3))
#define BLACK   0x0000
#define WHITE   0xFFFF
#define RED     RGB565(255,0,0)
#define GREEN   RGB565(0,255,0)
#define BLUE    RGB565(0,0,255)
#define YELLOW  RGB565(255,255,0)
#define CYAN    RGB565(0,255,255)

static const struct device *uart;

static void uart_print(const char *s)
{
    while (*s) {
        if (*s == '\n')
            uart_poll_out(uart, '\r');
        uart_poll_out(uart, *s++);
    }
}

/* Traced heap wrappers */
static void *talloc(const char *name, unsigned size)
{
    void *p = heap_alloc(size);
    if (p) trace_alloc(name, p, size);
    return p;
}

static void tfree(void *p)
{
    if (p) trace_free(p);
    heap_free(p);
}

static void task_a(void)
{
    void *sensor = talloc("sensor_ctx", 64);
    void *buf    = talloc("tx_buffer", 128);
    while (1) {
        uart_print("task_a: working\n");
        sched_sleep_ms(200);
    }
    (void)sensor; (void)buf;
}

static void task_b(void)
{
    void *cfg = talloc("config", 32);
    sched_sleep_ms(100);
    void *tmp = talloc("temp_data", 48);
    sched_sleep_ms(300);
    uart_print("task_b: freeing temp_data\n");
    tfree(tmp);
    while (1) {
        uart_print("task_b: working\n");
        sched_sleep_ms(200);
    }
    (void)cfg;
}

static void task_c(void)
{
    static const uint16_t colors[] = {RED, GREEN, BLUE, YELLOW, CYAN, WHITE};
    int frame = 0;
    while (1) {
        uint16_t col = colors[frame % 6];
        /* Draw a moving bar */
        int y = (frame * 20) % 320;
        lcd_fill_rect(0, y, 240, 20, col);
        uart_print("task_c: draw\n");
        sched_sleep_ms(150);
        frame++;
    }
}

static void idle_task(void)
{
    while (1) {}
}

extern char _heap_start;
extern char _heap_size;

void main(void)
{
    uart = DEVICE_DT_GET(usart2);

    heap_init(&_heap_start, (size_t)&_heap_size);
    lcd_init();

    uart_print("RTOS demo — heap tracing\n\n");

    sched_create_task(task_a,    "task_a", 1);
    sched_create_task(task_b,    "task_b", 1);
    sched_create_task(task_c,    "task_c", 1);
    sched_create_task(idle_task, "idle",   255);

    extern void systick_init(uint32_t cpu_hz, uint32_t tick_hz);
    systick_init(DT_SYSCLK_HZ, 1000);

    sched_start();
}
