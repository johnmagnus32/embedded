#ifndef BOARD_H
#define BOARD_H

#include "cpu.h"
#include "nvic.h"
#include "systick.h"
#include "uart.h"
#include "trace_dev.h"
#include "chardev.h"
#include "dts.h"

#define MAX_UARTS 3

struct board {
    struct cpu_state cpu;
    struct nvic      nvic;
    struct systick   systick;
    struct uart      uarts[MAX_UARTS];
    int              nuarts;
    struct trace_dev trace;
    uint32_t         sysclk_hz;
    uint8_t         *flash;
    uint8_t         *ram;
};

void board_init(struct board *b, const struct dts *dt, struct chardev_table *chardevs);
void board_tick(struct board *b);

#endif
