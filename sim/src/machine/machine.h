#ifndef MACHINE_H
#define MACHINE_H

#include <stddef.h>
#include <stdint.h>

struct chardev_table;
struct armv7m_cpu;
struct membus;
struct event_queue;

struct machine_desc {
    const char *name;
    const char *description;
    size_t      board_size;
    void (*init)(void *board, struct chardev_table *chardevs);
    int  (*tick)(void *board);
    struct armv7m_cpu     *(*get_cpu)(void *board);
    struct membus        *(*get_bus)(void *board);
    uint8_t             **(*get_flash)(void *board);
    uint8_t             **(*get_ram)(void *board);
    struct event_queue   *(*get_eq)(void *board);
    uint32_t              (*get_sysclk)(void *board);
};

const struct machine_desc *machine_find(const char *name);

#endif
