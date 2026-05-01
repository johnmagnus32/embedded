# Optimization 7: Event-Driven Peripheral Scheduler (MAME-style)

Replace per-tick device polling with a sorted event queue. Instead of calling every device's tick function every CPU step, schedule events at specific cycle counts and only process them when due. Work in `/home/johmagnu/learning/simple-stm32/sim`. Read `src/soc/stm32f411.c`, `src/arch/armv7m/armv7m_systick.c`, `src/hw/stm32/stm32_spi.c`, `src/devices/ili9341.c`, and `src/hw/stm32/stm32_dma.c` before making changes. Build with `make` from `sim/`.

## Problem

`stm32f411_tick` currently checks every device every tick:

```c
int stm32f411_tick(struct stm32f411 *soc)
{
    int r = armv7m_cpu_step(&soc->cpu, &soc->bus);
    armv7m_systick_check(...);     // comparison every tick
    stm32_spi_tick(&soc->spis[0]); // function call every tick
    stm32_spi_tick(&soc->spis[1]); // function call every tick
    stm32_dma_tick(&soc->dma1);    // function call every tick
    stm32_dma_tick(&soc->dma2);    // function call every tick
    if (soc->nvic.needs_update)    // branch every tick
        armv7m_nvic_update(...);
    return r;
}
```

Even with `active` guards and `__builtin_expect`, that's 6+ branches per tick at 40M ticks/sec = 240M branch predictions/sec. Most of the time nothing fires.

## Solution

A central event queue. Each device schedules "call me at cycle N" instead of being polled. The tick loop does one comparison per tick against the earliest event.

## Part 1: Event queue

Create `src/core/event_queue.h` and `src/core/event_queue.c`:

```c
#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

#include <stdint.h>

typedef void (*event_callback_t)(void *opaque);

#define EVENT_QUEUE_MAX 32

struct event {
    uint64_t fire_at;
    event_callback_t callback;
    void *opaque;
    int id;           /* for cancellation: unique per event source */
    int active;
};

struct event_queue {
    struct event events[EVENT_QUEUE_MAX];
    int count;
    uint64_t next_event;  /* cached: earliest fire_at across all active events */
};

void event_queue_init(struct event_queue *eq);

/* Schedule an event at a specific cycle count.
 * If an event with the same id exists, it's replaced (rescheduled).
 * id should be unique per event source (e.g., SYSTICK=1, SPI0=2, etc.) */
void event_schedule(struct event_queue *eq, int id, uint64_t fire_at,
                    event_callback_t callback, void *opaque);

/* Cancel a pending event by id. */
void event_cancel(struct event_queue *eq, int id);

/* Process all events that are due. Call from the tick loop. */
void event_process(struct event_queue *eq, uint64_t current_cycle);

/* Recompute next_event cache (call after schedule/cancel). */
void event_update_next(struct event_queue *eq);

#endif
```

Implementation:

```c
void event_queue_init(struct event_queue *eq)
{
    memset(eq, 0, sizeof(*eq));
    eq->next_event = UINT64_MAX;
}

void event_schedule(struct event_queue *eq, int id, uint64_t fire_at,
                    event_callback_t callback, void *opaque)
{
    /* Find existing event with this id, or an empty slot */
    struct event *slot = NULL;
    for (int i = 0; i < EVENT_QUEUE_MAX; i++) {
        if (eq->events[i].active && eq->events[i].id == id) {
            slot = &eq->events[i];
            break;
        }
    }
    if (!slot) {
        for (int i = 0; i < EVENT_QUEUE_MAX; i++) {
            if (!eq->events[i].active) { slot = &eq->events[i]; break; }
        }
    }
    if (!slot) return;  /* queue full */

    slot->fire_at = fire_at;
    slot->callback = callback;
    slot->opaque = opaque;
    slot->id = id;
    slot->active = 1;

    if (fire_at < eq->next_event)
        eq->next_event = fire_at;
}

void event_cancel(struct event_queue *eq, int id)
{
    for (int i = 0; i < EVENT_QUEUE_MAX; i++) {
        if (eq->events[i].active && eq->events[i].id == id) {
            eq->events[i].active = 0;
            break;
        }
    }
    event_update_next(eq);
}

void event_process(struct event_queue *eq, uint64_t current_cycle)
{
    for (int i = 0; i < EVENT_QUEUE_MAX; i++) {
        struct event *e = &eq->events[i];
        if (e->active && current_cycle >= e->fire_at) {
            e->active = 0;
            e->callback(e->opaque);
            /* callback may schedule new events, so don't break */
        }
    }
    event_update_next(eq);
}

void event_update_next(struct event_queue *eq)
{
    eq->next_event = UINT64_MAX;
    for (int i = 0; i < EVENT_QUEUE_MAX; i++) {
        if (eq->events[i].active && eq->events[i].fire_at < eq->next_event)
            eq->next_event = eq->events[i].fire_at;
    }
}
```

## Part 2: Event IDs

Define unique IDs for each event source:

```c
enum event_id {
    EVT_SYSTICK = 1,
    EVT_SPI0_TXE,
    EVT_SPI1_TXE,
    EVT_ILI9341_REFRESH,
    EVT_DMA1_STREAM0,  /* through EVT_DMA1_STREAM7 */
    /* ... */
};
```

## Part 3: Convert SysTick

Currently uses its own `next_fire` field. Move to the event queue:

```c
// Old (in armv7m_systick.c):
void armv7m_systick_check(struct armv7m_systick *st, struct armv7m_nvic *nvic, uint64_t cycle)
{
    if (cycle >= st->next_fire) {
        armv7m_nvic_set_pending(nvic, IRQ_VEC_SYSTICK);
        st->next_fire = cycle + st->rvr;
    }
}

// New: SysTick callback
static void systick_event_cb(void *opaque)
{
    struct stm32f411 *soc = opaque;
    if (soc->systick.csr & 2)
        armv7m_nvic_set_pending(&soc->nvic, IRQ_VEC_SYSTICK);
    /* Reschedule */
    event_schedule(&soc->eq, EVT_SYSTICK,
                   soc->cpu.cycle_count + soc->systick.rvr,
                   systick_event_cb, soc);
}

// When firmware enables SysTick (writes CSR):
event_schedule(&soc->eq, EVT_SYSTICK,
               soc->cpu.cycle_count + soc->systick.rvr,
               systick_event_cb, soc);

// When firmware disables SysTick:
event_cancel(&soc->eq, EVT_SYSTICK);
```

## Part 4: Convert SPI pacing

```c
// Old (in stm32_spi_tick):
if (s->spi_cycle_counter > 0) {
    s->spi_cycle_counter--;
    if (s->spi_cycle_counter == 0) { s->sr |= SR_TXE; kick_dma(s); }
}

// New: when DR is written (SPI transfer starts):
event_schedule(eq, EVT_SPI0_TXE,
               cpu->cycle_count + s->spi_cycles_per_byte,
               spi0_txe_callback, s);

// Callback:
static void spi0_txe_callback(void *opaque)
{
    struct stm32_spi *s = opaque;
    s->sr |= SR_TXE;
    s->sr &= ~SR_BSY;
    kick_dma(s);
}
```

No more `stm32_spi_tick` function. SPI only touches the event queue when a transfer starts.

## Part 5: Convert ILI9341 refresh

```c
// Old (in ili9341_tick):
if (d->refresh_counter > 0) d->refresh_counter--;
if (d->refresh_counter == 0) { ili9341_flush(d); d->refresh_counter = d->refresh_interval; }

// New: schedule refresh at init
event_schedule(eq, EVT_ILI9341_REFRESH,
               cpu->cycle_count + d->refresh_interval,
               ili9341_refresh_callback, d);

static void ili9341_refresh_callback(void *opaque)
{
    struct ili9341 *d = opaque;
    ili9341_flush(d);
    /* Reschedule next refresh */
    event_schedule(eq, EVT_ILI9341_REFRESH,
                   *d->cycle_count_ptr + d->refresh_interval,
                   ili9341_refresh_callback, d);
}
```

No more `ili9341_tick` function.

## Part 6: Convert DMA

DMA streams that use per-tick transfers (non-externally-driven) schedule their next transfer as an event. Externally-driven streams (audio) remain wall-clock paced.

## Part 7: Update the tick loop

Add the event queue to `struct stm32f411`:

```c
struct stm32f411 {
    /* ... existing ... */
    struct event_queue eq;
};
```

The tick loop becomes:

```c
int stm32f411_tick(struct stm32f411 *soc)
{
    int r = armv7m_cpu_step(&soc->cpu, &soc->bus);

    /* One comparison — replaces all per-device checks */
    if (__builtin_expect(soc->cpu.cycle_count >= soc->eq.next_event, 0))
        event_process(&soc->eq, soc->cpu.cycle_count);

    /* NVIC still needs per-tick check (interrupts can be set by CPU instructions) */
    if (__builtin_expect(soc->nvic.needs_update, 0))
        armv7m_nvic_update(&soc->nvic, &soc->cpu, &soc->bus);

    return r;
}
```

From 6+ checks per tick down to 2 (event queue + NVIC). The event queue check is almost always false (next event is thousands of cycles away), so the branch predictor nails it.

## Expected improvement

The per-tick overhead drops from ~6 function calls + branches to 1 comparison. At 40M ticks/sec:
- Old: 240M+ branch predictions/sec for device checks
- New: 40M branch predictions/sec (one per tick, almost always not-taken)

Expected: ~10-15% MIPS improvement. Compounds with other optimizations. More importantly, adding new devices in the future has zero per-tick cost — they just schedule events.

## Testing

Run all existing tests — SysTick timing, IRQ latency, SPI throughput must all still pass. Run `make perf-bench` before and after. Record MIPS in commit message.

## Readability

The code actually gets cleaner:
- No more `*_tick` functions with counter decrement boilerplate
- Device behavior is "when X happens, schedule Y at cycle N"
- The tick loop is trivially simple
- Adding a new timed device = one `event_schedule` call, one callback function

## Results (measured 2026-05-01)

Converted SysTick, SPI pacing (both normal and I2S), ILI9341 refresh, and IO polling to event queue. Removed `stm32_spi_tick` and `ili9341_tick` functions entirely. The `gameboy_tick` function is now a single line: `return stm32f411_tick(&b->soc)`.

Tested on top of OPT1-6 baseline (62 MIPS).

| Metric | Before (OPT1-6) | After (+OPT7) | Change |
|--------|-----------------|---------------|--------|
| MIPS | 62 | 71 | +15% |
| Chardev idle FPS | 215 | 312 | +45% |
| Audio samples/sec | 63,698 | 83,389 | +31% |
| SPI partial FPS | 64 | 63 | — |
| DMA partial FPS | 78 | 76 | — |

The tick loop went from 6+ device checks per tick to 1 event queue comparison. The event queue check is almost always false (next event is hundreds or thousands of cycles away), so the branch predictor handles it perfectly.

### Cumulative optimization summary (all OPTs applied)

| Starting point | MIPS | Total improvement |
|---|---|---|
| No optimizations | 30 | — |
| +OPT3 (lazy SysTick + NVIC dirty) | 33 | +10% |
| +OPT4 (LTO) | 47 | +57% |
| +OPT2 (membus TLB) | 47 | +57% |
| +OPT5 (-O3 -march=native) | 52 | +73% |
| +OPT6 (fast paths + skip idle) | 56 | +87% |
| +OPT1 (computed goto dispatch) | 62 | +107% |
| +OPT7 (event queue) | **71** | **+137%** |
