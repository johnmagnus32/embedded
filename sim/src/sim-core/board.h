#ifndef BOARD_H
#define BOARD_H

#include "cpu.h"
#include "nvic.h"
#include "systick.h"

struct board {
    struct cpu_state cpu;
    struct nvic      nvic;
    struct systick   systick;
    uint8_t         *flash;
    uint8_t         *ram;

    /* Breakpoints (owned by debugger, checked by board) */
    uint32_t breakpoints[32];
    int      nbp;
    int      bp_hit;
};

void board_init(struct board *b);
void board_tick(struct board *b);

#endif
