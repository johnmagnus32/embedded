/*
 * i2s_stm32.c — STM32 I2S audio output driver
 *
 * Uses SPI2 peripheral in I2S mode. Config from devicetree.
 */

#include <stdint.h>
#include "devicetree.h"
#include "device.h"
#include "drivers/audio.h"
#include "drivers/clock.h"

/* SPI/I2S register offsets */
#define I2S_CR1_OFF   0x00
#define I2S_SR_OFF    0x08
#define I2S_DR_OFF    0x0C
#define I2S_CFGR_OFF  0x1C
#define I2S_PR_OFF    0x20
#define I2S_TXE       (1 << 1)

#define REG(base, off) (*(volatile uint32_t *)((base) + (off)))

struct i2s_stm32_config {
    uint32_t base;
    uint8_t  clk_bus;
    uint8_t  clk_bit;
};

DEVICE_DT_DECLARE(rcc);

static void i2s_stm32_write_sample(const struct device *dev,
                                   int16_t left, int16_t right)
{
    const struct i2s_stm32_config *cfg = dev->config;
    while (!(REG(cfg->base, I2S_SR_OFF) & I2S_TXE)) {}
    REG(cfg->base, I2S_DR_OFF) = (uint16_t)left;
    while (!(REG(cfg->base, I2S_SR_OFF) & I2S_TXE)) {}
    REG(cfg->base, I2S_DR_OFF) = (uint16_t)right;
}

static int i2s_stm32_init(const struct device *dev)
{
    const struct i2s_stm32_config *cfg = dev->config;
    clock_on(DEVICE_DT_GET(rcc), cfg->clk_bus, cfg->clk_bit);
    REG(cfg->base, I2S_CFGR_OFF) = (1 << 11) | (1 << 10); /* I2SMOD + I2SE */
    REG(cfg->base, I2S_PR_OFF) = 0;
    return 0;
}

static const struct audio_driver_api i2s_stm32_api = {
    .write_sample = i2s_stm32_write_sample,
};

#define I2S_DEFINE(n)                                                   \
    static const struct i2s_stm32_config i2s_cfg_##n = {               \
        .base    = DT_INST_ST_STM32_I2S_##n##_REG_ADDR,               \
        .clk_bus = DT_INST_ST_STM32_I2S_##n##_CLK_BUS,                \
        .clk_bit = DT_INST_ST_STM32_I2S_##n##_CLK_BIT,                \
    };                                                                  \
    DEVICE_DT_DEFINE(DT_INST_ST_STM32_I2S_##n##_LABEL,                \
                     i2s_stm32_init, NULL, &i2s_cfg_##n,               \
                     &i2s_stm32_api);

DT_INST_FOREACH_STATUS_OKAY(ST_STM32_I2S, I2S_DEFINE)
