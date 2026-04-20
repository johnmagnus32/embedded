/*
 * uart.h — UART driver API (Zephyr-style)
 *
 * Application code calls these through the struct device,
 * never touching hardware directly.
 */

#ifndef DRIVERS_UART_H
#define DRIVERS_UART_H

#include "device.h"

/* Driver API — like Zephyr's struct uart_driver_api */
struct uart_driver_api {
    void (*poll_out)(const struct device *dev, char c);
};

/* Convenience wrappers */
static inline void uart_poll_out(const struct device *dev, char c)
{
    const struct uart_driver_api *api = dev->api;
    api->poll_out(dev, c);
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
