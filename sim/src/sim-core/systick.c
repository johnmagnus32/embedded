/*
 * systick.c — SysTick Timer
 */
#include "systick.h"
#include "nvic.h"

void systick_init(struct systick *st)
{
    st->csr = 0;
    st->rvr = 0;
    st->cvr = 0;
    st->counter = 0;
}

void systick_tick(struct systick *st, struct nvic *nvic)
{
    if (!(st->csr & 1)) return;
    st->counter++;
    if (st->counter >= st->rvr && st->rvr > 0) {
        st->counter = 0;
        if (st->csr & 2)
            nvic_set_pending(nvic, IRQ_VEC_SYSTICK);
    }
}

uint32_t systick_read(void *opaque, uint32_t offset)
{
    struct systick *st = (struct systick *)opaque;
    switch (offset) {
    case 0x00: return st->csr;
    case 0x04: return st->rvr;
    case 0x08: return st->cvr;
    default:   return 0;
    }
}

void systick_write(void *opaque, uint32_t offset, uint32_t val)
{
    struct systick *st = (struct systick *)opaque;
    switch (offset) {
    case 0x00: st->csr = val; break;
    case 0x04: st->rvr = val; break;
    case 0x08: st->cvr = val; break;
    }
}
