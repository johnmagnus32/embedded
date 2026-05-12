/*
 * irq_dispatch.c — Two-level interrupt dispatcher
 *
 * All 82 peripheral IRQ vectors point to _isr_wrapper.
 * This function reads the active IRQ number from the IPSR register,
 * indexes the software ISR table, and calls the registered handler
 * with its argument (typically a device pointer).
 */

#include "irq.h"
#include "devicetree_irq.h"

void _isr_wrapper(void)
{
    uint32_t ipsr;
    __asm volatile ("mrs %0, ipsr" : "=r"(ipsr));
    int irq = (int)ipsr - 16;

    if (irq >= 0 && irq < NUM_IRQS) {
        isr_fn fn = _sw_isr_table[irq].isr;
        if (fn)
            fn(_sw_isr_table[irq].arg);
    }
}
