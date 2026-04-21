/*
 * heap.h — Simple first-fit heap allocator
 *
 * Like Zephyr's k_malloc/k_free (backed by sys_heap).
 * Uses a linked list of free blocks with first-fit search.
 *
 * Not thread-safe by itself — uses a mutex internally.
 */

#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>

void  heap_init(void *mem, size_t size);
void *heap_alloc(size_t size);
void  heap_free(void *ptr);

/* Stats */
size_t heap_free_bytes(void);
size_t heap_used_bytes(void);

#endif
