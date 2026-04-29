/*
 * adc.h — ADC driver API
 */

#ifndef DRIVERS_ADC_H
#define DRIVERS_ADC_H

#include "device.h"
#include <stdint.h>

struct adc_driver_api {
    uint32_t (*read)(const struct device *dev);
};

static inline uint32_t adc_read(const struct device *dev)
{
    const struct adc_driver_api *api = dev->api;
    return api->read(dev);
}

#endif
