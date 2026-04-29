/*
 * clock.c — STM32 RCC clock control driver
 *
 * Base address comes from device tree (rcc@40023800).
 * Other drivers call clock_on(rcc_dev, bus, bit) to enable their clock.
 *
 * Maps to Zephyr's drivers/clock_control/clock_stm32_ll_common.c
 */

#include <stdint.h>
#include "devicetree.h"
#include "device.h"
#include "drivers/clock.h"

#define REG(base, off) (*(volatile uint32_t *)((base) + (off)))

/* RCC enable register offsets (STM32F4) */
#define RCC_AHB1ENR_OFF  0x30
#define RCC_APB1ENR_OFF  0x40
#define RCC_APB2ENR_OFF  0x44

struct clock_stm32_config {
    uint32_t base;
};

static const uint8_t bus_offsets[] = {
    [0] = RCC_AHB1ENR_OFF,
    [1] = RCC_APB1ENR_OFF,
    [2] = RCC_APB2ENR_OFF,
};

static void clock_stm32_on(const struct device *dev, uint8_t bus, uint8_t bit)
{
    const struct clock_stm32_config *cfg = dev->config;
    if (bus > 2) return;
    REG(cfg->base, bus_offsets[bus]) |= (1U << bit);
}

static const struct clock_driver_api clock_stm32_api = {
    .on = clock_stm32_on,
};

/* ---- DT_INST instantiation ---- */

#define _CLK_INST_LABEL(n) DT_INST_ST_STM32_RCC_##n##_LABEL

#define STM32_RCC_DEFINE(n)                                         \
    static const struct clock_stm32_config clock_cfg_##n = {        \
        .base = DT_INST_ST_STM32_RCC_##n##_REG_ADDR,               \
    };                                                              \
    DEVICE_DT_DEFINE(_CLK_INST_LABEL(n),                            \
                     NULL, NULL, &clock_cfg_##n, &clock_stm32_api);

DT_INST_FOREACH_STATUS_OKAY(ST_STM32_RCC, STM32_RCC_DEFINE)
