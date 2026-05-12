/*
 * irq.h — Two-level interrupt dispatch infrastructure
 *
 * All peripheral IRQs go through _isr_wrapper, which reads the active
 * IRQ number from IPSR and dispatches via a software table. This allows
 * drivers to register ISRs with a device pointer argument, and makes
 * IRQ assignments a device-tree-only concern.
 */

#ifndef IRQ_H
#define IRQ_H

#include <stdint.h>

#define NUM_IRQS 82

typedef void (*isr_fn)(void *arg);

struct isr_table_entry {
    isr_fn isr;
    void  *arg;
};

/* Generated table — defined in devicetree_irq.h */
extern const struct isr_table_entry _sw_isr_table[NUM_IRQS];

static inline void irq_enable(uint8_t irq)
{
    volatile uint32_t *iser = (volatile uint32_t *)(0xE000E100 + (irq / 32) * 4);
    *iser = (1 << (irq % 32));
}

static inline void irq_disable(uint8_t irq)
{
    volatile uint32_t *icer = (volatile uint32_t *)(0xE000E180 + (irq / 32) * 4);
    *icer = (1 << (irq % 32));
}

#endif
