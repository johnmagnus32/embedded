/*
 * nvic.c — Nested Vectored Interrupt Controller
 *
 * Collects pending IRQs from devices, prioritizes, and tells the CPU
 * to take the highest-priority pending interrupt.
 */
#include "nvic.h"
#include "cpu.h"

void nvic_init(struct nvic *n)
{
    n->pending = 0;
    n->scb_icsr = 0;
    n->scb_shpr3 = 0;
    n->scb_vtor = 0;
    n->scb_shcsr = 0;
}

void nvic_set_pending(struct nvic *n, int vector)
{
    n->pending |= (1u << vector);
}

void nvic_update(struct nvic *n, struct cpu_state *cpu, uint8_t *flash, uint8_t *ram)
{
    /* Check if firmware wrote PENDSVSET to SCB_ICSR */
    if (n->scb_icsr & (1 << 28))
        n->pending |= (1u << IRQ_VEC_PENDSV);

    /* Nothing to do if masked or already in handler */
    if (!n->pending || cpu->primask || cpu->in_handler || cpu->irq_shadow)
        return;

    /* Take highest priority: SVC > SysTick > PendSV */
    int vec = 0;
    if (n->pending & (1u << IRQ_VEC_SVC)) {
        vec = IRQ_VEC_SVC;
    } else if (n->pending & (1u << IRQ_VEC_SYSTICK)) {
        vec = IRQ_VEC_SYSTICK;
    } else if (n->pending & (1u << IRQ_VEC_PENDSV)) {
        vec = IRQ_VEC_PENDSV;
        n->scb_icsr &= ~(1 << 28);  /* clear PENDSVSET */
    }

    if (vec) {
        n->pending &= ~(1u << vec);
        take_interrupt(cpu, flash, ram, vec);
    }
}

/* Memory-mapped register access (0xE000ED00 range) */
int nvic_handles(uint32_t addr)
{
    return (addr >= 0xE000ED00 && addr <= 0xE000ED24);
}

uint32_t nvic_read(struct nvic *n, uint32_t addr)
{
    switch (addr) {
    case 0xE000ED04: return n->scb_icsr;
    case 0xE000ED08: return n->scb_vtor;
    case 0xE000ED20: return n->scb_shpr3;
    case 0xE000ED24: return n->scb_shcsr;
    default: return 0;
    }
}

void nvic_write(struct nvic *n, uint32_t addr, uint32_t val)
{
    switch (addr) {
    case 0xE000ED04: n->scb_icsr = val; break;
    case 0xE000ED08: n->scb_vtor = val; break;
    case 0xE000ED20: n->scb_shpr3 = val; break;
    case 0xE000ED24: n->scb_shcsr = val; break;
    }
}
