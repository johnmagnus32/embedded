/*
 * i2s_stm32.c — STM32 I2S audio output driver (buffer submission model)
 *
 * The driver owns double-buffers and a semaphore. The app calls:
 *   audio_get_buffer() — blocks until DMA frees a half
 *   audio_put_buffer() — advances to next half
 *
 * DMA runs in circular mode over both halves. The ISR just signals
 * the semaphore — no app code runs in interrupt context.
 */

#include <stdint.h>
#include <string.h>
#include "devicetree.h"
#include "device.h"
#include "drivers/audio.h"
#include "drivers/clock.h"
#include "drivers/dma.h"
#include "sync.h"

/* SPI/I2S register offsets */
#define I2S_CR2_OFF   0x04
#define I2S_SR_OFF    0x08
#define I2S_DR_OFF    0x0C
#define I2S_CFGR_OFF  0x1C
#define I2S_PR_OFF    0x20
#define I2S_TXDMAEN   (1 << 1)

#define REG(base, off) (*(volatile uint32_t *)((base) + (off)))

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
    int16_t buf[2][AUDIO_BUF_SAMPLES * 2];  /* two halves, stereo */
    struct semaphore buf_free;               /* signaled when DMA frees a half */
    int write_idx;                           /* which half the app fills next */
    int playing;
};

DEVICE_DT_DECLARE(rcc);
DEVICE_DT_DECLARE(dma1);

/* DMA ISR — just signals the semaphore, no app code */
static void i2s_dma_callback(const struct device *dma_dev, uint8_t stream,
                             int status, void *user_data)
{
    (void)dma_dev; (void)stream; (void)status;
    struct i2s_stm32_data *data = user_data;
    sem_give(&data->buf_free);
}

static int i2s_stm32_start(const struct device *dev)
{
    const struct i2s_stm32_config *cfg = dev->config;
    struct i2s_stm32_data *data = dev->data;

    data->dma_dev = DEVICE_DT_GET(dma1);
    data->write_idx = 0;
    data->playing = 1;
    data->buf_free = (struct semaphore)SEM_INIT(0);

    /* Zero both halves (silence until app fills) */
    memset(data->buf, 0, sizeof(data->buf));

    /* Enable TXDMAEN */
    REG(cfg->base, I2S_CR2_OFF) = I2S_TXDMAEN;

    /* Configure DMA circular over both halves */
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
        .count       = AUDIO_BUF_SAMPLES * 2 * 2,  /* both halves, stereo */
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

    data->playing = 0;
    if (data->dma_dev)
        dma_stop(data->dma_dev, cfg->dma_stream);
    REG(cfg->base, I2S_CR2_OFF) = 0;

    /* Unblock anyone waiting in get_buffer */
    sem_give(&data->buf_free);
    return 0;
}

static int16_t *i2s_stm32_get_buffer(const struct device *dev)
{
    struct i2s_stm32_data *data = dev->data;
    sem_take(&data->buf_free);
    if (!data->playing) return NULL;
    return data->buf[data->write_idx];
}

static void i2s_stm32_put_buffer(const struct device *dev, int16_t *buf)
{
    struct i2s_stm32_data *data = dev->data;
    (void)buf;  /* buffer is already in the DMA ring */
    data->write_idx ^= 1;
}

/* Pin configuration helper */
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

    clock_on(clk, cfg->clk_bus, cfg->clk_bit);

    /* GPIO port clocks + pin config */
    uint8_t ws_idx = (cfg->ws_port - 0x40020000) / 0x400;
    clock_on(clk, 0, ws_idx);
    uint8_t sck_idx = (cfg->sck_port - 0x40020000) / 0x400;
    if (sck_idx != ws_idx) clock_on(clk, 0, sck_idx);
    uint8_t sd_idx = (cfg->sd_port - 0x40020000) / 0x400;
    if (sd_idx != ws_idx && sd_idx != sck_idx) clock_on(clk, 0, sd_idx);

    i2s_pin_configure_af(cfg->ws_port, cfg->ws_pin, cfg->af);
    i2s_pin_configure_af(cfg->sck_port, cfg->sck_pin, cfg->af);
    i2s_pin_configure_af(cfg->sd_port, cfg->sd_pin, cfg->af);

    /* Enable PLLI2S */
    volatile uint32_t *rcc_cr = (volatile uint32_t *)0x40023800;
    *rcc_cr |= (1 << 26);
    while (!(*rcc_cr & (1 << 27))) {}

    /* I2SCFGR: I2SMOD + Master TX */
    REG(cfg->base, I2S_CFGR_OFF) = (1 << 11) | (2 << 8);
    /* I2SPR: ~22 kHz at 96 MHz I2SCLK */
    REG(cfg->base, I2S_PR_OFF) = 68;
    /* Enable */
    REG(cfg->base, I2S_CFGR_OFF) |= (1 << 10);
    return 0;
}

static const struct audio_driver_api i2s_stm32_api = {
    .start      = i2s_stm32_start,
    .stop       = i2s_stm32_stop,
    .get_buffer = i2s_stm32_get_buffer,
    .put_buffer = i2s_stm32_put_buffer,
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
