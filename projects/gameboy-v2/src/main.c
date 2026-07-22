/*
 * main.c — Application entry point
 *
 * Boots FPGA, initializes PPU, then runs game via RTOS tasks.
 */

#include "config.h"
#include "devicetree.h"
#include "device.h"
#include "drivers/uart.h"
#include "drivers/spi.h"
#include "drivers/audio.h"
#include "drivers/adc.h"
#include "drivers/gpio.h"
#include "sched.h"
#include "heap.h"
#include "board.h"
#include "buttons.h"
#include "audio.h"
#include "flash_audio.h"
#include "fpga_loader.h"
#include "game.h"

DEVICE_DT_DECLARE(DT_CHOSEN_CONSOLE);   /* usart2 during bring-up (see board.dts) */
DEVICE_DT_DECLARE(spi1);
DEVICE_DT_DECLARE(spi3);
DEVICE_DT_DECLARE(i2s2);
DEVICE_DT_DECLARE(adc1);
DEVICE_DT_DECLARE(gpioa);
DEVICE_DT_DECLARE(gpiob);
DEVICE_DT_DECLARE(gpioc);

const struct device *uart;
const struct device *spi_ppu;
const struct device *spi_cfg;
const struct device *audio_dev;
const struct device *adc_dev;
const struct device *dev_gpioa;
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

static void idle_task(void)
{
    while (1) {}
}

void main(void)
{
    uart      = DEVICE_DT_GET(DT_CHOSEN_CONSOLE);
    spi_ppu   = DEVICE_DT_GET(spi1);
    spi_cfg   = DEVICE_DT_GET(spi3);
    audio_dev = DEVICE_DT_GET(i2s2);
    adc_dev   = DEVICE_DT_GET(adc1);
    dev_gpioa = DEVICE_DT_GET(gpioa);
    dev_gpiob = DEVICE_DT_GET(gpiob);
    dev_gpioc = DEVICE_DT_GET(gpioc);

    heap_init(&_heap_start, (size_t)&_heap_size);

    device_init_all();

    /* Load FPGA bitstream before anything else */
    fpga_load_bitstream();

    buttons_init();

    uart_print("start\n");

    /* Audio + NOR-flash-audio disabled for display bring-up: focus on the
     * FPGA->ILI9341 path only. The PPU free-runs the background (sky-blue) with
     * no game commands, so game_task is optional for a first "does the screen
     * light up" test — left enabled so sprites still upload if wired, but the
     * screen shows the background regardless. Re-enable audio/upload later. */
    /* sched_create_task(audio_task,       "audio",  0); */
    /* sched_create_task(flash_audio_task, "upload", 1); */
    sched_create_task(game_task,        "game",   1);
    sched_create_task(idle_task,        "idle",   255);

    extern void systick_init(uint32_t cpu_hz, uint32_t tick_hz);
    systick_init(DT_SYSCLK_HZ, 1000);

    sched_start();
}
