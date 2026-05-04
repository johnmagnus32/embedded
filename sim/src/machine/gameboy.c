/*
 * board.c — Board-level wiring: SoC + external devices
 *
 * Wires chardevs to UARTs, attaches ILI9341 display to SPI1,
 * registers trace_dev on the membus.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gameboy.h"
#include "machine.h"
#include "stm32_gpio.h"
#include "event_queue.h"

static void ili9341_refresh_cb(void *opaque);
static void io_poll_cb(void *opaque);
static void chardev_flush_cb(void *opaque);

void gameboy_init(struct gameboy *b, struct chardev_table *chardevs)
{
    stm32f411_init(&b->soc, BOARD_SYSCLK_HZ);
    b->display = NULL;

    /* Wire chardevs to UARTs */
    struct chardev *cd;
    cd = chardevs ? chardev_find(chardevs, "usart2") : NULL;
    if (cd) {
        stm32_uart_init(&b->soc.usarts[1], cd);
        fprintf(stderr, "[board] USART2 chardev wired\n");
    }

    /* Trace port at 0xE0000000 (debug feature, not part of SoC) */
    struct chardev *trace_cd = chardevs ? chardev_find(chardevs, "trace") : NULL;
    trace_dev_init(&b->trace, trace_cd, &b->soc.cpu.cycle_count);
    membus_register(&b->soc.bus, 0xE0000000, 0x04, trace_dev_read, trace_dev_write, &b->trace);

    /* ILI9341 display on SPI1 (index 0), CS on GPIOA pin 4, DC on GPIOA pin 3 */
    static struct ili9341 display;
    ili9341_init(&display);
    display.chardev = chardevs ? chardev_find(chardevs, "display") : NULL;
    b->display = &display;
    display.cycle_count_ptr = &b->soc.cpu.cycle_count;

    int si_idx = spi_bus_attach(&b->soc.spis[0].bus, &display, ili9341_transfer);
    if (si_idx >= 0) {
        /* Firmware doesn't drive CS — default to always selected */
        b->soc.spis[0].bus.slaves[si_idx].cs_active = 1;
    }
    /* DC pin: GPIOA pin 3 */
    b->soc.gpio[0].out[3].handler = ili9341_set_dc;
    b->soc.gpio[0].out[3].opaque = &display;

    fprintf(stderr, "[board] ILI9341 on SPI1, DC=PA3, CS=PA4\n");

    /* MAX98357A audio DAC — receives samples via I2S sink from SPI2 */
    struct chardev *audio_cd = chardevs ? chardev_find(chardevs, "audio") : NULL;
    max98357a_init(&b->audio);
    b->audio.cd = audio_cd;
    /* Wire SPI2 (I2S) → MAX98357A via I2S sink */
    b->soc.spis[1].i2s_sink = &b->audio.sink;
    fprintf(stderr, "[board] MAX98357A on I2S2 sink, chardev=%s\n", audio_cd ? "audio" : "none");

    /* W25Q128 external flash on SPI3, CS on PB0 */
    w25q128_init(&b->flash);
    b->flash.cycle_count_ptr = &b->soc.cpu.cycle_count;
    int flash_idx = spi_bus_attach(&b->soc.spis[2].bus, &b->flash, w25q128_transfer);
    /* CS pin: GPIOB pin 0 (active low) — toggles bus cs_active + device state */
    b->flash.spi_slave = &b->soc.spis[2].bus.slaves[flash_idx];
    b->soc.gpio[1].out[0].handler = w25q128_cs_handler;
    b->soc.gpio[1].out[0].opaque = &b->flash;
    fprintf(stderr, "[board] W25Q128 on SPI3, CS=PB0\n");

    /* Board I/O chardev for external input */
    b->io_chardev = chardevs ? chardev_find(chardevs, "io") : NULL;
    b->chardevs = chardevs;

    /* Schedule periodic events on the SoC event queue */
    event_schedule(&b->soc.eq, EVT_ILI9341_REFRESH, display.refresh_interval,
                   ili9341_refresh_cb, b);
    event_schedule(&b->soc.eq, EVT_IO_POLL, 10000, io_poll_cb, b);
    if (chardevs)
        event_schedule(&b->soc.eq, EVT_CHARDEV_FLUSH, 10000, chardev_flush_cb, b);
}

static void gameboy_poll_io(struct gameboy *b)
{
    static char buf[256];
    static int len = 0;
    uint8_t tmp[128];
    int n = chardev_read_nonblock(b->io_chardev, tmp, sizeof(tmp));
    if (n <= 0) return;
    if (len + n >= (int)sizeof(buf)) len = 0;
    memcpy(buf + len, tmp, n);
    len += n; buf[len] = 0;
    char *nl;
    while ((nl = strchr(buf, '\n'))) {
        *nl = 0;
        int port, pin, level;
        if (sscanf(buf, "gpio:%d:%d:%d", &port, &pin, &level) == 3
            && port >= 0 && port < 3 && pin >= 0 && pin < 16)
            stm32_gpio_set_input(&b->soc.gpio[port], pin, level);
        int ch, val;
        if (sscanf(buf, "dial:%d:%d", &ch, &val) == 2 && ch >= 0 && ch < 16)
            b->soc.adc.channels[ch] = (uint32_t)val;
        int rem = len - (int)(nl - buf + 1);
        memmove(buf, nl + 1, rem);
        len = rem;
    }
}

static void ili9341_refresh_cb(void *opaque)
{
    struct gameboy *b = (struct gameboy *)opaque;
    ili9341_flush(b->display);
    event_schedule(&b->soc.eq, EVT_ILI9341_REFRESH,
                   b->soc.cpu.cycle_count + b->display->refresh_interval,
                   ili9341_refresh_cb, b);
}

static void io_poll_cb(void *opaque)
{
    struct gameboy *b = (struct gameboy *)opaque;
    if (b->io_chardev) gameboy_poll_io(b);
    event_schedule(&b->soc.eq, EVT_IO_POLL,
                   b->soc.cpu.cycle_count + 10000,
                   io_poll_cb, b);
}

static void chardev_flush_cb(void *opaque)
{
    struct gameboy *b = (struct gameboy *)opaque;
    ili9341_send(b->display);  /* drain display frame via double-buffer */
    chardev_flush_all(b->chardevs);
    event_schedule(&b->soc.eq, EVT_CHARDEV_FLUSH,
                   b->soc.cpu.cycle_count + 10000,
                   chardev_flush_cb, b);
}

int gameboy_tick(struct gameboy *b)
{
    return stm32f411_tick(&b->soc);
}

/* Machine registry wrappers */
static void gameboy_init_wrap(void *board, struct chardev_table *cd)
{ gameboy_init((struct gameboy *)board, cd); }

static int gameboy_tick_wrap(void *board)
{ return gameboy_tick((struct gameboy *)board); }

static struct armv7m_cpu *gameboy_get_cpu(void *board)
{ return &((struct gameboy *)board)->soc.cpu; }

static struct membus *gameboy_get_bus(void *board)
{ return &((struct gameboy *)board)->soc.bus; }

static uint8_t **gameboy_get_flash(void *board)
{ return &((struct gameboy *)board)->soc.flash; }

static uint8_t **gameboy_get_ram(void *board)
{ return &((struct gameboy *)board)->soc.ram; }

static uint32_t gameboy_get_sysclk(void *board)
{ return ((struct gameboy *)board)->soc.sysclk_hz; }

static int gameboy_load_device(void *board, const char *name, const char *path)
{
    struct gameboy *b = (struct gameboy *)board;
    if (strcmp(name, "flash0") == 0) {
        int n = w25q128_load(&b->flash, path);
        if (n < 0) { fprintf(stderr, "[board] Failed to load flash: %s\n", path); return -1; }
        fprintf(stderr, "[board] Loaded %d bytes into W25Q128 from %s\n", n, path);
        return 0;
    }
    fprintf(stderr, "[board] Unknown device: %s\n", name);
    return -1;
}

const struct machine_desc gameboy_machine = {
    .name        = "gameboy",
    .description = "STM32F411 + ILI9341 display",
    .board_size  = sizeof(struct gameboy),
    .init        = gameboy_init_wrap,
    .tick        = gameboy_tick_wrap,
    .get_cpu     = gameboy_get_cpu,
    .get_bus     = gameboy_get_bus,
    .get_flash   = gameboy_get_flash,
    .get_ram     = gameboy_get_ram,
    .get_sysclk  = gameboy_get_sysclk,
    .load_device = gameboy_load_device,
};
