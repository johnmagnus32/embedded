/*
 * main.c — Application code
 *
 * No register addresses. No config structs. Just:
 *   1. Get the device by its DT label
 *   2. Call the driver API
 *
 * Compare to a Zephyr app:
 *   const struct device *uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
 *   uart_poll_out(uart, 'H');
 */

#include "device.h"
#include "drivers/uart.h"

/* Declare the device created by the driver (in uart.c) */
DEVICE_DT_DECLARE(usart2);

int main(void)
{
    /* Get the console UART — resolved at compile time, no lookup */
    const struct device *console = DEVICE_DT_GET(usart2);

    /* Init the device (in Zephyr this happens automatically at boot) */
    console->init(console);

    uart_puts(console, "Hello from bare metal STM32F411RE!\n");
    uart_puts(console, "Device tree + driver model + auto config.\n");

    while (1) {
        uart_puts(console, "tick\n");
        for (volatile int i = 0; i < 1600000; i++)
            ;
    }
}
