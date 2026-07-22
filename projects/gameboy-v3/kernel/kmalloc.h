/*
 * kmalloc.h — a minimal kernel heap for variable-sized allocations (S9).
 *
 * S6's pmm hands out whole 4 KB pages; the ramfs/VFS need small objects (inodes,
 * dirents, file structs, the growable page-pointer arrays). This is a simple
 * bump-plus-freelist allocator carved out of pmm pages — enough for S9, not a
 * general-purpose malloc. It is a bit like the seed of Linux's slab: get pages
 * from the page allocator, sub-divide them for small objects.
 *
 * Scope/limits (honest): first-fit free list with block coalescing on free.
 * Not thread-safe (single kernel context, syscall-boundary switching). No
 * alignment guarantees beyond 8 bytes. Good enough to build a filesystem on.
 */
#ifndef GV3K_KMALLOC_H
#define GV3K_KMALLOC_H

#include <stdint.h>
#include <stddef.h>

void *kmalloc(uint32_t size);         /* 8-byte aligned; zeroed; NULL on OOM */
void *kzalloc(uint32_t size);         /* alias — kmalloc already zeroes */
void  kfree(void *ptr);
void *krealloc(void *ptr, uint32_t newsize);

/* stats (for the S9 self-report) */
uint32_t kmalloc_used(void);

#endif /* GV3K_KMALLOC_H */
