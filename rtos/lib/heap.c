/*
 * heap.c — First-fit heap allocator with free-block coalescing
 *
 * Memory layout:
 *   [header][user data][header][user data]...[header (free, rest of heap)]
 *
 * Each block has a header:
 *   - size: total block size including header
 *   - free: 1 if available, 0 if allocated
 *   - next: pointer to next block (contiguous, not a free list)
 *
 * On free(), adjacent free blocks are merged (coalesced) to prevent
 * fragmentation. This is what Zephyr's sys_heap does (more efficiently
 * with a buddy allocator, but same concept).
 *
 * Thread-safe via irq_lock (same as our sync primitives).
 */

#include "heap.h"
#include <stdint.h>

struct block_header {
    size_t size;                /* total size including this header */
    uint8_t free;
    struct block_header *next;  /* next block in memory (not free list) */
};

#define HEADER_SIZE sizeof(struct block_header)
#define ALIGN_UP(x, a) (((x) + (a) - 1) & ~((a) - 1))

static struct block_header *heap_start;
static size_t total_size;

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

void heap_init(void *mem, size_t size)
{
    heap_start = (struct block_header *)mem;
    heap_start->size = size;
    heap_start->free = 1;
    heap_start->next = (void *)0;
    total_size = size;
}

void *heap_alloc(size_t size)
{
    size = ALIGN_UP(size, 4) + HEADER_SIZE;

    uint32_t key = irq_lock();

    struct block_header *curr = heap_start;

    while (curr) {
        if (curr->free && curr->size >= size) {
            /* Split block if there's enough room for another block */
            if (curr->size >= size + HEADER_SIZE + 4) {
                struct block_header *new_block =
                    (struct block_header *)((uint8_t *)curr + size);
                new_block->size = curr->size - size;
                new_block->free = 1;
                new_block->next = curr->next;
                curr->size = size;
                curr->next = new_block;
            }
            curr->free = 0;
            irq_unlock(key);
            return (void *)((uint8_t *)curr + HEADER_SIZE);
        }
        curr = curr->next;
    }

    irq_unlock(key);
    return (void *)0;  /* out of memory */
}

void heap_free(void *ptr)
{
    if (!ptr) return;

    uint32_t key = irq_lock();

    struct block_header *block =
        (struct block_header *)((uint8_t *)ptr - HEADER_SIZE);
    block->free = 1;

    /* Coalesce adjacent free blocks */
    struct block_header *curr = heap_start;
    while (curr && curr->next) {
        if (curr->free && curr->next->free) {
            curr->size += curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }

    irq_unlock(key);
}

size_t heap_free_bytes(void)
{
    size_t free = 0;
    struct block_header *curr = heap_start;
    while (curr) {
        if (curr->free) free += curr->size - HEADER_SIZE;
        curr = curr->next;
    }
    return free;
}

size_t heap_used_bytes(void)
{
    return total_size - heap_free_bytes() - HEADER_SIZE;
}
