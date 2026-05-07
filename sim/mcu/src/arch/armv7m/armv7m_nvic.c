/*
 * armv7m_nvic.c — ARMv7-M Nested Vectored Interrupt Controller
 */
#include "armv7m_nvic.h"
#include "armv7m_cpu.h"
#include "membus.h"

void armv7m_nvic_init(struct armv7m_nvic *n)
{
    n->pending = 0;
    n->scb_icsr = 0;
    n->scb_shpr3 = 0;
    n->scb_vtor = 0;
    n->scb_shcsr = 0;
    n->iser[0] = n->iser[1] = n->iser[2] = 0;
}

void armv7m_nvic_set_pending(struct armv7m_nvic *n, int vector)
{
    n->pending |= (1ULL << vector);
    n->needs_update = 1;
}

void armv7m_nvic_update(struct armv7m_nvic *n, struct armv7m_cpu *cpu, struct membus *bus)
{
    n->needs_update = 0;
    if (n->scb_icsr & (1 << 28))
        n->pending |= (1ULL << IRQ_VEC_PENDSV);

    if (!n->pending || cpu->primask || cpu->in_handler || cpu->irq_shadow) {
        if (n->pending) n->needs_update = 1;
        return;
    }

    int vec = 0;
    if (n->pending & (1ULL << IRQ_VEC_SVC)) {
        vec = IRQ_VEC_SVC;
    } else if (n->pending & (1ULL << IRQ_VEC_SYSTICK)) {
        vec = IRQ_VEC_SYSTICK;
    } else if (n->pending & (1ULL << IRQ_VEC_PENDSV)) {
        vec = IRQ_VEC_PENDSV;
        n->scb_icsr &= ~(1 << 28);
    } else {
        /* External IRQs: vectors 16+ (IRQ0=16, IRQ1=17, ...) */
        for (int i = 16; i < 57; i++) {
            int iser_idx = (i - 16) / 32;
            int iser_bit = (i - 16) % 32;
            if ((n->pending & (1ULL << i)) && (n->iser[iser_idx] & (1u << iser_bit))) {
                vec = i;
                break;
            }
        }
    }

    if (vec) {
        n->pending &= ~(1ULL << vec);
        armv7m_take_interrupt(cpu, bus, vec);
    }
}

uint32_t armv7m_nvic_scb_read(void *opaque, uint32_t offset)
{
    struct armv7m_nvic *n = (struct armv7m_nvic *)opaque;
    switch (offset) {
    case 0x04: return n->scb_icsr;
    case 0x08: return n->scb_vtor;
    case 0x20: return n->scb_shpr3;
    case 0x24: return n->scb_shcsr;
    case 0x90: return 0x00000800; /* MPU TYPE */
    default:   return 0;
    }
}

void armv7m_nvic_scb_write(void *opaque, uint32_t offset, uint32_t val)
{
    struct armv7m_nvic *n = (struct armv7m_nvic *)opaque;
    switch (offset) {
    case 0x04: n->scb_icsr = val; n->needs_update = 1; break;
    case 0x08: n->scb_vtor = val; break;
    case 0x20: n->scb_shpr3 = val; break;
    case 0x24: n->scb_shcsr = val; break;
    }
}

uint32_t armv7m_nvic_iser_read(void *opaque, uint32_t offset)
{
    struct armv7m_nvic *n = (struct armv7m_nvic *)opaque;
    int idx = offset / 4;
    if (idx < 3) return n->iser[idx];
    return 0;
}

void armv7m_nvic_iser_write(void *opaque, uint32_t offset, uint32_t val)
{
    struct armv7m_nvic *n = (struct armv7m_nvic *)opaque;
    int idx = offset / 4;
    if (idx < 3) n->iser[idx] |= val;
}
