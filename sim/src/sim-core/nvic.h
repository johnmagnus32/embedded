#ifndef NVIC_H
#define NVIC_H

#include <stdint.h>

struct cpu_state;

/* Interrupt vector numbers */
#define IRQ_VEC_SVC     11
#define IRQ_VEC_PENDSV  14
#define IRQ_VEC_SYSTICK 15

struct nvic {
    uint32_t pending;     /* bitmask of pending IRQ lines */
    uint32_t scb_icsr;    /* SCB Interrupt Control and State Register */
    uint32_t scb_shpr3;   /* SCB System Handler Priority Register 3 */
    uint32_t scb_vtor;    /* Vector Table Offset Register */
    uint32_t scb_shcsr;   /* System Handler Control and State Register */
};

void nvic_init(struct nvic *n);
void nvic_set_pending(struct nvic *n, int vector);
void nvic_update(struct nvic *n, struct cpu_state *cpu, uint8_t *flash, uint8_t *ram);

/* Memory-mapped register access */
uint32_t nvic_read(struct nvic *n, uint32_t addr);
void     nvic_write(struct nvic *n, uint32_t addr, uint32_t val);
int      nvic_handles(uint32_t addr);

#endif
