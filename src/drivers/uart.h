/*
 * uart.h — UART driver API
 *
 * Like Zephyr's struct uart_driver_api, the driver exposes a clean
 * interface. The caller doesn't need to know register addresses.
 */

#ifndef DRIVERS_UART_H
#define DRIVERS_UART_H

#include <stddef.h>

/* Configuration from device tree (passed to uart_init) */
struct uart_config {
    unsigned long base;       /* USART peripheral base address */
    unsigned long baudrate;   /* desired baud rate */
    unsigned long clk_hz;     /* input clock frequency */
    unsigned long gpio_base;  /* TX pin GPIO port base */
    unsigned int  tx_pin;     /* TX pin number */
    unsigned int  tx_af;      /* TX pin alternate function */
    unsigned int  gpio_clk_bit; /* GPIO port clock enable bit (AHB1ENR) */
    unsigned int  uart_clk_bit; /* UART clock enable bit (APB1ENR) */
};

void uart_init(const struct uart_config *cfg);
void uart_putc(const struct uart_config *cfg, char c);
void uart_puts(const struct uart_config *cfg, const char *s);

#endif
