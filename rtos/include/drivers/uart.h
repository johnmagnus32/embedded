/*
 * uart.h — UART driver API (Zephyr-style)
 */

#ifndef DRIVERS_UART_H
#define DRIVERS_UART_H

#include "device.h"

struct uart_driver_api {
    void (*poll_out)(const struct device *dev, char c);
    int  (*poll_in)(const struct device *dev, char *c);
};

static inline void uart_poll_out(const struct device *dev, char c)
{
    const struct uart_driver_api *api = dev->api;
    api->poll_out(dev, c);
}

/* Returns 0 if char received, -1 if nothing available */
static inline int uart_poll_in(const struct device *dev, char *c)
{
    const struct uart_driver_api *api = dev->api;
    return api->poll_in(dev, c);
}

static inline void uart_puts(const struct device *dev, const char *s)
{
    while (*s) {
        if (*s == '\n')
            uart_poll_out(dev, '\r');
        uart_poll_out(dev, *s++);
    }
}

#endif
