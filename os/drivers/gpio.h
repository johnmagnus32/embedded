#ifndef DRIVERS_GPIO_H
#define DRIVERS_GPIO_H

#include "device.h"
#include <stdint.h>

/* GPIO driver API — like Zephyr's struct gpio_driver_api */
struct gpio_driver_api {
    int (*pin_configure)(const struct device *dev, uint8_t pin, uint8_t flags);
    int (*pin_get)(const struct device *dev, uint8_t pin);
    void (*pin_set)(const struct device *dev, uint8_t pin, int value);
};

/* Pin configuration flags */
#define GPIO_INPUT       0x01
#define GPIO_OUTPUT      0x02
#define GPIO_PULL_UP     0x04

/* Convenience wrappers */
static inline int gpio_pin_configure(const struct device *dev, uint8_t pin, uint8_t flags)
{
    const struct gpio_driver_api *api = dev->api;
    return api->pin_configure(dev, pin, flags);
}

static inline int gpio_pin_get(const struct device *dev, uint8_t pin)
{
    const struct gpio_driver_api *api = dev->api;
    return api->pin_get(dev, pin);
}

static inline void gpio_pin_set(const struct device *dev, uint8_t pin, int value)
{
    const struct gpio_driver_api *api = dev->api;
    api->pin_set(dev, pin, value);
}

#endif
