#ifndef BOARD_H
#define BOARD_H

#include "cpu.h"
#include "nvic.h"
#include "systick.h"
#include "uart.h"
#include "dts.h"

#define MAX_UARTS 3

struct board {
    struct cpu_state cpu;
    struct nvic      nvic;
    struct systick   systick;
    struct uart      uarts[MAX_UARTS];
    int              nuarts;
    uint32_t         sysclk_hz;
    uint8_t         *flash;
    uint8_t         *ram;
};

void board_init(struct board *b, const struct dts *dt);
void board_tick(struct board *b);

#endif
