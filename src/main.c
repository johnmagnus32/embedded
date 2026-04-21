/*
 * main.c — Application: print button state over UART
 */

#include "device.h"
#include "drivers/uart.h"
#include "drivers/gpio_keys.h"

DEVICE_DT_DECLARE(usart2);
DEVICE_DT_DECLARE(buttons);

static const struct { int code; const char *name; } key_map[] = {
    { KEY_UP,    "UP "    },
    { KEY_DOWN,  "DOWN "  },
    { KEY_LEFT,  "LEFT "  },
    { KEY_RIGHT, "RIGHT " },
    { KEY_A,     "A "     },
    { KEY_B,     "B "     },
};

int main(void)
{
    const struct device *console = DEVICE_DT_GET(usart2);
    const struct device *keys = DEVICE_DT_GET(buttons);

    console->init(console);
    keys->init(keys);

    uart_puts(console, "Hello from bare metal STM32F411RE!\n");
    uart_puts(console, "Press buttons on GPIOC pins 0-5.\n\n");

    while (1) {
        for (int i = 0; i < 6; i++) {
            if (gpio_keys_is_pressed(keys, key_map[i].code)) {
                uart_puts(console, key_map[i].name);
            }
        }
        uart_puts(console, "\r");

        for (volatile int i = 0; i < 400000; i++)
            ;
    }
}
