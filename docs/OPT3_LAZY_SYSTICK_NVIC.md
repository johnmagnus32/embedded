# Optimization 3: Lazy SysTick and NVIC

SysTick and NVIC are called every tick even when nothing changed (~8% host CPU). Make them lazy. Work in `/home/johmagnu/learning/simple-stm32/sim/mcu`. Read `src/arch/armv7m/armv7m_systick.c` and `armv7m_nvic.c` before making changes. Build with `make` from `sim/mcu/`.

## SysTick: countdown instead of increment

Instead of incrementing a counter every tick, compute when the next event fires:

```c
struct armv7m_systick {
    uint32_t csr, rvr, cvr;
    uint64_t next_fire;  /* cycle count when SysTick fires */
};

static inline void armv7m_systick_check(struct armv7m_systick *st,
                                         struct armv7m_nvic *nvic,
                                         uint64_t cycle)
{
    if (cycle >= st->next_fire) {
        if (st->csr & 2) armv7m_nvic_set_pending(nvic, IRQ_VEC_SYSTICK);
        st->next_fire = cycle + st->rvr;
    }
}
```

One comparison per tick. When disabled, `next_fire = UINT64_MAX` — always false.

Call `armv7m_systick_reload` when firmware writes CSR or RVR. Compute CVR reads from `next_fire - current_cycle`.

## NVIC: dirty flag

Only call `armv7m_nvic_update` when something changed:

```c
void armv7m_nvic_set_pending(struct armv7m_nvic *n, int vector)
{
    n->pending |= (1u << vector);
    n->needs_update = 1;
}

// In tick loop:
if (soc->nvic.needs_update)
    armv7m_nvic_update(&soc->nvic, &soc->cpu, &soc->bus);
```

Also set `needs_update` when: firmware writes SCB_ICSR (PENDSVSET), CPSIE clears primask, exception return clears in_handler.

## Expected improvement

~5-6% (40 → ~43 MIPS). Modest alone but compounds with other optimizations.

## Testing

Run `make perf-bench` before and after. Verify SysTick still fires correctly (check UART timing). Record MIPS in commit message.

## Results (measured 2026-05-01)

| Metric | Baseline | OPT3 only | OPT3 + LTO |
|--------|----------|-----------|------------|
| MIPS | 30 | 33 (+10%) | 47 (+57%) |
| DMA partial FPS | 30 | 49 (+63%) | 57 (+90%) |
| Chardev idle FPS | 128 | 142 (+11%) | 189 (+48%) |

OPT3 alone: modest MIPS gain but large DMA FPS improvement (NVIC dirty flag eliminates update call during DMA transfers). Compounds well with LTO.
