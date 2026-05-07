/*
 * i2s_stm32.c — STM32 I2S audio output driver
 *
 * Uses SPI2 peripheral in I2S mode. DMA streaming via dma.h API.
 */

#include <stdint.h>
#include "devicetree.h"
#include "device.h"
#include "drivers/audio.h"
#include "drivers/clock.h"
#include "drivers/dma.h"

/* SPI/I2S register offsets (this driver's own peripheral) */
#define I2S_CR1_OFF   0x00
#define I2S_CR2_OFF   0x04
#define I2S_SR_OFF    0x08
#define I2S_DR_OFF    0x0C
#define I2S_CFGR_OFF  0x1C
#define I2S_PR_OFF    0x20
#define I2S_TXE       (1 << 1)
#define I2S_TXDMAEN   (1 << 1)

#define REG(base, off) (*(volatile uint32_t *)((base) + (off)))

#define BUF_SAMPLES 256

struct i2s_stm32_config {
    uint32_t base;
    uint8_t  clk_bus;
    uint8_t  clk_bit;
    uint8_t  dma_stream;
    uint8_t  dma_channel;
    uint32_t ws_port;
    uint8_t  ws_pin;
    uint32_t sck_port;
    uint8_t  sck_pin;
    uint32_t sd_port;
    uint8_t  sd_pin;
    uint8_t  af;
};

struct i2s_stm32_data {
    const struct device *dma_dev;
    audio_fill_fn fill;
    void *user_data;
    int16_t buf[2][BUF_SAMPLES];
    int buf_idx;
};

DEVICE_DT_DECLARE(rcc);
DEVICE_DT_DECLARE(dma1);

static void i2s_dma_callback(const struct device *dma_dev, uint8_t stream,
                             int status, void *user_data)
{
    struct i2s_stm32_data *data = user_data;
    if (!data->fill) return;

    /* On half-complete: fill first half. On complete: fill second half. */
    int idx = (status == DMA_STATUS_HALF_COMPLETE) ? 0 : 1;
    data->fill(data->buf[idx], BUF_SAMPLES, data->user_data);
}

static void i2s_stm32_write_sample(const struct device *dev,
                                   int16_t left, int16_t right)
{
    const struct i2s_stm32_config *cfg = dev->config;
    while (!(REG(cfg->base, I2S_SR_OFF) & I2S_TXE)) {}
    REG(cfg->base, I2S_DR_OFF) = (uint16_t)left;
    while (!(REG(cfg->base, I2S_SR_OFF) & I2S_TXE)) {}
    REG(cfg->base, I2S_DR_OFF) = (uint16_t)right;
}

static int i2s_stm32_start(const struct device *dev,
                           audio_fill_fn fill, void *user_data)
{
    const struct i2s_stm32_config *cfg = dev->config;
    struct i2s_stm32_data *data = dev->data;

    data->fill = fill;
    data->user_data = user_data;
    data->dma_dev = DEVICE_DT_GET(dma1);
    data->buf_idx = 0;

    /* Pre-fill both buffer halves with silence */
    for (int i = 0; i < BUF_SAMPLES; i++) {
        data->buf[0][i] = 0;
        data->buf[1][i] = 0;
    }

    /* Enable TXDMAEN in SPI CR2 (this is the I2S driver's own register) */
    REG(cfg->base, I2S_CR2_OFF) = I2S_TXDMAEN;

    /* Configure DMA via the DMA driver API */
    struct dma_channel_config dma_cfg = {
        .stream      = cfg->dma_stream,
        .channel     = cfg->dma_channel,
        .direction   = DMA_MEM_TO_PERIPH,
        .periph_size = 1,  /* 16-bit */
        .mem_size    = 1,  /* 16-bit */
        .mem_inc     = 1,
        .circular    = 1,
        .periph_addr = cfg->base + I2S_DR_OFF,
        .mem_addr    = (uint32_t)data->buf[0],
        .count       = BUF_SAMPLES * 2,  /* both halves */
        .callback    = i2s_dma_callback,
        .user_data   = data,
    };

    dma_configure(data->dma_dev, &dma_cfg);
    dma_start(data->dma_dev, cfg->dma_stream);
    return 0;
}

static int i2s_stm32_stop(const struct device *dev)
{
    const struct i2s_stm32_config *cfg = dev->config;
    struct i2s_stm32_data *data = dev->data;

    if (data->dma_dev)
        dma_stop(data->dma_dev, cfg->dma_stream);

    /* Disable TXDMAEN */
    REG(cfg->base, I2S_CR2_OFF) = 0;
    data->fill = 0;
    return 0;
}

static void i2s_pin_configure_af(uint32_t port, uint8_t pin, uint8_t af)
{
    volatile uint32_t *moder = (volatile uint32_t *)(port + 0x00);
    volatile uint32_t *afr = (volatile uint32_t *)(port + (pin < 8 ? 0x20 : 0x24));
    uint8_t af_pos = (pin % 8) * 4;
    *moder &= ~(3U << (pin * 2));
    *moder |=  (2U << (pin * 2));
    *afr &= ~(0xFU << af_pos);
    *afr |=  ((uint32_t)af << af_pos);
}

static int i2s_stm32_init(const struct device *dev)
{
    const struct i2s_stm32_config *cfg = dev->config;
    const struct device *clk = DEVICE_DT_GET(rcc);

    /* Enable peripheral clock */
    clock_on(clk, cfg->clk_bus, cfg->clk_bit);

    /* Enable GPIO port clocks and configure pins */
    uint8_t ws_port_idx = (cfg->ws_port - 0x40020000) / 0x400;
    clock_on(clk, 0, ws_port_idx);
    uint8_t sck_port_idx = (cfg->sck_port - 0x40020000) / 0x400;
    if (sck_port_idx != ws_port_idx) clock_on(clk, 0, sck_port_idx);
    uint8_t sd_port_idx = (cfg->sd_port - 0x40020000) / 0x400;
    if (sd_port_idx != ws_port_idx && sd_port_idx != sck_port_idx)
        clock_on(clk, 0, sd_port_idx);

    i2s_pin_configure_af(cfg->ws_port, cfg->ws_pin, cfg->af);
    i2s_pin_configure_af(cfg->sck_port, cfg->sck_pin, cfg->af);
    i2s_pin_configure_af(cfg->sd_port, cfg->sd_pin, cfg->af);

    /* Enable PLLI2S (I2S clock source) */
    volatile uint32_t *rcc_cr = (volatile uint32_t *)0x40023800;
    *rcc_cr |= (1 << 26);  /* PLLI2SON */
    while (!(*rcc_cr & (1 << 27))) {}  /* wait PLLI2SRDY */

    /* I2SCFGR: I2SMOD=1, I2SCFG=10 (Master TX), Philips std, 16-bit */
    REG(cfg->base, I2S_CFGR_OFF) = (1 << 11) | (2 << 8);
    /* I2SPR: prescaler for ~22 kHz sample rate at 96 MHz I2SCLK
     * Fs = I2SCLK / (32 * (2*I2SDIV + ODD))
     * 96M / (32 * 136) = 22,058 Hz → I2SDIV=68, ODD=0 */
    REG(cfg->base, I2S_PR_OFF) = 68;
    /* Enable */
    REG(cfg->base, I2S_CFGR_OFF) |= (1 << 10);
    return 0;
}

static const struct audio_driver_api i2s_stm32_api = {
    .write_sample = i2s_stm32_write_sample,
    .start        = i2s_stm32_start,
    .stop         = i2s_stm32_stop,
};

#define I2S_DEFINE(n)                                                   \
    static const struct i2s_stm32_config i2s_cfg_##n = {               \
        .base        = DT_INST_ST_STM32_I2S_##n##_REG_ADDR,           \
        .clk_bus     = DT_INST_ST_STM32_I2S_##n##_CLK_BUS,            \
        .clk_bit     = DT_INST_ST_STM32_I2S_##n##_CLK_BIT,            \
        .dma_stream  = 4,                                              \
        .dma_channel = 0,                                              \
        .ws_port     = DT_INST_ST_STM32_I2S_##n##_PROP_WS_PORT,       \
        .ws_pin      = DT_INST_ST_STM32_I2S_##n##_PROP_WS_PIN,        \
        .sck_port    = DT_INST_ST_STM32_I2S_##n##_PROP_SCK_PORT,      \
        .sck_pin     = DT_INST_ST_STM32_I2S_##n##_PROP_SCK_PIN,       \
        .sd_port     = DT_INST_ST_STM32_I2S_##n##_PROP_SD_PORT,       \
        .sd_pin      = DT_INST_ST_STM32_I2S_##n##_PROP_SD_PIN,        \
        .af          = DT_INST_ST_STM32_I2S_##n##_PROP_AF,            \
    };                                                                  \
    static struct i2s_stm32_data i2s_data_##n;                         \
    DEVICE_DT_DEFINE(DT_INST_ST_STM32_I2S_##n##_LABEL,                \
                     i2s_stm32_init, &i2s_data_##n, &i2s_cfg_##n,     \
                     &i2s_stm32_api, 20);

DT_INST_FOREACH_STATUS_OKAY(ST_STM32_I2S, I2S_DEFINE)
