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
#include "stm32_exti.h"

void gameboy_init(struct gameboy *b, struct chardev_table *chardevs)
{
    stm32f411_init(&b->soc);
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

    int si_idx = spi_bus_attach(&b->soc.spis[0].bus, &display, ili9341_transfer);
    if (si_idx >= 0) {
        /* Firmware doesn't drive CS — default to always selected */
        b->soc.spis[0].bus.slaves[si_idx].cs_active = 1;
    }
    /* DC pin: GPIOA pin 3 */
    b->soc.gpio[0].out[3].handler = ili9341_set_dc;
    b->soc.gpio[0].out[3].opaque = &display;

    /* Re-wire GPIOB pins 0-4 → EXTI inputs (buttons on GPIOB) */
    for (int i = 0; i < 5; i++) {
        b->soc.gpio[0].idr_change[i].handler = NULL;
        b->soc.gpio[0].idr_change[i].opaque = NULL;
        b->soc.gpio[1].idr_change[i].handler = stm32_exti_input_handler;
        b->soc.gpio[1].idr_change[i].opaque = &b->soc.exti_inputs[i];
    }

    fprintf(stderr, "[board] ILI9341 on SPI1, DC=PA3, CS=PA4\n");
}

void gameboy_tick(struct gameboy *b)
{
    stm32f411_tick(&b->soc);
}

/* Machine registry wrappers */
static void gameboy_init_wrap(void *board, struct chardev_table *cd)
{ gameboy_init((struct gameboy *)board, cd); }

static void gameboy_tick_wrap(void *board)
{ gameboy_tick((struct gameboy *)board); }

static struct cpu_state *gameboy_get_cpu(void *board)
{ return &((struct gameboy *)board)->soc.cpu; }

static struct membus *gameboy_get_bus(void *board)
{ return &((struct gameboy *)board)->soc.bus; }

static uint8_t **gameboy_get_flash(void *board)
{ return &((struct gameboy *)board)->soc.flash; }

static uint8_t **gameboy_get_ram(void *board)
{ return &((struct gameboy *)board)->soc.ram; }

static struct armv7m_nvic *gameboy_get_nvic(void *board)
{ return &((struct gameboy *)board)->soc.nvic; }

static struct stm32_gpio *gameboy_get_gpio(void *board, int port)
{ return &((struct gameboy *)board)->soc.gpio[port]; }

static struct ili9341 *gameboy_get_display(void *board)
{ return ((struct gameboy *)board)->display; }

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
    .get_nvic    = gameboy_get_nvic,
    .get_gpio    = gameboy_get_gpio,
    .get_display = gameboy_get_display,
};
