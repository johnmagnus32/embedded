#ifndef GAMEBOY_H
#define GAMEBOY_H

#include "stm32f411.h"
#include "ili9341.h"
#include "trace_dev.h"
#include "chardev.h"

struct gameboy {
    struct stm32f411 soc;
    struct ili9341   *display;
    struct trace_dev trace;
};

void gameboy_init(struct gameboy *b, struct chardev_table *chardevs);
void gameboy_tick(struct gameboy *b);

#endif
