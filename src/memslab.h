/*
 * memslab.h — Fixed-size block allocator (memory slab)
 *
 * Like Zephyr's k_mem_slab — allocates blocks of a fixed size.
 * No fragmentation (all blocks are the same size).
 * O(1) alloc and free (just pop/push from a free list).
 *
 * Use when you need many objects of the same size (network buffers,
 * message structs, sensor readings, etc.)
 */

#ifndef MEMSLAB_H
#define MEMSLAB_H

#include <stddef.h>
#include <stdint.h>

struct memslab {
    void **free_list;       /* pointer to first free block */
    size_t block_size;
    size_t num_blocks;
    size_t used;
};

/* Initialize a slab over a pre-allocated buffer */
void memslab_init(struct memslab *slab, void *buf,
                  size_t block_size, size_t num_blocks);

/* Allocate one block (returns NULL if none free) */
void *memslab_alloc(struct memslab *slab);

/* Free one block back to the slab */
void memslab_free(struct memslab *slab, void *block);

#endif
