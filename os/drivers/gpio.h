#ifndef DRIVERS_GPIO_H
#define DRIVERS_GPIO_H

#include "device.h"
#include <stdint.h>

typedef void (*gpio_callback_t)(uint8_t pin);

/* Interrupt trigger flags */
#define GPIO_INT_DISABLE      0x00
#define GPIO_INT_EDGE_RISING  0x01
#define GPIO_INT_EDGE_FALLING 0x02
#define GPIO_INT_EDGE_BOTH    0x03

/* GPIO driver API — like Zephyr's struct gpio_driver_api */
struct gpio_driver_api {
    int (*pin_configure)(const struct device *dev, uint8_t pin, uint8_t flags);
    int (*pin_get)(const struct device *dev, uint8_t pin);
    void (*pin_set)(const struct device *dev, uint8_t pin, int value);
    int (*pin_interrupt_configure)(const struct device *dev, uint8_t pin, uint8_t flags);
    int (*pin_register_callback)(const struct device *dev, uint8_t pin, gpio_callback_t cb);
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

static inline int gpio_pin_interrupt_configure(const struct device *dev, uint8_t pin, uint8_t flags)
{
    const struct gpio_driver_api *api = dev->api;
    if (!api->pin_interrupt_configure) return -1;
    return api->pin_interrupt_configure(dev, pin, flags);
}

static inline int gpio_pin_register_callback(const struct device *dev, uint8_t pin, gpio_callback_t cb)
{
    const struct gpio_driver_api *api = dev->api;
    if (!api->pin_register_callback) return -1;
    return api->pin_register_callback(dev, pin, cb);
}

#endif
