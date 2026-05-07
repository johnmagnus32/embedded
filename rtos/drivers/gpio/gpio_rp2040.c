/*
 * gpio_rp2040.c — RP2040 GPIO driver
 *
 * RP2040 GPIO is split across three register blocks:
 *   IO_BANK0 (0x40014000) — function select per pin
 *   PADS_BANK0 (0x4001c000) — pull-up/down, drive strength
 *   SIO (0xd0000000) — fast GPIO set/clear/read
 *
 * Completely different from STM32's MODER/BSRR/IDR.
 * Same API (gpio_driver_api).
 */

#include <stdint.h>
#include "config.h"

#ifdef CONFIG_GPIO_RP2040

#include "devicetree.h"
#include "device.h"
#include "drivers/gpio.h"

#define REG(base, off) (*(volatile uint32_t *)((base) + (off)))

struct gpio_rp2040_config {
    uint32_t io_base;    /* IO_BANK0 */
    uint32_t sio_base;   /* SIO for fast GPIO */
    uint32_t pads_base;  /* PADS_BANK0 */
};

/* SIO GPIO register offsets */
#define SIO_GPIO_IN      0x004
#define SIO_GPIO_OUT_SET 0x014
#define SIO_GPIO_OUT_CLR 0x018
#define SIO_GPIO_OE_SET  0x024
#define SIO_GPIO_OE_CLR  0x028

/* IO_BANK0: each pin has STATUS(+0) and CTRL(+4), 8 bytes apart */
#define GPIO_CTRL(base, pin) REG(base, 0x04 + (pin) * 8)

/* PADS_BANK0: each pin at offset 0x04 + pin*4 */
#define PAD_CTRL(base, pin) REG(base, 0x04 + (pin) * 4)

static int gpio_rp2040_pin_configure(const struct device *dev, uint8_t pin, uint8_t flags)
{
    const struct gpio_rp2040_config *cfg = dev->config;

    /* Set function to SIO (funcsel = 5) for GPIO use */
    GPIO_CTRL(cfg->io_base, pin) = 5;

    /* Configure pad: input enable, pull-up if requested */
    uint32_t pad = (1 << 6);  /* IE = input enable */
    if (flags & GPIO_PULL_UP)
        pad |= (1 << 3);     /* PUE = pull-up enable */
    PAD_CTRL(cfg->pads_base, pin) = pad;

    /* Set direction */
    if (flags & GPIO_OUTPUT)
        REG(cfg->sio_base, SIO_GPIO_OE_SET) = (1 << pin);
    else
        REG(cfg->sio_base, SIO_GPIO_OE_CLR) = (1 << pin);

    return 0;
}

static int gpio_rp2040_pin_get(const struct device *dev, uint8_t pin)
{
    const struct gpio_rp2040_config *cfg = dev->config;
    return (REG(cfg->sio_base, SIO_GPIO_IN) >> pin) & 1;
}

static void gpio_rp2040_pin_set(const struct device *dev, uint8_t pin, int value)
{
    const struct gpio_rp2040_config *cfg = dev->config;
    if (value)
        REG(cfg->sio_base, SIO_GPIO_OUT_SET) = (1 << pin);
    else
        REG(cfg->sio_base, SIO_GPIO_OUT_CLR) = (1 << pin);
}

static const struct gpio_driver_api gpio_rp2040_api = {
    .pin_configure = gpio_rp2040_pin_configure,
    .pin_get = gpio_rp2040_pin_get,
    .pin_set = gpio_rp2040_pin_set,
};

/* ---- DT_INST instantiation ---- */

#define _RP2040_GPIO_LABEL(n) DT_INST_RASPBERRYPI_RP2040_GPIO_##n##_LABEL
#define _RP2040_GPIO_PROP(n, p) DT_INST_RASPBERRYPI_RP2040_GPIO_##n##_PROP_##p

#define RP2040_GPIO_DEFINE(n)                                       \
    static const struct gpio_rp2040_config gpio_rp2040_cfg_##n = {  \
        .io_base   = DT_INST_RASPBERRYPI_RP2040_GPIO_##n##_REG_ADDR, \
        .sio_base  = _RP2040_GPIO_PROP(n, SIO_REG),                \
        .pads_base = _RP2040_GPIO_PROP(n, PADS_REG),               \
    };                                                              \
    DEVICE_DT_DEFINE(_RP2040_GPIO_LABEL(n),                         \
                     NULL, NULL, &gpio_rp2040_cfg_##n,               \
                     &gpio_rp2040_api, 20);

DT_INST_FOREACH_STATUS_OKAY(RASPBERRYPI_RP2040_GPIO, RP2040_GPIO_DEFINE)

#endif /* CONFIG_GPIO_RP2040 */
