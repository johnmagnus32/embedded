/*
 * uart.c — USART device
 */
#include "uart.h"
#include "chardev.h"

void uart_init(struct uart *u, struct chardev *cd)
{
    u->chardev = cd;
}

uint32_t uart_read(void *opaque, uint32_t offset)
{
    (void)opaque;
    if (offset == 0x00) return (1 << 7) | (1 << 6);  /* TXE+TC */
    return 0;
}

void uart_write(void *opaque, uint32_t offset, uint32_t val)
{
    struct uart *u = (struct uart *)opaque;
    if (offset != 0x04) return;
    chardev_write(u->chardev, (uint8_t)(val & 0xFF));
}
