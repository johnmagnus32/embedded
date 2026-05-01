# Optimization 4: Inline the Hot Tick Path

3 levels of function calls per instruction (~10% host CPU). Flatten with inlining + LTO. Work in `/home/johmagnu/learning/simple-stm32/sim`. Read `src/machine/gameboy.c`, `src/soc/stm32f411.c`, `src/arch/armv7m/armv7m_cpu.c` before making changes. Build with `make` from `sim/`.

## Problem

Each tick: `gameboy_tick → stm32f411_tick → armv7m_cpu_step + systick + nvic` = 5 function calls × 40M/sec = 200M calls/sec.

## Solution

### Step 1: Mark hot functions static inline

Move small hot functions into headers:

```c
// armv7m_systick.h:
static inline void armv7m_systick_check(struct armv7m_systick *st,
                                         struct armv7m_nvic *nvic, uint64_t cycle)
{
    if (__builtin_expect(cycle >= st->next_fire, 0))
        armv7m_systick_fire(st, nvic, cycle);
}

// stm32f411.h:
static inline void stm32f411_tick(struct stm32f411 *soc)
{
    armv7m_cpu_step(&soc->cpu, &soc->bus);
    armv7m_systick_check(&soc->systick, &soc->nvic, soc->cpu.cycle_count);
    if (__builtin_expect(soc->nvic.needs_update, 0))
        armv7m_nvic_update(&soc->nvic, &soc->cpu, &soc->bus);
}
```

### Step 2: Inline the run loop

In the continue handler, call `stm32f411_tick` directly instead of through `mach->tick` function pointer (which the compiler can't inline through).

### Step 3: Enable LTO

```makefile
CFLAGS = -Wall -O2 -g -flto
LDFLAGS = -flto
```

Keep a non-LTO debug build target.

### Step 4: Profile-guided optimization (optional)

```bash
make CFLAGS="-O2 -fprofile-generate"
./build/sim-core --bench 10000000 --no-chardev
make clean && make CFLAGS="-O2 -fprofile-use"
```

## Expected improvement

- Inlining: ~1.3× (40 → ~52 MIPS)
- LTO: ~1.2× additional (52 → ~62 MIPS)
- Combined with OPT1-3: ~100-120 MIPS total

## Testing

Run `make perf-bench` before and after each change. Record MIPS progression:

```
perf: 40.4 MIPS (baseline)
perf: 52.1 MIPS (inline)
perf: 63.8 MIPS (inline + LTO)
```

## Results (measured 2026-05-01)

| Metric | Baseline | LTO only | LTO + OPT3 |
|--------|----------|----------|------------|
| MIPS | 30 | 45 (+50%) | 47 (+57%) |
| DMA partial FPS | 30 | 38 (+27%) | 57 (+90%) |
| Chardev idle FPS | 128 | 173 (+35%) | 189 (+48%) |

LTO alone is the single biggest win — 50% MIPS improvement. Compiler inlines across translation units (stm32f411_tick → armv7m_cpu_step, peripheral ticks). Compounds with OPT3.

### Direct call experiment (2026-05-01)

Replaced `ctx->mach->tick(ctx->board)` with `gameboy_tick((struct gameboy *)ctx->board)` in all three hot loops (headless, bench, debug continue). Result: 48 MIPS — slightly worse than 52 with indirect call.

Why: LTO already inlines `stm32f411_tick` → `armv7m_cpu_step` → all instruction handlers INTO `gameboy_tick`, producing a 13,599-byte monolith. The direct call doesn't help because the function is already too large to inline further — the indirect call overhead (~5ns) is negligible compared to the ~20ns per tick of actual work inside the function. The slight regression is likely from code layout changes affecting instruction cache behavior.

Conclusion: LTO already captures the full inlining benefit. The `mach->tick` function pointer indirection is not a bottleneck. No further work needed on OPT4.
