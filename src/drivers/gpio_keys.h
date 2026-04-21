#ifndef DRIVERS_GPIO_KEYS_H
#define DRIVERS_GPIO_KEYS_H

#include "device.h"
#include <stdbool.h>

enum gpio_key_code {
    KEY_UP    = 103,
    KEY_DOWN  = 108,
    KEY_LEFT  = 105,
    KEY_RIGHT = 106,
    KEY_A     = 30,
    KEY_B     = 48,
};

struct gpio_keys_api {
    bool (*is_pressed)(const struct device *dev, int code);
};

static inline bool gpio_keys_is_pressed(const struct device *dev, int code)
{
    const struct gpio_keys_api *api = dev->api;
    return api->is_pressed(dev, code);
}

#endif
