/*
 * clock.h — Clock control driver API
 *
 * Like Zephyr's clock_control API. Other drivers call clock_on()
 * to enable their peripheral clock instead of writing RCC directly.
 *
 * The bus/bit values come from each device's "clocks" DT property:
 *   clocks = <&rcc bus bit>;
 *   bus: 0=AHB1, 1=APB1, 2=APB2
 *   bit: which bit in the enable register
 */

#ifndef DRIVERS_CLOCK_H
#define DRIVERS_CLOCK_H

#include "device.h"
#include <stdint.h>

struct clock_driver_api {
    void (*on)(const struct device *dev, uint8_t bus, uint8_t bit);
};

static inline void clock_on(const struct device *dev, uint8_t bus, uint8_t bit)
{
    const struct clock_driver_api *api = dev->api;
    api->on(dev, bus, bit);
}

#endif
