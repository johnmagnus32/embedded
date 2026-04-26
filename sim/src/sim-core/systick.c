/*
 * systick.c — SysTick Timer
 *
 * Counts CPU cycles. When the counter reaches the reload value,
 * it resets and raises an IRQ to the NVIC.
 */
#include "systick.h"
#include "nvic.h"

#define SYSTICK_BASE 0xE000E010

void systick_init(struct systick *st)
{
    st->csr = 0;
    st->rvr = 0;
    st->cvr = 0;
    st->counter = 0;
}

void systick_tick(struct systick *st, struct nvic *nvic)
{
    if (!(st->csr & 1)) return;  /* not enabled */

    st->counter++;
    if (st->counter >= st->rvr && st->rvr > 0) {
        st->counter = 0;
        if (st->csr & 2)  /* IRQ enabled */
            nvic_set_pending(nvic, IRQ_VEC_SYSTICK);
    }
}

int systick_handles(uint32_t addr)
{
    return (addr >= SYSTICK_BASE && addr < SYSTICK_BASE + 0x10);
}

uint32_t systick_read(struct systick *st, uint32_t addr)
{
    switch (addr) {
    case SYSTICK_BASE + 0x00: return st->csr;
    case SYSTICK_BASE + 0x04: return st->rvr;
    case SYSTICK_BASE + 0x08: return st->cvr;
    default: return 0;
    }
}

void systick_write(struct systick *st, uint32_t addr, uint32_t val)
{
    switch (addr) {
    case SYSTICK_BASE + 0x00: st->csr = val; break;
    case SYSTICK_BASE + 0x04: st->rvr = val; break;
    case SYSTICK_BASE + 0x08: st->cvr = val; break;
    }
}
