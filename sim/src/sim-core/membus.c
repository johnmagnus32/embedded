/*
 * membus.c — memory bus with direct-pointer fast path for RAM/flash regions
 */
#include <stdio.h>
#include <string.h>
#include "membus.h"

void membus_init(struct membus *bus)
{
    memset(bus, 0, sizeof(*bus));
}

void membus_register(struct membus *bus, uint32_t base, uint32_t size,
                     mem_read_fn read, mem_write_fn write, void *opaque)
{
    if (bus->nregions >= MAX_REGIONS) {
        fprintf(stderr, "[membus] Too many regions\n");
        return;
    }
    struct mem_region *r = &bus->regions[bus->nregions++];
    r->base = base;
    r->size = size;
    r->read = read;
    r->write = write;
    r->opaque = opaque;
    r->direct = NULL;
    r->read_only = 0;
}

void membus_register_ram(struct membus *bus, uint32_t base, uint32_t size,
                         uint8_t *data, int read_only)
{
    if (bus->nregions >= MAX_REGIONS) {
        fprintf(stderr, "[membus] Too many regions\n");
        return;
    }
    struct mem_region *r = &bus->regions[bus->nregions++];
    r->base = base;
    r->size = size;
    r->read = NULL;
    r->write = NULL;
    r->opaque = NULL;
    r->direct = data;
    r->read_only = read_only;
}

static struct mem_region *find_region(struct membus *bus, uint32_t addr)
{
    for (int i = 0; i < bus->nregions; i++) {
        struct mem_region *r = &bus->regions[i];
        if (addr >= r->base && addr < r->base + r->size)
            return r;
    }
    return NULL;
}

static void warn_unmapped_read(uint32_t addr)
{
    static uint32_t warned[16]; static int nwarned;
    uint32_t page = addr & 0xFFFFF000;
    for (int i = 0; i < nwarned; i++)
        if (warned[i] == page) return;
    if (nwarned < 16) {
        warned[nwarned++] = page;
        fprintf(stderr, "[mem] Unhandled read at 0x%08X (returning 0)\n", addr);
    }
}

static void warn_unmapped_write(uint32_t addr, uint32_t val)
{
    static uint32_t warned[16]; static int nwarned;
    uint32_t page = addr & 0xFFFFF000;
    for (int i = 0; i < nwarned; i++)
        if (warned[i] == page) return;
    if (nwarned < 16) {
        warned[nwarned++] = page;
        fprintf(stderr, "[mem] Unhandled write at 0x%08X = 0x%08X (ignored)\n", addr, val);
    }
}

uint32_t membus_read32(struct membus *bus, uint32_t addr)
{
    struct mem_region *r = find_region(bus, addr);
    if (r) {
        if (r->direct)
            return *(uint32_t *)(r->direct + (addr - r->base));
        if (r->read)
            return r->read(r->opaque, addr - r->base);
    }
    warn_unmapped_read(addr);
    return 0;
}

void membus_write32(struct membus *bus, uint32_t addr, uint32_t val)
{
    struct mem_region *r = find_region(bus, addr);
    if (r) {
        if (r->direct) {
            if (!r->read_only)
                *(uint32_t *)(r->direct + (addr - r->base)) = val;
            return;
        }
        if (r->write) {
            r->write(r->opaque, addr - r->base, val);
            return;
        }
    }
    warn_unmapped_write(addr, val);
}

uint16_t membus_read16(struct membus *bus, uint32_t addr)
{
    struct mem_region *r = find_region(bus, addr);
    if (r) {
        if (r->direct)
            return *(uint16_t *)(r->direct + (addr - r->base));
        if (r->read)
            return (uint16_t)r->read(r->opaque, addr - r->base);
    }
    return 0;
}

void membus_write16(struct membus *bus, uint32_t addr, uint16_t val)
{
    struct mem_region *r = find_region(bus, addr);
    if (r) {
        if (r->direct) {
            if (!r->read_only)
                *(uint16_t *)(r->direct + (addr - r->base)) = val;
            return;
        }
        if (r->write) {
            r->write(r->opaque, addr - r->base, val);
            return;
        }
    }
    warn_unmapped_write(addr, val);
}

uint8_t membus_read8(struct membus *bus, uint32_t addr)
{
    struct mem_region *r = find_region(bus, addr);
    if (r) {
        if (r->direct)
            return r->direct[addr - r->base];
        if (r->read)
            return (uint8_t)r->read(r->opaque, addr - r->base);
    }
    return 0;
}

void membus_write8(struct membus *bus, uint32_t addr, uint8_t val)
{
    struct mem_region *r = find_region(bus, addr);
    if (r) {
        if (r->direct) {
            if (!r->read_only)
                r->direct[addr - r->base] = val;
            return;
        }
        if (r->write) {
            r->write(r->opaque, addr - r->base, val);
            return;
        }
    }
    warn_unmapped_write(addr, val);
}
