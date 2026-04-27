#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stdio.h>

struct uart {
    uint32_t base;
    int client_fd;  /* TCP socket to stream TX bytes to, -1 if none */
};

void     uart_init(struct uart *u, uint32_t base);
int      uart_listen(struct uart *u, int port);  /* start listening, return port */
void     uart_accept(struct uart *u);             /* accept a client */
int      uart_handles(struct uart *u, uint32_t addr);
uint32_t uart_read(struct uart *u, uint32_t addr);
void     uart_write(struct uart *u, uint32_t addr, uint32_t val);

#endif
