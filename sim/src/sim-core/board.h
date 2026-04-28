#ifndef BOARD_H
#define BOARD_H

#include "stm32f411.h"
#include "ili9341.h"
#include "trace_dev.h"
#include "chardev.h"

struct board {
    struct stm32f411 soc;
    struct ili9341   *display;
    struct trace_dev trace;
};

void board_init(struct board *b, struct chardev_table *chardevs);
void board_tick(struct board *b);

#endif
