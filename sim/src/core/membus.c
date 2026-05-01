/*
 * membus.c — memory bus with fast paths for flash/RAM + TLB for devices
 *
 * Flash instruction fetch and RAM data access bypass the TLB entirely
 * via direct pointer arithmetic. Device MMIO goes through the TLB.
 */
#include <stdio.h>
#include <string.h>
#include "membus.h"

void membus_init(struct membus *bus)
{
    memset(bus, 0, sizeof(*bus));
}

static void hash_insert(struct membus *bus, struct mem_region *r)
{
    uint32_t lo = r->base >> 20;
    uint32_t hi = (r->base + r->size - 1) >> 20;
    for (uint32_t i = lo; i <= hi && i < MEMBUS_HASH_SIZE; i++)
        if (!bus->hash[i]) bus->hash[i] = r;
}

void membus_register(struct membus *bus, uint32_t base, uint32_t size,
                     mem_read_fn read, mem_write_fn write, void *opaque)
{
    if (bus->nregions >= MAX_REGIONS) return;
    struct mem_region *r = &bus->regions[bus->nregions++];
    r->base = base; r->size = size;
    r->read = read; r->write = write; r->opaque = opaque;
    r->direct = NULL; r->read_only = 0;
    hash_insert(bus, r);
    memset(bus->tlb, 0, sizeof(bus->tlb));
}

void membus_register_ram(struct membus *bus, uint32_t base, uint32_t size,
                         uint8_t *data, int read_only)
{
    if (bus->nregions >= MAX_REGIONS) return;
    struct mem_region *r = &bus->regions[bus->nregions++];
    r->base = base; r->size = size;
    r->read = NULL; r->write = NULL; r->opaque = NULL;
    r->direct = data; r->read_only = read_only;
    hash_insert(bus, r);
    memset(bus->tlb, 0, sizeof(bus->tlb));
    /* Set direct pointers for fast paths */
    if (base == MEMBUS_FLASH_BASE) bus->flash_ptr = data;
    if (base == MEMBUS_RAM_BASE)   bus->ram_ptr = data;
}

/* Slow path: TLB + hash + linear scan (for device MMIO) */
static struct mem_region *find_region_slow(struct membus *bus, uint32_t addr)
{
    struct mem_region *r = bus->hash[addr >> 20];
    if (r && addr >= r->base && addr < r->base + r->size) return r;
    for (int i = 0; i < bus->nregions; i++) {
        r = &bus->regions[i];
        if (addr >= r->base && addr < r->base + r->size) return r;
    }
    return NULL;
}

static inline struct mem_region *find_region_tlb(struct membus *bus, uint32_t addr)
{
    int idx = TLB_INDEX(addr);
    struct tlb_entry *te = &bus->tlb[idx];
    if (__builtin_expect(te->region != NULL, 1)) {
        struct mem_region *r = te->region;
        if (addr >= r->base && addr < r->base + r->size)
            return r;
    }
    struct mem_region *r = find_region_slow(bus, addr);
    if (r) te->region = r;
    return r;
}

static void warn_unmapped(uint32_t addr, int is_write, uint32_t val)
{
    static uint32_t warned[16]; static int nwarned;
    uint32_t page = addr & 0xFFFFF000;
    for (int i = 0; i < nwarned; i++) if (warned[i] == page) return;
    if (nwarned < 16) {
        warned[nwarned++] = page;
        if (is_write) fprintf(stderr, "[mem] Unhandled write at 0x%08X = 0x%08X\n", addr, val);
        else fprintf(stderr, "[mem] Unhandled read at 0x%08X\n", addr);
    }
}

/* ---- Fast-path read/write: RAM > flash > TLB ---- */

uint32_t membus_read32(struct membus *bus, uint32_t addr)
{
    if (__builtin_expect(addr - MEMBUS_RAM_BASE < MEMBUS_RAM_SIZE, 1))
        return *(uint32_t *)(bus->ram_ptr + (addr - MEMBUS_RAM_BASE));
    if (addr - MEMBUS_FLASH_BASE < MEMBUS_FLASH_SIZE)
        return *(uint32_t *)(bus->flash_ptr + (addr - MEMBUS_FLASH_BASE));
    struct mem_region *r = find_region_tlb(bus, addr);
    if (r) {
        if (r->direct) return *(uint32_t *)(r->direct + (addr - r->base));
        if (r->read) return r->read(r->opaque, addr - r->base);
    }
    warn_unmapped(addr, 0, 0);
    return 0;
}

void membus_write32(struct membus *bus, uint32_t addr, uint32_t val)
{
    if (__builtin_expect(addr - MEMBUS_RAM_BASE < MEMBUS_RAM_SIZE, 1)) {
        *(uint32_t *)(bus->ram_ptr + (addr - MEMBUS_RAM_BASE)) = val;
        return;
    }
    struct mem_region *r = find_region_tlb(bus, addr);
    if (r) {
        if (r->direct) { if (!r->read_only) *(uint32_t *)(r->direct + (addr - r->base)) = val; return; }
        if (r->write) { r->write(r->opaque, addr - r->base, val); return; }
    }
    warn_unmapped(addr, 1, val);
}

uint16_t membus_read16(struct membus *bus, uint32_t addr)
{
    if (__builtin_expect(addr - MEMBUS_RAM_BASE < MEMBUS_RAM_SIZE, 1))
        return *(uint16_t *)(bus->ram_ptr + (addr - MEMBUS_RAM_BASE));
    if (addr - MEMBUS_FLASH_BASE < MEMBUS_FLASH_SIZE)
        return *(uint16_t *)(bus->flash_ptr + (addr - MEMBUS_FLASH_BASE));
    struct mem_region *r = find_region_tlb(bus, addr);
    if (r) {
        if (r->direct) return *(uint16_t *)(r->direct + (addr - r->base));
        if (r->read) return (uint16_t)r->read(r->opaque, addr - r->base);
    }
    return 0;
}

void membus_write16(struct membus *bus, uint32_t addr, uint16_t val)
{
    if (__builtin_expect(addr - MEMBUS_RAM_BASE < MEMBUS_RAM_SIZE, 1)) {
        *(uint16_t *)(bus->ram_ptr + (addr - MEMBUS_RAM_BASE)) = val;
        return;
    }
    struct mem_region *r = find_region_tlb(bus, addr);
    if (r) {
        if (r->direct) { if (!r->read_only) *(uint16_t *)(r->direct + (addr - r->base)) = val; return; }
        if (r->write) { r->write(r->opaque, addr - r->base, val); return; }
    }
}

uint8_t membus_read8(struct membus *bus, uint32_t addr)
{
    if (__builtin_expect(addr - MEMBUS_RAM_BASE < MEMBUS_RAM_SIZE, 1))
        return bus->ram_ptr[addr - MEMBUS_RAM_BASE];
    if (addr - MEMBUS_FLASH_BASE < MEMBUS_FLASH_SIZE)
        return bus->flash_ptr[addr - MEMBUS_FLASH_BASE];
    struct mem_region *r = find_region_tlb(bus, addr);
    if (r) {
        if (r->direct) return r->direct[addr - r->base];
        if (r->read) return (uint8_t)r->read(r->opaque, addr - r->base);
    }
    return 0;
}

void membus_write8(struct membus *bus, uint32_t addr, uint8_t val)
{
    if (__builtin_expect(addr - MEMBUS_RAM_BASE < MEMBUS_RAM_SIZE, 1)) {
        bus->ram_ptr[addr - MEMBUS_RAM_BASE] = val;
        return;
    }
    struct mem_region *r = find_region_tlb(bus, addr);
    if (r) {
        if (r->direct) { if (!r->read_only) r->direct[addr - r->base] = val; return; }
        if (r->write) { r->write(r->opaque, addr - r->base, val); return; }
    }
}
