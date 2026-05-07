/*
 * gpio_keys.c — gpio-keys driver
 *
 * Each child node in the device tree defines a key with a GPIO pin
 * and a keycode. The API takes keycodes directly (like Linux input).
 */

#include <stdint.h>
#include <stdbool.h>
#include "devicetree.h"
#include "device.h"
#include "drivers/gpio.h"
#include "drivers/gpio_keys.h"

struct key_def {
    uint8_t pin;
    uint8_t code;
};

#define CHILD(n) { \
    .pin = DT_INST_GPIO_KEYS_0_CHILD_##n##_PIN, \
    .code = DT_INST_GPIO_KEYS_0_CHILD_##n##_CODE, \
}

static const struct key_def keys[] = {
    CHILD(0), CHILD(1), CHILD(2), CHILD(3), CHILD(4), CHILD(5),
};
#define NUM_KEYS (sizeof(keys) / sizeof(keys[0]))

DEVICE_DT_DECLARE(gpioc);

static int gpio_keys_init(const struct device *dev)
{
    (void)dev;
    const struct device *gpio = DEVICE_DT_GET(gpioc);

    for (int i = 0; i < (int)NUM_KEYS; i++) {
        gpio_pin_configure(gpio, keys[i].pin, GPIO_INPUT | GPIO_PULL_UP);
    }
    return 0;
}

static bool gpio_keys_is_pressed_impl(const struct device *dev, int code)
{
    (void)dev;
    const struct device *gpio = DEVICE_DT_GET(gpioc);

    for (int i = 0; i < (int)NUM_KEYS; i++) {
        if (keys[i].code == code) {
            return gpio_pin_get(gpio, keys[i].pin) == 0;
        }
    }
    return false;
}

static const struct gpio_keys_api gpio_keys_api = {
    .is_pressed = gpio_keys_is_pressed_impl,
};

#define _BTN_INST_LABEL(n) DT_INST_GPIO_KEYS_##n##_LABEL

#define GPIO_KEYS_DEFINE(n) \
    DEVICE_DT_DEFINE(_BTN_INST_LABEL(n), \
                     gpio_keys_init, NULL, NULL, &gpio_keys_api, 60);

DT_INST_FOREACH_STATUS_OKAY(GPIO_KEYS, GPIO_KEYS_DEFINE)
