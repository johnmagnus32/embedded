/*
 * board.c — Board-level wiring: SoC + external devices
 *
 * Wires chardevs to UARTs, attaches ILI9341 display to SPI1,
 * registers trace_dev on the membus.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "board.h"

void board_init(struct board *b, struct chardev_table *chardevs)
{
    stm32f411_init(&b->soc);
    b->display = NULL;

    /* Wire chardevs to UARTs */
    struct chardev *cd;
    cd = chardevs ? chardev_find(chardevs, "usart2") : NULL;
    if (cd) {
        uart_init(&b->soc.usarts[1], cd);
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
        /* CS pin: GPIOA pin 4 */
        b->soc.gpio[0].out[4].handler = spi_slave_cs_handler;
        b->soc.gpio[0].out[4].opaque = &b->soc.spis[0].bus.slaves[si_idx];
    }
    /* DC pin: GPIOA pin 3 */
    b->soc.gpio[0].out[3].handler = ili9341_set_dc;
    b->soc.gpio[0].out[3].opaque = &display;

    fprintf(stderr, "[board] ILI9341 on SPI1, DC=PA3, CS=PA4\n");
}

void board_tick(struct board *b)
{
    stm32f411_tick(&b->soc);
}
