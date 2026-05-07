/*
 * armv7m_systick.c — ARMv7-M SysTick Timer (event-queue driven)
 *
 * The SysTick read/write handlers receive the SoC pointer as opaque
 * so they can schedule/cancel events on the SoC's event queue.
 */
#include "armv7m_systick.h"
#include "armv7m_nvic.h"
#include "stm32f411.h"
#include "event_queue.h"

void armv7m_systick_init(struct armv7m_systick *st)
{
    st->csr = 0;
    st->rvr = 0;
    st->cvr = 0;
    st->counter = 0;
    st->next_fire = UINT64_MAX;
}

/* Forward declaration of the event callback (defined in stm32f411.c) */
extern void systick_event_cb(void *opaque);

uint32_t armv7m_systick_read(void *opaque, uint32_t offset)
{
    struct stm32f411 *soc = (struct stm32f411 *)opaque;
    struct armv7m_systick *st = &soc->systick;
    switch (offset) {
    case 0x00: return st->csr;
    case 0x04: return st->rvr;
    case 0x08: return st->cvr;
    default:   return 0;
    }
}

void armv7m_systick_write(void *opaque, uint32_t offset, uint32_t val)
{
    struct stm32f411 *soc = (struct stm32f411 *)opaque;
    struct armv7m_systick *st = &soc->systick;
    switch (offset) {
    case 0x00:
        st->csr = val;
        if ((val & 1) && st->rvr > 0) {
            /* Enable: schedule first SysTick event */
            event_schedule(&soc->eq, EVT_SYSTICK,
                           soc->cpu.cycle_count + st->rvr,
                           systick_event_cb, soc);
        } else {
            /* Disable: cancel pending event */
            event_cancel(&soc->eq, EVT_SYSTICK);
        }
        break;
    case 0x04: st->rvr = val; break;
    case 0x08: st->cvr = val; st->counter = 0; break;
    }
}
