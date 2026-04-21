#ifndef DRIVERS_GPIO_LEDS_H
#define DRIVERS_GPIO_LEDS_H

#include "device.h"

struct gpio_leds_api {
    void (*set)(const struct device *dev, int index, int on);
};

static inline void gpio_leds_set(const struct device *dev, int index, int on)
{
    const struct gpio_leds_api *api = dev->api;
    api->set(dev, index, on);
}

#endif
