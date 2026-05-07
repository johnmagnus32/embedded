/*
 * clock_rp2040.c — RP2040 reset/clock driver
 *
 * RP2040 doesn't have per-peripheral clock gates like STM32.
 * Instead, peripherals are held in reset and you release them.
 * We use the same clock_on() API but it deasserts reset instead.
 *
 * RESETS base = 0x4000c000
 *   RESET register: write 1 to hold in reset
 *   RESET_DONE: read 1 when peripheral is out of reset
 */

#include <stdint.h>
#include "config.h"

#ifdef CONFIG_CLOCK_RP2040

#include "devicetree.h"
#include "device.h"
#include "drivers/clock.h"

#define REG(base, off) (*(volatile uint32_t *)((base) + (off)))

#define RESETS_RESET      0x00
#define RESETS_RESET_DONE 0x08

struct clock_rp2040_config {
    uint32_t base;
};

static void clock_rp2040_on(const struct device *dev, uint8_t bus, uint8_t bit)
{
    (void)bus;
    const struct clock_rp2040_config *cfg = dev->config;

    /* Clear the reset bit (deassert reset) */
    REG(cfg->base, RESETS_RESET) &= ~(1U << bit);

    /* Wait until reset is done */
    while (!(REG(cfg->base, RESETS_RESET_DONE) & (1U << bit)))
        ;
}

static const struct clock_driver_api clock_rp2040_api = {
    .on = clock_rp2040_on,
};

#define _RP2040_CLK_LABEL(n) DT_INST_RASPBERRYPI_RP2040_RESETS_##n##_LABEL

#define RP2040_RESETS_DEFINE(n)                                     \
    static const struct clock_rp2040_config clock_rp2040_cfg_##n = {\
        .base = DT_INST_RASPBERRYPI_RP2040_RESETS_##n##_REG_ADDR,  \
    };                                                              \
    DEVICE_DT_DEFINE(_RP2040_CLK_LABEL(n),                          \
                     NULL, NULL, &clock_rp2040_cfg_##n,              \
                     &clock_rp2040_api, 10);

DT_INST_FOREACH_STATUS_OKAY(RASPBERRYPI_RP2040_RESETS, RP2040_RESETS_DEFINE)

#endif /* CONFIG_CLOCK_RP2040 */
