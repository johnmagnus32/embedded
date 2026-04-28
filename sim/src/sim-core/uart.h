#ifndef UART_H
#define UART_H

#include <stdint.h>

struct chardev;

struct uart {
    struct chardev *chardev;
};

void     uart_init(struct uart *u, struct chardev *cd);
uint32_t uart_read(void *opaque, uint32_t offset);
void     uart_write(void *opaque, uint32_t offset, uint32_t val);

#endif
