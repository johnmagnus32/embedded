/*
 * main.c — Press a button, light the corresponding LED
 */

#include "device.h"
#include "drivers/uart.h"
#include "drivers/gpio_keys.h"
#include "drivers/gpio_leds.h"

DEVICE_DT_DECLARE(usart2);
DEVICE_DT_DECLARE(buttons);
DEVICE_DT_DECLARE(leds);

/* Button keycodes mapped to LED indices (same order as DT children) */
static const int key_to_led[] = {
    [0] = KEY_UP,     /* led-a    → maps to button-up    (sorted: led-a=0)  */
    [1] = KEY_DOWN,
    [2] = KEY_LEFT,
    [3] = KEY_RIGHT,
    [4] = KEY_A,
    [5] = KEY_B,
};

int main(void)
{
    const struct device *console = DEVICE_DT_GET(usart2);
    const struct device *keys = DEVICE_DT_GET(buttons);
    const struct device *led_dev = DEVICE_DT_GET(leds);

    console->init(console);
    keys->init(keys);
    led_dev->init(led_dev);

    uart_puts(console, "Hello from bare metal STM32F411RE!\n");
    uart_puts(console, "Press buttons → LEDs light up.\n\n");

    while (1) {
        for (int i = 0; i < 6; i++) {
            int pressed = gpio_keys_is_pressed(keys, key_to_led[i]);
            gpio_leds_set(led_dev, i, pressed);
        }
    }
}
