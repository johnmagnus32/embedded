/*
 * gpio_stm32.c — STM32 GPIO driver with EXTI interrupt support
 *
 * Uses the clock driver to enable port clocks.
 * Owns EXTI configuration for pins 0-15.
 */

#include <stdint.h>
#include "devicetree.h"
#include "device.h"
#include "drivers/gpio.h"
#include "drivers/clock.h"

#define GPIO_MODER   0x00
#define GPIO_PUPDR   0x0C
#define GPIO_IDR     0x10
#define GPIO_BSRR    0x18

/* EXTI registers */
#define EXTI_BASE    0x40013C00
#define SYSCFG_BASE  0x40013800
#define SYSCFG_EXTICR1 0x08  /* EXTICR1-4 at offsets 0x08, 0x0C, 0x10, 0x14 */
#define EXTI_IMR     0x00
#define EXTI_RTSR    0x08
#define EXTI_FTSR    0x0C
#define EXTI_PR      0x14

#define NVIC_ISER0   (*(volatile uint32_t *)0xE000E100)

#define REG(base, off) (*(volatile uint32_t *)((base) + (off)))

struct gpio_stm32_config {
    uint32_t base;
    uint8_t  clk_bus;
    uint8_t  clk_bit;
};

DEVICE_DT_DECLARE(rcc);

/* Per-pin EXTI callback and device tracking */
static gpio_callback_t exti_callbacks[16];
static const struct device *exti_devices[16];

/* Map EXTI line 0-4 to NVIC IRQ number */
static const uint8_t exti_irq[] = { 6, 7, 8, 9, 10 };

static int gpio_stm32_pin_configure(const struct device *dev, uint8_t pin, uint8_t flags)
{
    const struct gpio_stm32_config *cfg = dev->config;

    clock_on(DEVICE_DT_GET(rcc), cfg->clk_bus, cfg->clk_bit);

    REG(cfg->base, GPIO_MODER) &= ~(3U << (pin * 2));

    if (flags & GPIO_OUTPUT)
        REG(cfg->base, GPIO_MODER) |= (1U << (pin * 2));

    if (flags & GPIO_PULL_UP) {
        REG(cfg->base, GPIO_PUPDR) &= ~(3U << (pin * 2));
        REG(cfg->base, GPIO_PUPDR) |=  (1U << (pin * 2));
    } else if (flags & GPIO_PULL_DOWN) {
        REG(cfg->base, GPIO_PUPDR) &= ~(3U << (pin * 2));
        REG(cfg->base, GPIO_PUPDR) |=  (2U << (pin * 2));
    }

    return 0;
}

static int gpio_stm32_pin_get(const struct device *dev, uint8_t pin)
{
    const struct gpio_stm32_config *cfg = dev->config;
    return (REG(cfg->base, GPIO_IDR) >> pin) & 1;
}

static void gpio_stm32_pin_set(const struct device *dev, uint8_t pin, int value)
{
    const struct gpio_stm32_config *cfg = dev->config;
    if (value)
        REG(cfg->base, GPIO_BSRR) = (1 << pin);
    else
        REG(cfg->base, GPIO_BSRR) = (1 << (pin + 16));
}

static int gpio_stm32_pin_interrupt_configure(const struct device *dev, uint8_t pin, uint8_t flags)
{
    if (pin > 15) return -1;
    const struct gpio_stm32_config *cfg = dev->config;

    uint32_t mask = (1U << pin);

    if (flags == GPIO_INT_DISABLE) {
        REG(EXTI_BASE, EXTI_IMR) &= ~mask;
        return 0;
    }

    /* Select this GPIO port for the EXTI line via SYSCFG_EXTICR.
     * Port number derived from base address: GPIOA=0, GPIOB=1, etc. */
    /* SYSCFG clock must be enabled (APB2 bit 14) */
    clock_on(DEVICE_DT_GET(rcc), 2, 14);
    uint8_t port = (cfg->base - 0x40020000) / 0x400;
    int reg_idx = pin / 4;
    int shift = (pin % 4) * 4;
    uint32_t exticr = REG(SYSCFG_BASE, SYSCFG_EXTICR1 + reg_idx * 4);
    exticr &= ~(0xF << shift);
    exticr |= (port << shift);
    REG(SYSCFG_BASE, SYSCFG_EXTICR1 + reg_idx * 4) = exticr;

    if (flags & GPIO_INT_EDGE_RISING)
        REG(EXTI_BASE, EXTI_RTSR) |= mask;
    if (flags & GPIO_INT_EDGE_FALLING)
        REG(EXTI_BASE, EXTI_FTSR) |= mask;

    REG(EXTI_BASE, EXTI_IMR) |= mask;

    exti_devices[pin] = dev;

    /* Enable NVIC for EXTI lines */
    if (pin < 5)
        NVIC_ISER0 = (1U << exti_irq[pin]);  /* EXTI0-4: dedicated IRQs 6-10 */
    else if (pin <= 9)
        NVIC_ISER0 = (1U << 23);  /* EXTI9_5_IRQn = 23 */
    else
        *(volatile uint32_t *)0xE000E104 = (1U << (40 - 32));  /* EXTI15_10_IRQn = 40, in ISER1 */

    return 0;
}

static int gpio_stm32_pin_register_callback(const struct device *dev, uint8_t pin, gpio_callback_t cb)
{
    if (pin > 15) return -1;
    exti_callbacks[pin] = cb;
    return 0;
}

/* EXTI ISR common handler */
static void exti_common_handler(uint8_t pin)
{
    REG(EXTI_BASE, EXTI_PR) = (1U << pin);
    if (exti_callbacks[pin])
        exti_callbacks[pin](pin);
}

void exti0_handler(void *arg) { (void)arg; exti_common_handler(0); }
void exti1_handler(void *arg) { (void)arg; exti_common_handler(1); }
void exti2_handler(void *arg) { (void)arg; exti_common_handler(2); }
void exti3_handler(void *arg) { (void)arg; exti_common_handler(3); }
void exti4_handler(void *arg) { (void)arg; exti_common_handler(4); }

void exti9_5_handler(void *arg)
{ (void)arg;
    for (uint8_t pin = 5; pin <= 9; pin++) {
        if (REG(EXTI_BASE, EXTI_PR) & (1U << pin)) {
            exti_common_handler(pin);
        }
    }
}

void exti15_10_handler(void *arg)
{ (void)arg;
    for (uint8_t pin = 10; pin <= 15; pin++) {
        if (REG(EXTI_BASE, EXTI_PR) & (1U << pin)) {
            exti_common_handler(pin);
        }
    }
}

static const struct gpio_driver_api gpio_stm32_api = {
    .pin_configure           = gpio_stm32_pin_configure,
    .pin_get                 = gpio_stm32_pin_get,
    .pin_set                 = gpio_stm32_pin_set,
    .pin_interrupt_configure = gpio_stm32_pin_interrupt_configure,
    .pin_register_callback   = gpio_stm32_pin_register_callback,
};

#define _GPIO_INST_LABEL(n) DT_INST_ST_STM32_GPIO_##n##_LABEL

#define STM32_GPIO_DEFINE(n)                                        \
    static const struct gpio_stm32_config gpio_cfg_##n = {          \
        .base    = DT_INST_ST_STM32_GPIO_##n##_REG_ADDR,           \
        .clk_bus = DT_INST_ST_STM32_GPIO_##n##_CLK_BUS,            \
        .clk_bit = DT_INST_ST_STM32_GPIO_##n##_CLK_BIT,            \
    };                                                              \
    DEVICE_DT_DEFINE(_GPIO_INST_LABEL(n),                           \
                     NULL, NULL, &gpio_cfg_##n, &gpio_stm32_api, 20);

DT_INST_FOREACH_STATUS_OKAY(ST_STM32_GPIO, STM32_GPIO_DEFINE)
