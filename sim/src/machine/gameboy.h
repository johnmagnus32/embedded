#ifndef GAMEBOY_H
#define GAMEBOY_H

#include "stm32f411.h"
#include "ili9341.h"
#include "trace_dev.h"
#include "chardev.h"
#include "max98357a.h"

struct gameboy {
    struct stm32f411  soc;
    struct ili9341   *display;
    struct trace_dev  trace;
    struct max98357a  audio;
    struct chardev   *io_chardev;
    struct chardev_table *chardevs;
    uint64_t          next_io_poll;
};

void gameboy_init(struct gameboy *b, struct chardev_table *chardevs);
int  gameboy_tick(struct gameboy *b);

struct machine_desc;
extern const struct machine_desc gameboy_machine;

#endif
