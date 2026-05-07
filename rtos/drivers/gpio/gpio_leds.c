/*
 * gpio_leds.c — gpio-leds driver
 *
 * Each child node defines an LED on a GPIO pin.
 * Uses the GPIO driver API — doesn't touch registers directly.
 */

#include <stdint.h>
#include "devicetree.h"
#include "device.h"
#include "drivers/gpio.h"
#include "drivers/gpio_leds.h"

#define CHILD_PIN(n) DT_INST_GPIO_LEDS_0_CHILD_##n##_PIN

static const uint8_t led_pins[] = {
    CHILD_PIN(0), CHILD_PIN(1), CHILD_PIN(2),
    CHILD_PIN(3), CHILD_PIN(4), CHILD_PIN(5),
};
#define NUM_LEDS (sizeof(led_pins) / sizeof(led_pins[0]))

DEVICE_DT_DECLARE(gpiob);

static int gpio_leds_init_fn(const struct device *dev)
{
    (void)dev;
    const struct device *gpio = DEVICE_DT_GET(gpiob);

    for (int i = 0; i < (int)NUM_LEDS; i++) {
        gpio_pin_configure(gpio, led_pins[i], GPIO_OUTPUT);
    }
    return 0;
}

static void gpio_leds_set_fn(const struct device *dev, int index, int on)
{
    (void)dev;
    if (index < 0 || index >= (int)NUM_LEDS) return;

    const struct device *gpio = DEVICE_DT_GET(gpiob);
    gpio_pin_set(gpio, led_pins[index], on);
}

static const struct gpio_leds_api gpio_leds_api = {
    .set = gpio_leds_set_fn,
};

#define _LEDS_INST_LABEL(n) DT_INST_GPIO_LEDS_##n##_LABEL

#define GPIO_LEDS_DEFINE(n) \
    DEVICE_DT_DEFINE(_LEDS_INST_LABEL(n), \
                     gpio_leds_init_fn, NULL, NULL, &gpio_leds_api, 60);

DT_INST_FOREACH_STATUS_OKAY(GPIO_LEDS, GPIO_LEDS_DEFINE)
