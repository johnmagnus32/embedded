#ifndef MEMBUS_H
#define MEMBUS_H

#include <stdint.h>

typedef uint32_t (*mem_read_fn)(void *opaque, uint32_t offset);
typedef void (*mem_write_fn)(void *opaque, uint32_t offset, uint32_t val);

#define MAX_REGIONS 32

/* Memory map constants for fast paths */
#define MEMBUS_FLASH_BASE  0x08000000
#define MEMBUS_FLASH_SIZE  (512 * 1024)
#define MEMBUS_RAM_BASE    0x20000000
#define MEMBUS_RAM_SIZE    (128 * 1024)

struct mem_region {
    uint32_t base;
    uint32_t size;
    mem_read_fn read;
    mem_write_fn write;
    void *opaque;
    uint8_t *direct;
    int read_only;
};

#define MEMBUS_HASH_BITS 12
#define MEMBUS_HASH_SIZE (1 << MEMBUS_HASH_BITS)

/* Direct-mapped TLB for fast region lookup */
#define TLB_BITS 12
#define TLB_SIZE (1 << TLB_BITS)
#define TLB_MASK (TLB_SIZE - 1)
/* XOR-fold address to avoid flash/RAM collision */
#define TLB_INDEX(addr) (((addr) >> 12 ^ (addr) >> 20) & TLB_MASK)

struct tlb_entry {
    struct mem_region *region;  /* last region accessed at this index */
};

struct membus {
    struct mem_region regions[MAX_REGIONS];
    int nregions;
    struct mem_region *hash[MEMBUS_HASH_SIZE];
    struct tlb_entry tlb[TLB_SIZE];
    uint8_t *flash_ptr;  /* direct pointer to flash backing memory */
    uint8_t *ram_ptr;    /* direct pointer to RAM backing memory */
};

void     membus_init(struct membus *bus);
void     membus_register(struct membus *bus, uint32_t base, uint32_t size,
                         mem_read_fn read, mem_write_fn write, void *opaque);
void     membus_register_ram(struct membus *bus, uint32_t base, uint32_t size,
                             uint8_t *data, int read_only);
uint32_t membus_read32(struct membus *bus, uint32_t addr);
void     membus_write32(struct membus *bus, uint32_t addr, uint32_t val);
uint16_t membus_read16(struct membus *bus, uint32_t addr);
void     membus_write16(struct membus *bus, uint32_t addr, uint16_t val);
uint8_t  membus_read8(struct membus *bus, uint32_t addr);
void     membus_write8(struct membus *bus, uint32_t addr, uint8_t val);

#endif
