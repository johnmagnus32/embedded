/*
 * adc_stm32.c — STM32 ADC driver
 *
 * Single-channel polling ADC. Config from devicetree.
 */

#include <stdint.h>
#include "devicetree.h"
#include "device.h"
#include "drivers/adc.h"
#include "drivers/clock.h"

#define ADC_SR_OFF   0x00
#define ADC_CR1_OFF  0x04
#define ADC_CR2_OFF  0x08
#define ADC_SQR3_OFF 0x34
#define ADC_DR_OFF   0x4C

#define ADC_SR_EOC      (1 << 1)
#define ADC_CR2_ADON    (1 << 0)
#define ADC_CR2_SWSTART (1 << 30)

#define REG(base, off) (*(volatile uint32_t *)((base) + (off)))

struct adc_stm32_config {
    uint32_t base;
    uint8_t  clk_bus;
    uint8_t  clk_bit;
    uint8_t  channel;
};

DEVICE_DT_DECLARE(rcc);

static uint32_t adc_stm32_read(const struct device *dev)
{
    const struct adc_stm32_config *cfg = dev->config;
    REG(cfg->base, ADC_CR2_OFF) = ADC_CR2_ADON | ADC_CR2_SWSTART;
    while (!(REG(cfg->base, ADC_SR_OFF) & ADC_SR_EOC)) {}
    return REG(cfg->base, ADC_DR_OFF);
}

static int adc_stm32_init(const struct device *dev)
{
    const struct adc_stm32_config *cfg = dev->config;
    clock_on(DEVICE_DT_GET(rcc), cfg->clk_bus, cfg->clk_bit);
    REG(cfg->base, ADC_CR2_OFF) = ADC_CR2_ADON;
    REG(cfg->base, ADC_SQR3_OFF) = cfg->channel;
    return 0;
}

static const struct adc_driver_api adc_stm32_api = {
    .read = adc_stm32_read,
};

#define ADC_STM32_DEFINE(n)                                             \
    static const struct adc_stm32_config adc_cfg_##n = {               \
        .base    = DT_INST_ST_STM32_ADC_##n##_REG_ADDR,               \
        .clk_bus = DT_INST_ST_STM32_ADC_##n##_CLK_BUS,                \
        .clk_bit = DT_INST_ST_STM32_ADC_##n##_CLK_BIT,                \
        .channel = DT_INST_ST_STM32_ADC_##n##_PROP_CHANNEL,           \
    };                                                                  \
    DEVICE_DT_DEFINE(DT_INST_ST_STM32_ADC_##n##_LABEL,                \
                     adc_stm32_init, NULL, &adc_cfg_##n,               \
                     &adc_stm32_api, 40);

DT_INST_FOREACH_STATUS_OKAY(ST_STM32_ADC, ADC_STM32_DEFINE)
