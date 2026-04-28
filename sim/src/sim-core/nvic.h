#ifndef NVIC_H
#define NVIC_H

#include <stdint.h>

struct cpu_state;
struct membus;

#define IRQ_VEC_SVC     11
#define IRQ_VEC_PENDSV  14
#define IRQ_VEC_SYSTICK 15

struct nvic {
    uint32_t pending;
    uint32_t scb_icsr;
    uint32_t scb_shpr3;
    uint32_t scb_vtor;
    uint32_t scb_shcsr;
    uint32_t iser[3];
};

void nvic_init(struct nvic *n);
void nvic_set_pending(struct nvic *n, int vector);
void nvic_update(struct nvic *n, struct cpu_state *cpu, struct membus *bus);

/* Offset-based membus callbacks for SCB range (0xE000ED00, size 0x28) */
uint32_t nvic_scb_read(void *opaque, uint32_t offset);
void     nvic_scb_write(void *opaque, uint32_t offset, uint32_t val);

/* Offset-based membus callbacks for ISER range (0xE000E100, size 0x10) */
uint32_t nvic_iser_read(void *opaque, uint32_t offset);
void     nvic_iser_write(void *opaque, uint32_t offset, uint32_t val);

#endif
