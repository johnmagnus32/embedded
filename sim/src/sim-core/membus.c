/*
 * membus.c — QEMU-inspired memory bus with function pointer dispatch
 */
#include <stdio.h>
#include <string.h>
#include "membus.h"
#include "cpu.h"

void membus_init(struct membus *bus, uint8_t *flash, uint8_t *ram)
{
    memset(bus, 0, sizeof(*bus));
    bus->flash = flash;
    bus->ram = ram;
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
    /* Fast path: flash */
    if (addr >= FLASH_BASE && addr < FLASH_BASE + FLASH_SIZE)
        return *(uint32_t *)(bus->flash + (addr - FLASH_BASE));
    /* Fast path: RAM */
    if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE)
        return *(uint32_t *)(bus->ram + (addr - RAM_BASE));
    /* Device dispatch */
    struct mem_region *r = find_region(bus, addr);
    if (r && r->read)
        return r->read(r->opaque, addr - r->base);
    warn_unmapped_read(addr);
    return 0;
}

void membus_write32(struct membus *bus, uint32_t addr, uint32_t val)
{
    if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE) {
        *(uint32_t *)(bus->ram + (addr - RAM_BASE)) = val;
        return;
    }
    struct mem_region *r = find_region(bus, addr);
    if (r && r->write) {
        r->write(r->opaque, addr - r->base, val);
        return;
    }
    warn_unmapped_write(addr, val);
}

uint16_t membus_read16(struct membus *bus, uint32_t addr)
{
    if (addr >= FLASH_BASE && addr < FLASH_BASE + FLASH_SIZE)
        return *(uint16_t *)(bus->flash + (addr - FLASH_BASE));
    if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE)
        return *(uint16_t *)(bus->ram + (addr - RAM_BASE));
    return (uint16_t)membus_read32(bus, addr);
}

void membus_write16(struct membus *bus, uint32_t addr, uint16_t val)
{
    if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE) {
        *(uint16_t *)(bus->ram + (addr - RAM_BASE)) = val;
        return;
    }
    membus_write32(bus, addr, val);
}

uint8_t membus_read8(struct membus *bus, uint32_t addr)
{
    if (addr >= FLASH_BASE && addr < FLASH_BASE + FLASH_SIZE)
        return bus->flash[addr - FLASH_BASE];
    if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE)
        return bus->ram[addr - RAM_BASE];
    return 0;
}

void membus_write8(struct membus *bus, uint32_t addr, uint8_t val)
{
    if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE) {
        bus->ram[addr - RAM_BASE] = val;
        return;
    }
    membus_write32(bus, addr, val);
}
