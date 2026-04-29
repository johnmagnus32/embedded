#ifndef ARMV7M_SYSTICK_H
#define ARMV7M_SYSTICK_H

#include <stdint.h>

struct armv7m_nvic;

struct armv7m_systick {
    uint32_t csr;
    uint32_t rvr;
    uint32_t cvr;
    uint64_t counter;
};

void     armv7m_systick_init(struct armv7m_systick *st);
void     armv7m_systick_tick(struct armv7m_systick *st, struct armv7m_nvic *nvic);
uint32_t armv7m_systick_read(void *opaque, uint32_t offset);
void     armv7m_systick_write(void *opaque, uint32_t offset, uint32_t val);

#endif
