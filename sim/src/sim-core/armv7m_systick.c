/*
 * armv7m_systick.c — ARMv7-M SysTick Timer
 */
#include "armv7m_systick.h"
#include "armv7m_nvic.h"

void armv7m_systick_init(struct armv7m_systick *st)
{
    st->csr = 0;
    st->rvr = 0;
    st->cvr = 0;
    st->counter = 0;
}

void armv7m_systick_tick(struct armv7m_systick *st, struct armv7m_nvic *nvic)
{
    if (!(st->csr & 1)) return;
    st->counter++;
    if (st->counter >= st->rvr && st->rvr > 0) {
        st->counter = 0;
        if (st->csr & 2)
            armv7m_nvic_set_pending(nvic, IRQ_VEC_SYSTICK);
    }
}

uint32_t armv7m_systick_read(void *opaque, uint32_t offset)
{
    struct armv7m_systick *st = (struct armv7m_systick *)opaque;
    switch (offset) {
    case 0x00: return st->csr;
    case 0x04: return st->rvr;
    case 0x08: return st->cvr;
    default:   return 0;
    }
}

void armv7m_systick_write(void *opaque, uint32_t offset, uint32_t val)
{
    struct armv7m_systick *st = (struct armv7m_systick *)opaque;
    switch (offset) {
    case 0x00: st->csr = val; break;
    case 0x04: st->rvr = val; break;
    case 0x08: st->cvr = val; break;
    }
}
