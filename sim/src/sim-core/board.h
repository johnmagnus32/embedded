#ifndef BOARD_H
#define BOARD_H

#include "cpu.h"
#include "nvic.h"
#include "systick.h"
#include "uart.h"

struct board {
    struct cpu_state cpu;
    struct nvic      nvic;
    struct systick   systick;
    struct uart      uart;
    uint8_t         *flash;
    uint8_t         *ram;
};

void board_init(struct board *b);
void board_tick(struct board *b);

#endif
