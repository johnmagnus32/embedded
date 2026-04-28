#ifndef MEMBUS_H
#define MEMBUS_H

#include <stdint.h>

typedef uint32_t (*mem_read_fn)(void *opaque, uint32_t offset);
typedef void (*mem_write_fn)(void *opaque, uint32_t offset, uint32_t val);

#define MAX_REGIONS 32

struct mem_region {
    uint32_t base;
    uint32_t size;
    mem_read_fn read;
    mem_write_fn write;
    void *opaque;
};

struct membus {
    struct mem_region regions[MAX_REGIONS];
    int nregions;
    uint8_t *flash;
    uint8_t *ram;
};

void     membus_init(struct membus *bus, uint8_t *flash, uint8_t *ram);
void     membus_register(struct membus *bus, uint32_t base, uint32_t size,
                         mem_read_fn read, mem_write_fn write, void *opaque);
uint32_t membus_read32(struct membus *bus, uint32_t addr);
void     membus_write32(struct membus *bus, uint32_t addr, uint32_t val);
uint16_t membus_read16(struct membus *bus, uint32_t addr);
void     membus_write16(struct membus *bus, uint32_t addr, uint16_t val);
uint8_t  membus_read8(struct membus *bus, uint32_t addr);
void     membus_write8(struct membus *bus, uint32_t addr, uint8_t val);

#endif
