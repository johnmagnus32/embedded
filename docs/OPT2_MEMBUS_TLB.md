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
