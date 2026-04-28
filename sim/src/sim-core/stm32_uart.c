/*
 * stm32_uart.c — STM32 USART device
 */
#include "stm32_uart.h"
#include "chardev.h"

void stm32_uart_init(struct stm32_uart *u, struct chardev *cd)
{
    u->chardev = cd;
}

uint32_t stm32_uart_read(void *opaque, uint32_t offset)
{
    (void)opaque;
    if (offset == 0x00) return (1 << 7) | (1 << 6);  /* TXE+TC */
    return 0;
}

void stm32_uart_write(void *opaque, uint32_t offset, uint32_t val)
{
    struct stm32_uart *u = (struct stm32_uart *)opaque;
    if (offset != 0x04) return;
    chardev_write(u->chardev, (uint8_t)(val & 0xFF));
}
