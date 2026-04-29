#ifndef MACHINE_H
#define MACHINE_H

#include <stddef.h>
#include <stdint.h>

struct chardev_table;
struct cpu_state;
struct membus;

struct machine_desc {
    const char *name;
    const char *description;
    size_t      board_size;
    void (*init)(void *board, struct chardev_table *chardevs);
    void (*tick)(void *board);
    struct cpu_state     *(*get_cpu)(void *board);
    struct membus        *(*get_bus)(void *board);
    uint8_t             **(*get_flash)(void *board);
    uint8_t             **(*get_ram)(void *board);
};

const struct machine_desc *machine_find(const char *name);

#endif
