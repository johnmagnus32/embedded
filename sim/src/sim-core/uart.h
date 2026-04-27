#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stdio.h>

#define UART_BUF_SIZE 8192

struct uart {
    uint32_t base;
    char buf[UART_BUF_SIZE];
    int head;
    int count;
};

void     uart_init(struct uart *u, uint32_t base);
void     uart_set_state_dir(const char *dir);
int      uart_handles(struct uart *u, uint32_t addr);
uint32_t uart_read(struct uart *u, uint32_t addr);
void     uart_write(struct uart *u, uint32_t addr, uint32_t val);
void     uart_dump_state(struct uart *u, FILE *f);

#endif
