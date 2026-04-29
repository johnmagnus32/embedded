/*
 * stm32_dma.c — STM32 DMA controller model
 *
 * Transfers one data item per tick when request_pending is set.
 * Supports circular mode, transfer-complete and half-transfer interrupts.
 */
#include <string.h>
#include "stm32_dma.h"

/* Stream register offsets within a stream block (0x18 bytes each) */
#define STREAM_BASE  0x10
#define STREAM_SIZE  0x18
#define SxCR   0x00
#define SxNDTR 0x04
#define SxPAR  0x08
#define SxM0AR 0x0C
#define SxM1AR 0x10
#define SxFCR  0x14

/* CR bits */
#define CR_EN   (1 << 0)
#define CR_HTIE (1 << 3)
#define CR_TCIE (1 << 4)
#define CR_DIR_SHIFT 6
#define CR_CIRC (1 << 8)
#define CR_MINC (1 << 10)
#define CR_PSIZE_SHIFT 11
#define CR_MSIZE_SHIFT 13

/* Status register bit positions per stream (LISR: 0-3, HISR: 4-7) */
static const int tcif_bit[8] = { 5, 11, 21, 27, 5, 11, 21, 27 };
static const int htif_bit[8] = { 4, 10, 20, 26, 4, 10, 20, 26 };

/* DMA1 stream IRQ vectors: IRQ 11-17 → vector 16+IRQ */
static const int dma1_irq_vec[8] = {
    16+11, 16+12, 16+13, 16+14, 16+15, 16+16, 16+17, 16+47
};

void stm32_dma_init(struct stm32_dma *d, struct armv7m_nvic *nvic, struct membus *bus)
{
    memset(d, 0, sizeof(*d));
    d->nvic = nvic;
    d->bus = bus;
}

uint32_t stm32_dma_read(void *opaque, uint32_t offset)
{
    struct stm32_dma *d = (struct stm32_dma *)opaque;

    if (offset == 0x00) return d->lisr;
    if (offset == 0x04) return d->hisr;
    if (offset == 0x08) return 0; /* LIFCR write-only */
    if (offset == 0x0C) return 0; /* HIFCR write-only */

    if (offset >= STREAM_BASE) {
        int idx = (offset - STREAM_BASE) / STREAM_SIZE;
        int reg = (offset - STREAM_BASE) % STREAM_SIZE;
        if (idx >= 8) return 0;
        struct stm32_dma_stream *s = &d->streams[idx];
        switch (reg) {
        case SxCR:   return s->cr;
        case SxNDTR: return s->ndtr;
        case SxPAR:  return s->par;
        case SxM0AR: return s->m0ar;
        case SxM1AR: return s->m1ar;
        case SxFCR:  return s->fcr;
        }
    }
    return 0;
}

void stm32_dma_write(void *opaque, uint32_t offset, uint32_t val)
{
    struct stm32_dma *d = (struct stm32_dma *)opaque;

    if (offset == 0x08) { d->lisr &= ~val; return; } /* LIFCR */
    if (offset == 0x0C) { d->hisr &= ~val; return; } /* HIFCR */

    if (offset >= STREAM_BASE) {
        int idx = (offset - STREAM_BASE) / STREAM_SIZE;
        int reg = (offset - STREAM_BASE) % STREAM_SIZE;
        if (idx >= 8) return;
        struct stm32_dma_stream *s = &d->streams[idx];
        switch (reg) {
        case SxCR:
            if ((val & CR_EN) && !(s->cr & CR_EN)) {
                /* Enabling: save originals for circular reload */
                s->ndtr_orig = s->ndtr;
                s->m0ar_orig = s->m0ar;
                d->any_active = 1;
            }
            if (!(val & CR_EN))
                s->request_pending = 0;
            s->cr = val;
            break;
        case SxNDTR: s->ndtr = val; break;
        case SxPAR:  s->par = val; break;
        case SxM0AR: s->m0ar = val; break;
        case SxM1AR: s->m1ar = val; break;
        case SxFCR:  s->fcr = val; break;
        }
    }
}

void stm32_dma_tick(struct stm32_dma *d)
{
    if (!d->any_active) return;

    int still_active = 0;
    for (int i = 0; i < 8; i++) {
        struct stm32_dma_stream *s = &d->streams[i];
        if (!(s->cr & CR_EN) || !s->request_pending)
            continue;

        still_active = 1;
        s->request_pending = 0;

        /* Determine transfer size: 1=half, 2=word (byte=0) */
        int psize = (s->cr >> CR_PSIZE_SHIFT) & 3;
        int msize = (s->cr >> CR_MSIZE_SHIFT) & 3;
        int bytes = 1 << psize;
        (void)msize;

        /* Transfer direction: 01 = mem→periph */
        int dir = (s->cr >> CR_DIR_SHIFT) & 3;
        uint32_t data;

        if (dir == 1) {
            /* Memory to peripheral */
            if (bytes == 2)
                data = membus_read16(d->bus, s->m0ar);
            else if (bytes == 4)
                data = membus_read32(d->bus, s->m0ar);
            else
                data = membus_read8(d->bus, s->m0ar);

            if (bytes == 2)
                membus_write16(d->bus, s->par, (uint16_t)data);
            else if (bytes == 4)
                membus_write32(d->bus, s->par, data);
            else
                membus_write8(d->bus, s->par, (uint8_t)data);
        } else {
            /* Peripheral to memory */
            if (bytes == 2)
                data = membus_read16(d->bus, s->par);
            else if (bytes == 4)
                data = membus_read32(d->bus, s->par);
            else
                data = membus_read8(d->bus, s->par);

            if (bytes == 2)
                membus_write16(d->bus, s->m0ar, (uint16_t)data);
            else if (bytes == 4)
                membus_write32(d->bus, s->m0ar, data);
            else
                membus_write8(d->bus, s->m0ar, (uint8_t)data);
        }

        /* Advance memory pointer if MINC */
        if (s->cr & CR_MINC)
            s->m0ar += bytes;

        s->ndtr--;

        /* Half-transfer interrupt */
        if (s->ndtr == s->ndtr_orig / 2) {
            if (s->cr & CR_HTIE) {
                uint32_t *sr = (i < 4) ? &d->lisr : &d->hisr;
                *sr |= (1u << htif_bit[i]);
                armv7m_nvic_set_pending(d->nvic, dma1_irq_vec[i]);
            }
        }

        /* Transfer complete */
        if (s->ndtr == 0) {
            uint32_t *sr = (i < 4) ? &d->lisr : &d->hisr;
            if (s->cr & CR_CIRC) {
                s->ndtr = s->ndtr_orig;
                s->m0ar = s->m0ar_orig;
            } else {
                s->cr &= ~CR_EN;
            }
            if (s->cr & CR_TCIE) {
                *sr |= (1u << tcif_bit[i]);
                armv7m_nvic_set_pending(d->nvic, dma1_irq_vec[i]);
            }
        }
    }
    d->any_active = still_active;
}
