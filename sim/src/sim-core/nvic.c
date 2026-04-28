/*
 * nvic.c — Nested Vectored Interrupt Controller
 */
#include "nvic.h"
#include "cpu.h"
#include "membus.h"

void nvic_init(struct nvic *n)
{
    n->pending = 0;
    n->scb_icsr = 0;
    n->scb_shpr3 = 0;
    n->scb_vtor = 0;
    n->scb_shcsr = 0;
    n->iser[0] = n->iser[1] = n->iser[2] = 0;
}

void nvic_set_pending(struct nvic *n, int vector)
{
    n->pending |= (1u << vector);
}

void nvic_update(struct nvic *n, struct cpu_state *cpu, struct membus *bus)
{
    if (n->scb_icsr & (1 << 28))
        n->pending |= (1u << IRQ_VEC_PENDSV);

    if (!n->pending || cpu->primask || cpu->in_handler || cpu->irq_shadow)
        return;

    int vec = 0;
    if (n->pending & (1u << IRQ_VEC_SVC)) {
        vec = IRQ_VEC_SVC;
    } else if (n->pending & (1u << IRQ_VEC_SYSTICK)) {
        vec = IRQ_VEC_SYSTICK;
    } else if (n->pending & (1u << IRQ_VEC_PENDSV)) {
        vec = IRQ_VEC_PENDSV;
        n->scb_icsr &= ~(1 << 28);
    }

    if (vec) {
        n->pending &= ~(1u << vec);
        take_interrupt(cpu, bus, vec);
    }
}

/* SCB registers at base 0xE000ED00 */
uint32_t nvic_scb_read(void *opaque, uint32_t offset)
{
    struct nvic *n = (struct nvic *)opaque;
    switch (offset) {
    case 0x04: return n->scb_icsr;
    case 0x08: return n->scb_vtor;
    case 0x20: return n->scb_shpr3;
    case 0x24: return n->scb_shcsr;
    case 0x90: return 0x00000800; /* MPU TYPE */
    default:   return 0;
    }
}

void nvic_scb_write(void *opaque, uint32_t offset, uint32_t val)
{
    struct nvic *n = (struct nvic *)opaque;
    switch (offset) {
    case 0x04: n->scb_icsr = val; break;
    case 0x08: n->scb_vtor = val; break;
    case 0x20: n->scb_shpr3 = val; break;
    case 0x24: n->scb_shcsr = val; break;
    /* 0x90..0xA0: SCB/MPU writes silently ignored */
    }
}

/* ISER registers at base 0xE000E100 */
uint32_t nvic_iser_read(void *opaque, uint32_t offset)
{
    struct nvic *n = (struct nvic *)opaque;
    int idx = offset / 4;
    if (idx < 3) return n->iser[idx];
    return 0;
}

void nvic_iser_write(void *opaque, uint32_t offset, uint32_t val)
{
    struct nvic *n = (struct nvic *)opaque;
    int idx = offset / 4;
    if (idx < 3) n->iser[idx] |= val;
}
