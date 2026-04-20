/*
 * main.c — Application: print button state over UART
 */

#include "device.h"
#include "drivers/uart.h"
#include "drivers/buttons.h"

DEVICE_DT_DECLARE(usart2);
DEVICE_DT_DECLARE(buttons);

static const char *btn_names[] = {"UP", "DOWN", "LEFT", "RIGHT", "A", "B"};

int main(void)
{
    const struct device *console = DEVICE_DT_GET(usart2);
    const struct device *btns = DEVICE_DT_GET(buttons);

    console->init(console);
    btns->init(btns);

    uart_puts(console, "Hello from bare metal STM32F411RE!\n");
    uart_puts(console, "Press buttons on GPIOC pins 0-5.\n\n");

    while (1) {
        for (int i = 0; i < BTN_COUNT; i++) {
            if (button_is_pressed(btns, i)) {
                uart_puts(console, btn_names[i]);
                uart_puts(console, " ");
            }
        }
        uart_puts(console, "\r");

        for (volatile int i = 0; i < 400000; i++)
            ;
    }
}
