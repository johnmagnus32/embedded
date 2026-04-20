/*
 * gpio.c — STM32 GPIO driver
 *
 * Supports input with pull-up and reading pin state.
 * Config comes from device tree via DT_INST macros.
 */

#include <stdint.h>
#include "devicetree.h"
#include "device.h"
#include "drivers/gpio.h"

/* STM32 GPIO register offsets */
#define GPIO_MODER   0x00
#define GPIO_PUPDR   0x0C
#define GPIO_IDR     0x10

#define RCC_BASE     0x40023800
#define RCC_AHB1ENR  (*(volatile uint32_t *)(RCC_BASE + 0x30))

#define REG(base, off) (*(volatile uint32_t *)((base) + (off)))

struct gpio_stm32_config {
    uint32_t base;
    uint8_t  clk_bit;
};

static int gpio_stm32_pin_configure(const struct device *dev, uint8_t pin, uint8_t flags)
{
    const struct gpio_stm32_config *cfg = dev->config;

    /* Enable port clock */
    RCC_AHB1ENR |= (1 << cfg->clk_bit);

    if (flags & GPIO_INPUT) {
        /* Set pin to input mode (MODER = 00) */
        REG(cfg->base, GPIO_MODER) &= ~(3U << (pin * 2));

        /* Configure pull-up if requested */
        if (flags & GPIO_PULL_UP) {
            REG(cfg->base, GPIO_PUPDR) &= ~(3U << (pin * 2));
            REG(cfg->base, GPIO_PUPDR) |=  (1U << (pin * 2));  /* 01 = pull-up */
        }
    }
    return 0;
}

static int gpio_stm32_pin_get(const struct device *dev, uint8_t pin)
{
    const struct gpio_stm32_config *cfg = dev->config;
    return (REG(cfg->base, GPIO_IDR) >> pin) & 1;
}

static const struct gpio_driver_api gpio_stm32_api = {
    .pin_configure = gpio_stm32_pin_configure,
    .pin_get = gpio_stm32_pin_get,
};

/* ---- DT_INST instantiation ---- */

#define CONCAT3(a, b, c) a##b##c
#define _GPIO_INST_REG(n) DT_INST_ST_STM32_GPIO_##n##_REG_ADDR
#define _GPIO_INST_CLK(n) DT_INST_ST_STM32_GPIO_##n##_CLK_BIT
#define _GPIO_INST_LABEL(n) DT_INST_ST_STM32_GPIO_##n##_LABEL

#define STM32_GPIO_DEFINE(n)                                        \
    static const struct gpio_stm32_config gpio_cfg_##n = {          \
        .base    = _GPIO_INST_REG(n),                               \
        .clk_bit = _GPIO_INST_CLK(n),                               \
    };                                                              \
    DEVICE_DT_DEFINE(_GPIO_INST_LABEL(n),                           \
                     NULL, NULL, &gpio_cfg_##n, &gpio_stm32_api);

DT_INST_FOREACH_STATUS_OKAY(ST_STM32_GPIO, STM32_GPIO_DEFINE)
