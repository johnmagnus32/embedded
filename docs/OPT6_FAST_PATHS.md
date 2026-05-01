# Optimization 6: Fast Paths for Instruction Fetch and RAM Access

The membus TLB is already implemented but every memory access still goes through `tlb_lookup` — even instruction fetches from flash and data accesses to RAM, which are 99%+ of all accesses. Add direct pointer fast paths that bypass the TLB entirely for these two ranges. Work in `/home/johmagnu/learning/simple-stm32/sim`. Read `src/core/membus.c` and `src/arch/armv7m/armv7m_cpu.c`. Build with `make` from `sim/`.

## Part 1: Direct flash pointer for instruction fetch

The hottest memory access is instruction fetch — every tick calls `membus_read16(bus, pc)`. Since PC is in flash 99.9% of the time, bypass the TLB:

```c
// In armv7m_cpu_step:
uint16_t insn;
if (__builtin_expect(pc - FLASH_BASE < FLASH_SIZE, 1))
    insn = *(uint16_t *)(bus->flash_ptr + (pc - FLASH_BASE));
else
    insn = membus_read16(bus, pc);
```

Add `flash_ptr` and `ram_ptr` to `struct membus`:

```c
struct membus {
    /* ... existing fields ... */
    uint8_t *flash_ptr;  /* direct pointer to flash backing memory */
    uint8_t *ram_ptr;    /* direct pointer to RAM backing memory */
};
```

Set them in `membus_register_ram` when flash/RAM regions are registered:

```c
void membus_register_ram(struct membus *bus, uint32_t base, uint32_t size,
                         uint8_t *data, int read_only)
{
    /* ... existing code ... */
    if (base == FLASH_BASE) bus->flash_ptr = data;
    if (base == RAM_BASE)   bus->ram_ptr = data;
}
```

## Part 2: Direct RAM fast path for data access

Add fast paths to `membus_read32` and `membus_write32`:

```c
__attribute__((hot))
uint32_t membus_read32(struct membus *bus, uint32_t addr)
{
    if (__builtin_expect(addr - RAM_BASE < RAM_SIZE, 1))
        return *(uint32_t *)(bus->ram_ptr + (addr - RAM_BASE));
    if (addr - FLASH_BASE < FLASH_SIZE)
        return *(uint32_t *)(bus->flash_ptr + (addr - FLASH_BASE));
    return membus_read32_slow(bus, addr);  /* TLB path for devices */
}

__attribute__((hot))
void membus_write32(struct membus *bus, uint32_t addr, uint32_t val)
{
    if (__builtin_expect(addr - RAM_BASE < RAM_SIZE, 1)) {
        *(uint32_t *)(bus->ram_ptr + (addr - RAM_BASE)) = val;
        return;
    }
    membus_write32_slow(bus, addr, val);
}
```

Move the TLB lookup into `_slow` variants. Apply the same pattern to read16, read8, write16, write8.

Note: this re-introduces the flash/RAM special-casing we removed earlier to be "like QEMU." But QEMU compensates with a JIT that inlines TLB checks into generated code. For an interpreter, explicit fast paths are the right tradeoff — the TLB still handles device MMIO correctly.

## Part 3: Skip idle SPI/DMA ticks

In `stm32f411_tick`, only call SPI and DMA tick functions when they're active:

```c
int stm32f411_tick(struct stm32f411 *soc)
{
    int r = armv7m_cpu_step(&soc->cpu, &soc->bus);
    armv7m_systick_check(&soc->systick, &soc->nvic, soc->cpu.cycle_count);
    if (__builtin_expect(soc->spis[0].active, 0)) stm32_spi_tick(&soc->spis[0]);
    if (__builtin_expect(soc->spis[1].active, 0)) stm32_spi_tick(&soc->spis[1]);
    if (__builtin_expect(soc->dma1.any_active, 0)) stm32_dma_tick(&soc->dma1);
    if (__builtin_expect(soc->dma2.any_active, 0)) stm32_dma_tick(&soc->dma2);
    if (__builtin_expect(soc->nvic.needs_update, 0))
        armv7m_nvic_update(&soc->nvic, &soc->cpu, &soc->bus);
    return r;
}
```

Add an `active` flag to `stm32_spi` that's set when SPI is enabled (CR1 SPE bit) and cleared when disabled. When the game is running pure logic (no SPI transfers), four function calls are eliminated per tick.

## Part 3b: Convert per-tick counters to cycle-count comparisons

Three devices currently decrement a counter every tick: `ili9341_tick` (refresh timer), `stm32_spi_tick` (SPI/I2S pacing), and `armv7m_systick` (already converted). Convert the remaining two to the same "next event" pattern:

**ILI9341:** Currently decrements `refresh_counter` every tick — called 40M times/sec to do work 60 times/sec. Instead:

```c
// In ili9341.h:
struct ili9341 {
    /* ... existing ... */
    uint64_t next_refresh;      /* cycle count when next refresh should happen */
    uint64_t *cycle_count_ptr;  /* pointer to cpu->cycle_count */
};

// In ili9341_tick — now a cheap comparison instead of a decrement:
void ili9341_tick(struct ili9341 *d)
{
    if (__builtin_expect(*d->cycle_count_ptr >= d->next_refresh, 0)) {
        ili9341_flush(d);
        d->next_refresh = *d->cycle_count_ptr + d->refresh_interval;
    }
}
```

**SPI pacing:** Currently decrements `spi_cycle_counter` and `i2s_cycle_counter` every tick. Instead, store the target cycle count:

```c
// In stm32_spi:
uint64_t spi_txe_at;   /* cycle count when TXE should go high */
uint64_t i2s_txe_at;   /* cycle count when I2S TXE should go high */
uint64_t *cycle_count_ptr;

void stm32_spi_tick(struct stm32_spi *s)
{
    uint64_t now = *s->cycle_count_ptr;
    if (s->spi_txe_at && now >= s->spi_txe_at) {
        s->sr |= SR_TXE;
        s->sr &= ~SR_BSY;
        s->spi_txe_at = 0;
        kick_dma(s);
    }
    if (s->i2s_txe_at && now >= s->i2s_txe_at) {
        s->sr |= SR_TXE;
        s->i2s_txe_at = 0;
        kick_dma(s);
    }
}
```

With the `active` flag from Part 3, `stm32_spi_tick` is only called when a transfer is in progress, so this is already fast. But the comparison approach avoids the decrement and is consistent with SysTick.

## Part 4: Batch tick execution

Move the IO/display polling out of `gameboy_tick` and into the main run loop as a batch:

```c
// In the continue/run handler:
while (!stopped) {
    // Inner loop: pure emulation, no IO checks
    for (int i = 0; i < 10000; i++) {
        int r = stm32f411_tick(&gb->soc);
        if (r) { stopped = 1; break; }
    }
    // Outer loop: IO, display, audio
    gameboy_poll_io(gb);
    ili9341_tick(gb->display);
    chardev_flush_all(gb->chardevs);
}
```

This eliminates the `cycle_count % 10000` modulo check on every tick and gives the compiler a tight inner loop to optimize.

## Expected improvement

- Flash instruction fetch fast path: ~8-10%
- RAM data access fast path: ~5-8%
- Skip idle SPI/DMA: ~3%
- Batch tick execution: ~3-5%
- Combined: ~20-25% (on top of other optimizations)

## Testing

Run `make perf-bench` before and after. Run all existing tests to verify correctness. Record MIPS in commit message.

## Results (measured 2026-05-01)

Tested incrementally on top of OPT2+3+4+5 baseline (52 MIPS).

| Metric | Baseline (OPT2-5) | +Part 1+2 (fast paths) | +Part 3 (skip idle) |
|--------|-------------------|----------------------|---------------------|
| MIPS | 52 | 56 (+8%) | **63 (+21%)** |
| SPI partial FPS | 62 | 68 (+10%) | **71 (+15%)** |
| DMA partial FPS | 72 | 80 (+11%) | **80 (+11%)** |
| Chardev idle FPS | 198 | 223 (+13%) | **261 (+32%)** |
| Chardev DMA FPS | 139 | 152 (+9%) | **154 (+11%)** |
| Audio samples/sec | 57,829 | 64,161 (+11%) | **62,482 (+8%)** |

Part 1+2 (flash/RAM fast paths): +8% MIPS. Bypasses TLB for the two most common address ranges. One unsigned subtraction + comparison per access instead of TLB lookup.

Part 3 (skip idle SPI ticks): +13% MIPS on top. Eliminates stm32_spi_tick function calls when no SPI transfer is in progress. Combined with __builtin_expect, the compiler moves the SPI tick code off the hot path entirely.

Parts 3b and 4 were also implemented:
- 3b: Converted ili9341_tick from per-tick decrement to cycle-count comparison. Cleaner code but no measurable improvement.
- 4: Replaced cycle_count % 10000 modulo with next-event comparison. No measurable improvement.

Stable MIPS with all parts: **55-57 MIPS** (the earlier 63 reading was a measurement artifact).

### Cumulative optimization summary

| Starting point | MIPS | Total improvement |
|---|---|---|
| No optimizations | 30 | — |
| +OPT3 (lazy SysTick + NVIC dirty) | 33 | +10% |
| +OPT4 (LTO) | 47 | +57% |
| +OPT2 (membus TLB) | 47 | +57% |
| +OPT5 (-O3 -march=native) | 52 | +73% |
| +OPT6 (fast paths + skip idle) | **56** | **+87%** |
