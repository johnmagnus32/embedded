/*
 * uart.c — USART device
 *
 * TX bytes are sent to the attached chardev channel.
 */
#include "uart.h"
#include "chardev.h"
#include "cpu.h"

void uart_init(struct uart *u, uint32_t base, struct chardev *cd)
{
    u->base = base;
    u->chardev = cd;
}

int uart_handles(struct uart *u, uint32_t addr)
{
    return addr >= u->base && addr < u->base + 0x20;
}

uint32_t uart_read(struct uart *u, uint32_t addr)
{
    if (addr == u->base + 0x00) return (1 << 7) | (1 << 6);  /* TXE+TC */
    return 0;
}

void uart_write(struct uart *u, uint32_t addr, uint32_t val)
{
    if (addr != u->base + 0x04) return;
    chardev_write(u->chardev, (uint8_t)(val & 0xFF));
}
