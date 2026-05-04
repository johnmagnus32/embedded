/*
 * stm32_spi.c — STM32 SPI/I2S peripheral model with clock pacing
 *
 * Normal SPI: TXE cleared on DR write, re-set after spi_cycles_per_byte
 *   ticks (derived from CR1 baud rate prescaler). 8 SPI clocks per byte.
 * I2S mode: TXE paced by i2s_cycles_per_sample (one stereo pair).
 */
#include <string.h>
#include "stm32_spi.h"
#include "stm32_dma.h"

#define SPI_CR1     0x00
#define SPI_CR2     0x04
#define SPI_SR      0x08
#define SPI_DR      0x0C
#define SPI_I2SCFGR 0x1C
#define SPI_I2SPR   0x20

#define SR_TXE   (1 << 1)
#define SR_RXNE  (1 << 0)
#define SR_BSY   (1 << 7)

#define DEFAULT_SYSCLK      16000000
#define DEFAULT_SAMPLE_RATE  22050

/*
 * SPI baud rate prescaler: CR1 bits [5:3] (BR)
 *   000 = /2, 001 = /4, 010 = /8, ... 111 = /256
 * Cycles per byte = 8 bits * prescaler (SPI clock = APB / prescaler)
 */
static void spi_compute_baud(struct stm32_spi *s)
{
    int br = (s->cr1 >> 3) & 7;
    int prescaler = 2 << br;  /* 2, 4, 8, 16, 32, 64, 128, 256 */
    s->spi_cycles_per_byte = 8 * prescaler;
}

static void i2s_compute_prescaler(struct stm32_spi *s)
{
    uint32_t i2sdiv = (s->i2spr >> 0) & 0xFF;
    uint32_t odd    = (s->i2spr >> 8) & 1;
    if (i2sdiv >= 2)
        s->i2s_cycles_per_sample = 32 * (2 * i2sdiv + odd);
    else
        s->i2s_cycles_per_sample = DEFAULT_SYSCLK / DEFAULT_SAMPLE_RATE;
}

void stm32_spi_init(struct stm32_spi *s)
{
    memset(s, 0, sizeof(*s));
    s->sr = SR_TXE;
    s->spi_cycles_per_byte = 16;  /* default: /2 prescaler, 8*2=16 */
    s->i2s_cycles_per_sample = DEFAULT_SYSCLK / DEFAULT_SAMPLE_RATE;
}

uint32_t stm32_spi_read(void *opaque, uint32_t offset)
{
    struct stm32_spi *s = (struct stm32_spi *)opaque;
    switch (offset) {
    case SPI_CR1:     return s->cr1;
    case SPI_CR2:     return s->cr2;
    case SPI_SR:      return s->sr;
    case SPI_DR:      s->sr &= ~SR_RXNE; return s->rx_data;
    case SPI_I2SCFGR: return s->i2scfgr;
    case SPI_I2SPR:   return s->i2spr;
    default:          return 0;
    }
}

static inline void kick_dma(struct stm32_spi *s)
{
    if ((s->cr2 & (1 << 1)) && s->dma_tx) {
        s->dma_tx->request_pending = 1;
        if (s->dma_tx->any_active_ptr)
            *s->dma_tx->any_active_ptr = 1;
        if (s->dma_tx->any_pending_ptr)
            *s->dma_tx->any_pending_ptr = 1;
    }
}

static void spi_txe_callback(void *opaque)
{
    struct stm32_spi *s = (struct stm32_spi *)opaque;
    s->sr |= SR_TXE;
    s->sr &= ~SR_BSY;
    kick_dma(s);
}

static void i2s_txe_callback(void *opaque)
{
    struct stm32_spi *s = (struct stm32_spi *)opaque;
    s->sr |= SR_TXE;
    kick_dma(s);
}

void stm32_spi_write(void *opaque, uint32_t offset, uint32_t val)
{
    struct stm32_spi *s = (struct stm32_spi *)opaque;
    switch (offset) {
    case SPI_CR1:
        s->cr1 = val;
        spi_compute_baud(s);
        break;
    case SPI_CR2:
        s->cr2 = val;
        if ((val & (1 << 1)) && (s->sr & SR_TXE))
            kick_dma(s);
        break;
    case SPI_DR:
        if (s->i2s_mode) {
            if (s->i2s_lr == 0) {
                /* Left channel: store, TXE immediate for right channel */
                s->i2s_pending_left = (int16_t)val;
                s->i2s_lr = 1;
                s->sr |= SR_TXE;
                kick_dma(s);
            } else {
                /* Right channel: push sample, start I2S pacing delay */
                if (s->i2s_sink && s->i2s_sink->write)
                    s->i2s_sink->write(s->i2s_sink->opaque,
                                       s->i2s_pending_left, (int16_t)val);
                s->i2s_lr = 0;
                s->sr &= ~SR_TXE;
                if (s->eq && s->cycle_ptr)
                    event_schedule(s->eq, s->spi_evt_id,
                                   *s->cycle_ptr + s->i2s_cycles_per_sample,
                                   i2s_txe_callback, s);
            }
        } else {
            /* Normal SPI: transfer byte, clear TXE, schedule TXE re-assertion */
            s->rx_data = spi_bus_transfer(&s->bus, (uint8_t)val);
            s->sr |= SR_RXNE;
            s->sr &= ~(SR_TXE | SR_BSY);
            s->sr |= SR_BSY;
            if (s->eq && s->cycle_ptr)
                event_schedule(s->eq, s->spi_evt_id,
                               *s->cycle_ptr + s->spi_cycles_per_byte,
                               spi_txe_callback, s);
        }
        break;
    case SPI_I2SCFGR:
        s->i2scfgr = val;
        s->i2s_mode = (val & (1 << 11)) ? 1 : 0;
        if (s->i2s_mode) {
            s->i2s_lr = 0;
            s->sr |= SR_TXE;
        }
        break;
    case SPI_I2SPR:
        s->i2spr = val;
        i2s_compute_prescaler(s);
        break;
    }
}

/* stm32_spi_tick removed — SPI/I2S pacing now uses event queue callbacks */
