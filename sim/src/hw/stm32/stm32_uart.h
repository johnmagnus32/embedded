#ifndef STM32_UART_H
#define STM32_UART_H

#include <stdint.h>

struct chardev;

struct stm32_uart {
    struct chardev *chardev;
    uint8_t tx_buf[128];
    int tx_len;
};

void     stm32_uart_init(struct stm32_uart *u, struct chardev *cd);
uint32_t stm32_uart_read(void *opaque, uint32_t offset);
void     stm32_uart_write(void *opaque, uint32_t offset, uint32_t val);

#endif
