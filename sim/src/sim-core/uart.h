#ifndef UART_H
#define UART_H

#include <stdint.h>

struct chardev;

struct uart {
    uint32_t base;
    struct chardev *chardev;  /* output channel, NULL if none */
};

void     uart_init(struct uart *u, uint32_t base, struct chardev *cd);
int      uart_handles(struct uart *u, uint32_t addr);
uint32_t uart_read(struct uart *u, uint32_t addr);
void     uart_write(struct uart *u, uint32_t addr, uint32_t val);

#endif
