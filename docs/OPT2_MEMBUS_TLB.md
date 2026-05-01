# Optimization 2: Direct-Mapped TLB for membus

The membus `find_region` linear scan accounts for ~16% of host CPU. Replace with a direct-mapped cache. Work in `/home/johmagnu/learning/simple-stm32/sim`. Read `src/core/membus.c` before making changes. Build with `make` from `sim/`.

## Problem

Every memory access iterates ~15 regions. At ~40M accesses/sec = ~600M comparisons/sec.

## Solution

256-entry TLB indexed by address bits [19:12]:

```c
#define TLB_BITS 8
#define TLB_SIZE (1 << TLB_BITS)
#define TLB_MASK (TLB_SIZE - 1)

struct tlb_entry {
    uint32_t tag;              /* page-aligned address */
    struct mem_region *region;
};

struct membus {
    struct mem_region regions[MAX_REGIONS];
    int nregions;
    struct tlb_entry tlb[TLB_SIZE];
};
```

### Lookup

```c
uint32_t membus_read32(struct membus *bus, uint32_t addr)
{
    int idx = (addr >> 12) & TLB_MASK;
    struct tlb_entry *te = &bus->tlb[idx];

    if (te->tag == (addr & 0xFFFFF000) && te->region) {
        struct mem_region *r = te->region;
        if (r->direct)
            return *(uint32_t *)(r->direct + (addr - r->base));
        return r->read(r->opaque, addr - r->base);
    }

    // TLB miss — full lookup, cache result
    struct mem_region *r = find_region(bus, addr);
    if (r) {
        te->tag = addr & 0xFFFFF000;
        te->region = r;
        if (r->direct)
            return *(uint32_t *)(r->direct + (addr - r->base));
        return r->read(r->opaque, addr - r->base);
    }
    return 0;
}
```

Apply to all membus_read/write variants. Flush TLB in `membus_register`.

### Why it works

Flash and RAM are large contiguous regions. Instruction fetches hit the same few pages repeatedly. >99% hit rate expected. Hot path becomes ~3 host instructions instead of ~30.

## Expected improvement

~1.5× on membus-heavy workloads.

## Testing

Run `make perf-bench` before and after. Record MIPS in commit message.

## Results (measured 2026-05-01)

Tested on top of OPT3 + OPT4 (LTO) baseline (47 MIPS).

First attempt failed: 256-entry TLB with page-tag matching caused 3 test failures. Root cause: multiple MMIO regions share the same 4KB page (SysTick at 0xE000E010, SCB at 0xE000ED00). The TLB cached one region per page, so a SysTick access would evict the SCB entry and subsequent SCB writes went to the wrong handler.

Fix: replaced page-tag matching with region bounds validation. The TLB entry caches a region pointer, and the hit check verifies `addr >= region->base && addr < region->base + region->size`. Slightly more work per hit (two comparisons vs one) but correct for all address layouts.

Also increased TLB from 256 to 4096 entries and used XOR-folded index `(addr >> 12) ^ (addr >> 20)` to avoid flash/RAM collision.

| Metric | OPT3+LTO | +OPT2 (TLB) |
|--------|----------|-------------|
| MIPS | 47 | 46 (no change) |
| DMA partial FPS | 57 | 69 (+21%) |
| Chardev idle FPS | 189 | 185 |

MIPS unchanged (instruction fetch already hits the hash table on first try for flash). DMA FPS improved 21% because DMA does many small MMIO writes (SPI DR) that benefit from TLB caching. All 35 tests pass.

### Full peripheral benchmark comparison

| Metric | OPT3+LTO (no TLB) | +OPT2 (TLB) | Change |
|--------|-------------------|-------------|--------|
| MIPS | 47 | 47 | — |
| SPI full frame FPS | 8 | 11 | +38% |
| SPI partial FPS | 42 | 59 | +40% |
| DMA partial FPS | 57 | 71 | +25% |
| DMA full frame FPS | 6 | 15 | +150% |
| I2S CPU (1000 samples) | 23ms | 16ms | +44% faster |
| I2S DMA (512 samples) | 13ms | 8ms | +63% faster |
| Chardev CPU display | 113 FPS | 154 FPS | +36% |
| Chardev DMA display | 59 FPS | 136 FPS | +130% |
| Audio chardev | 42,221 samp/s | 57,951 samp/s | +37% |

Every peripheral-heavy workload improved significantly. The TLB eliminates the linear region scan (~15 iterations) for repeated accesses to the same MMIO register (SPI_DR, DMA status, GPIO). MIPS unchanged because instruction fetch already hits the hash table directly for flash addresses. All 35 tests pass.
