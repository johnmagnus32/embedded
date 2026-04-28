#ifndef MACHINE_H
#define MACHINE_H

#include <stddef.h>
#include <stdint.h>

struct chardev_table;
struct cpu_state;
struct membus;
struct armv7m_nvic;
struct stm32_gpio;
struct ili9341;

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
    struct armv7m_nvic   *(*get_nvic)(void *board);
    struct stm32_gpio    *(*get_gpio)(void *board, int port);
    struct ili9341       *(*get_display)(void *board);
};

const struct machine_desc *machine_find(const char *name);

#endif
