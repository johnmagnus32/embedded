#ifndef BOARD_H
#define BOARD_H

#include "cpu.h"
#include "membus.h"
#include "nvic.h"
#include "systick.h"
#include "uart.h"
#include "spi.h"
#include "ili9341.h"
#include "trace_dev.h"
#include "gpio.h"
#include "chardev.h"
#include "dts.h"

#define MAX_UARTS 3
#define MAX_SPIS  2
#define MAX_GPIO  2

struct board {
    struct cpu_state cpu;
    struct membus    bus;
    struct nvic      nvic;
    struct systick   systick;
    struct uart      uarts[MAX_UARTS];
    int              nuarts;
    struct spi       spis[MAX_SPIS];
    int              nspis;
    struct gpio_port gpio[MAX_GPIO];
    int              ngpio;
    struct ili9341   *display;
    struct trace_dev trace;
    uint32_t         sysclk_hz;
    uint8_t         *flash;
    uint8_t         *ram;
};

void board_init(struct board *b, const struct dts *dt, struct chardev_table *chardevs);
void board_init_membus(struct board *b, const struct dts *dt);
void board_tick(struct board *b);

#endif
