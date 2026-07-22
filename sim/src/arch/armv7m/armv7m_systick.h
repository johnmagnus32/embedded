#ifndef ARMV7M_SYSTICK_H
#define ARMV7M_SYSTICK_H

#include <stdint.h>

struct armv7m_nvic;

struct armv7m_systick {
    uint32_t csr;
    uint32_t rvr;
    uint32_t cvr;
    uint64_t counter;
    uint64_t next_fire;
};

void     armv7m_systick_init(struct armv7m_systick *st);
void     armv7m_systick_reload(struct armv7m_systick *st, uint64_t cycle);
uint32_t armv7m_systick_read(void *opaque, uint32_t offset);
void     armv7m_systick_write(void *opaque, uint32_t offset, uint32_t val);

static inline void armv7m_systick_check(struct armv7m_systick *st,
                                         struct armv7m_nvic *nvic,
                                         uint64_t cycle)
{
    if (__builtin_expect(cycle >= st->next_fire, 0)) {
        extern void armv7m_nvic_set_pending(struct armv7m_nvic *n, int vector);
        if (st->csr & 2) armv7m_nvic_set_pending(nvic, 15);
        st->next_fire = cycle + st->rvr;
        st->counter = 0;
    }
}

#endif
