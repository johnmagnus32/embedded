/*
 * memslab.c — Fixed-size block allocator
 *
 * Each free block stores a pointer to the next free block (intrusive
 * free list). Alloc pops from the list, free pushes back.
 *
 * Block layout when free:
 *   [next_free_ptr][unused padding...]
 *
 * Block layout when allocated:
 *   [user data........................]
 */

#include "memslab.h"
#include <stdint.h>

static inline uint32_t irq_lock(void)
{
    uint32_t key;
    __asm volatile("mrs %0, primask\n cpsid i" : "=r"(key));
    return key;
}

static inline void irq_unlock(uint32_t key)
{
    __asm volatile("msr primask, %0" :: "r"(key));
}

void memslab_init(struct memslab *slab, void *buf,
                  size_t block_size, size_t num_blocks)
{
    slab->block_size = block_size;
    slab->num_blocks = num_blocks;
    slab->used = 0;

    /* Chain all blocks into a free list */
    uint8_t *p = (uint8_t *)buf;
    slab->free_list = (void **)p;

    for (size_t i = 0; i < num_blocks - 1; i++) {
        void **current = (void **)(p + i * block_size);
        *current = (void *)(p + (i + 1) * block_size);
    }
    /* Last block points to NULL */
    void **last = (void **)(p + (num_blocks - 1) * block_size);
    *last = (void *)0;
}

void *memslab_alloc(struct memslab *slab)
{
    uint32_t key = irq_lock();

    if (!slab->free_list) {
        irq_unlock(key);
        return (void *)0;
    }

    void *block = (void *)slab->free_list;
    slab->free_list = (void **)*slab->free_list;
    slab->used++;

    irq_unlock(key);
    return block;
}

void memslab_free(struct memslab *slab, void *block)
{
    if (!block) return;

    uint32_t key = irq_lock();

    void **bp = (void **)block;
    *bp = (void *)slab->free_list;
    slab->free_list = bp;
    slab->used--;

    irq_unlock(key);
}
