#ifndef ARMV7M_NVIC_H
#define ARMV7M_NVIC_H

#include <stdint.h>

struct cpu_state;
struct membus;

#define IRQ_VEC_SVC     11
#define IRQ_VEC_PENDSV  14
#define IRQ_VEC_SYSTICK 15

struct armv7m_nvic {
    uint32_t pending;
    uint32_t scb_icsr;
    uint32_t scb_shpr3;
    uint32_t scb_vtor;
    uint32_t scb_shcsr;
    uint32_t iser[3];
};

void armv7m_nvic_init(struct armv7m_nvic *n);
void armv7m_nvic_set_pending(struct armv7m_nvic *n, int vector);
void armv7m_nvic_update(struct armv7m_nvic *n, struct cpu_state *cpu, struct membus *bus);

uint32_t armv7m_nvic_scb_read(void *opaque, uint32_t offset);
void     armv7m_nvic_scb_write(void *opaque, uint32_t offset, uint32_t val);

uint32_t armv7m_nvic_iser_read(void *opaque, uint32_t offset);
void     armv7m_nvic_iser_write(void *opaque, uint32_t offset, uint32_t val);

#endif
