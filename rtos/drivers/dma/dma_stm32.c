/*
 * dma_stm32.c — STM32 DMA1 controller driver
 *
 * Owns all DMA1 registers and ISR handlers.
 */

#include <stdint.h>
#include "devicetree.h"
#include "device.h"
#include "drivers/dma.h"
#include "drivers/clock.h"

/* DMA1 register offsets */
#define DMA_LISR    0x00
#define DMA_HISR    0x04
#define DMA_LIFCR   0x08
#define DMA_HIFCR   0x0C
/* Per-stream: base + 0x10 + stream * 0x18 */
#define DMA_SxCR(s)    (0x10 + (s) * 0x18)
#define DMA_SxNDTR(s)  (0x14 + (s) * 0x18)
#define DMA_SxPAR(s)   (0x18 + (s) * 0x18)
#define DMA_SxM0AR(s)  (0x1C + (s) * 0x18)

/* NVIC */
#define NVIC_ISER0  (*(volatile uint32_t *)0xE000E100)

#define REG(base, off) (*(volatile uint32_t *)((base) + (off)))

/* Interrupt flag bit positions per stream (in LISR/HISR) */
static const uint8_t tcif_bit[] = { 5, 11, 21, 27, 5, 11, 21, 27 };
static const uint8_t htif_bit[] = { 4, 10, 20, 26, 4, 10, 20, 26 };

/* IRQ numbers for DMA1 streams 0-7 */
static const uint8_t stream_irq[] = { 11, 12, 13, 14, 15, 16, 17, 47 };

struct dma_stm32_config {
    uint32_t base;
    uint8_t  clk_bus;
    uint8_t  clk_bit;
};

#define DMA_MAX_STREAMS 8

struct dma_stm32_data {
    struct dma_channel_config ch[DMA_MAX_STREAMS];
};

DEVICE_DT_DECLARE(rcc);
DEVICE_DT_DECLARE(dma1);

static int dma_stm32_configure(const struct device *dev,
                               const struct dma_channel_config *cfg)
{
    struct dma_stm32_data *data = dev->data;
    if (cfg->stream >= DMA_MAX_STREAMS) return -1;
    data->ch[cfg->stream] = *cfg;
    return 0;
}

static int dma_stm32_start(const struct device *dev, uint8_t stream)
{
    const struct dma_stm32_config *cfg = dev->config;
    struct dma_stm32_data *data = dev->data;
    if (stream >= DMA_MAX_STREAMS) return -1;

    struct dma_channel_config *ch = &data->ch[stream];
    uint32_t base = cfg->base;

    /* Disable stream first */
    REG(base, DMA_SxCR(stream)) = 0;

    /* Set addresses and count */
    REG(base, DMA_SxPAR(stream))  = ch->periph_addr;
    REG(base, DMA_SxM0AR(stream)) = ch->mem_addr;
    REG(base, DMA_SxNDTR(stream)) = ch->count;

    /* Build CR: CHSEL | DIR | PSIZE | MSIZE | MINC | CIRC | TCIE | HTIE */
    uint32_t cr = ((uint32_t)ch->channel << 25)
                | ((uint32_t)(ch->direction & 3) << 6)
                | ((uint32_t)ch->periph_size << 11)
                | ((uint32_t)ch->mem_size << 13)
                | (ch->mem_inc ? (1 << 10) : 0)
                | (ch->circular ? (1 << 8) : 0)
                | (1 << 4)   /* TCIE */
                | (1 << 3);  /* HTIE */

    /* Enable NVIC for this stream */
    uint8_t irq = stream_irq[stream];
    if (irq < 32)
        NVIC_ISER0 = (1 << irq);
    else
        (*(volatile uint32_t *)0xE000E104) = (1 << (irq - 32));

    /* Enable stream */
    REG(base, DMA_SxCR(stream)) = cr | 1;
    return 0;
}

static int dma_stm32_stop(const struct device *dev, uint8_t stream)
{
    const struct dma_stm32_config *cfg = dev->config;
    if (stream >= DMA_MAX_STREAMS) return -1;
    REG(cfg->base, DMA_SxCR(stream)) &= ~1u;
    return 0;
}

static void dma_stm32_isr(uint8_t stream)
{
    const struct device *dev = DEVICE_DT_GET(dma1);
    const struct dma_stm32_config *cfg = dev->config;
    struct dma_stm32_data *data = dev->data;
    uint32_t base = cfg->base;

    /* Read status register (LISR for 0-3, HISR for 4-7) */
    uint32_t isr = (stream < 4)
        ? REG(base, DMA_LISR)
        : REG(base, DMA_HISR);

    int status = -1;
    if (isr & (1u << tcif_bit[stream])) {
        status = DMA_STATUS_COMPLETE;
    } else if (isr & (1u << htif_bit[stream])) {
        status = DMA_STATUS_HALF_COMPLETE;
    }

    /* Clear flags (LIFCR for 0-3, HIFCR for 4-7) */
    uint32_t clear = (1u << tcif_bit[stream]) | (1u << htif_bit[stream]);
    if (stream < 4)
        REG(base, DMA_LIFCR) = clear;
    else
        REG(base, DMA_HIFCR) = clear;

    if (status >= 0 && data->ch[stream].callback)
        data->ch[stream].callback(dev, stream, status, data->ch[stream].user_data);
}

/* ISR handlers — names must match startup_m4.s vector table */
void dma1_stream0_handler(void *arg) { (void)arg; dma_stm32_isr(0); }
void dma1_stream1_handler(void *arg) { (void)arg; dma_stm32_isr(1); }
void dma1_stream2_handler(void *arg) { (void)arg; dma_stm32_isr(2); }
void dma1_stream3_handler(void *arg) { (void)arg; dma_stm32_isr(3); }
void dma1_stream4_handler(void *arg) { (void)arg; dma_stm32_isr(4); }
void dma1_stream5_handler(void *arg) { (void)arg; dma_stm32_isr(5); }
void dma1_stream6_handler(void *arg) { (void)arg; dma_stm32_isr(6); }
void dma1_stream7_handler(void *arg) { (void)arg; dma_stm32_isr(7); }

static const struct dma_driver_api dma_stm32_api = {
    .configure = dma_stm32_configure,
    .start     = dma_stm32_start,
    .stop      = dma_stm32_stop,
};

static int dma_stm32_init(const struct device *dev)
{
    const struct dma_stm32_config *cfg = dev->config;
    clock_on(DEVICE_DT_GET(rcc), cfg->clk_bus, cfg->clk_bit);
    return 0;
}

#define DMA_DEFINE(n)                                                   \
    static const struct dma_stm32_config dma_cfg_##n = {               \
        .base    = DT_INST_ST_STM32_DMA_##n##_REG_ADDR,               \
        .clk_bus = DT_INST_ST_STM32_DMA_##n##_CLK_BUS,                \
        .clk_bit = DT_INST_ST_STM32_DMA_##n##_CLK_BIT,                \
    };                                                                  \
    static struct dma_stm32_data dma_data_##n;                         \
    DEVICE_DT_DEFINE(DT_INST_ST_STM32_DMA_##n##_LABEL,                \
                     dma_stm32_init, &dma_data_##n, &dma_cfg_##n,      \
                     &dma_stm32_api, 15);

DT_INST_FOREACH_STATUS_OKAY(ST_STM32_DMA, DMA_DEFINE)
