/*
 * kmalloc.c — a minimal kernel heap (S9).
 *
 * Design: a single growable arena of whole pages from pmm, managed as a
 * first-fit free list of blocks. Each block has an 8-byte header {size, free};
 * blocks are 8-byte aligned. On free we coalesce with the physically-next block
 * if it is also free (forward coalescing keeps it simple; it's enough to stop
 * the arena fragmenting under the ramfs's alloc/free churn). When no block is
 * large enough we grow the arena by allocating more pmm pages (contiguous runs
 * where possible) and appending a new free block.
 *
 * This is the seed of what Linux's slab/SLUB does — subdivide page-allocator
 * pages into small objects — minus the per-size caches and per-CPU magazines.
 * Single kernel context, so no locking.
 */

#include <stdint.h>
#include <stddef.h>
#include "kmalloc.h"
#include "pmm.h"
#include "libk.h"

/* Block header. `size` is the payload size (excludes this header), 8-aligned.
 * We keep a `next` link threading ALL blocks in address order so we can walk
 * for first-fit and coalesce with the successor. */
struct blk {
	uint32_t    size;      /* payload bytes (multiple of 8) */
	uint32_t    free;      /* 1 = free, 0 = in use */
	struct blk *next;      /* next block by address (NULL = last) */
};

#define HDR      ((uint32_t)sizeof(struct blk))   /* 12 -> rounded use below */
#define ALIGN8(x)  (((x) + 7u) & ~7u)
#define MIN_SPLIT  16u        /* don't split off a remainder smaller than this */

static struct blk *head;      /* first block in the arena */
static struct blk *tail;      /* last block (for appending on grow) */
static uint32_t    used_bytes;

/* Grow the arena by at least `need` payload bytes: grab ceil((need+HDR)/PAGE)
 * pages, wrap them in a free block, and append. Returns the new free block. */
static struct blk *grow(uint32_t need)
{
	uint32_t want = ALIGN8(need) + ALIGN8(HDR);
	uint32_t npages = (want + PAGE_SIZE - 1) / PAGE_SIZE;
	uint32_t pa = pmm_alloc_aligned(npages, PAGE_SIZE);   /* contiguous run */
	if (!pa)
		return 0;

	struct blk *b = (struct blk *)(uintptr_t)pa;
	b->size = npages * PAGE_SIZE - ALIGN8(HDR);
	b->free = 1;
	b->next = 0;

	if (!head) {
		head = tail = b;
	} else {
		tail->next = b;
		tail = b;
	}
	return b;
}

/* Split `b` so it holds exactly `size` payload; the remainder becomes a new
 * free block after it (only if the remainder is worth keeping). */
static void split(struct blk *b, uint32_t size)
{
	uint32_t rem = b->size - size;
	if (rem < ALIGN8(HDR) + MIN_SPLIT)
		return;                              /* too small to bother — keep whole */
	struct blk *n = (struct blk *)((uintptr_t)b + ALIGN8(HDR) + size);
	n->size = rem - ALIGN8(HDR);
	n->free = 1;
	n->next = b->next;
	b->size = size;
	b->next = n;
	if (tail == b)
		tail = n;
}

void *kmalloc(uint32_t size)
{
	if (size == 0)
		size = 8;
	size = ALIGN8(size);

	for (struct blk *b = head; b; b = b->next) {
		if (b->free && b->size >= size) {
			split(b, size);
			b->free = 0;
			used_bytes += b->size;
			void *p = (void *)((uintptr_t)b + ALIGN8(HDR));
			memset(p, 0, b->size);           /* zeroed, like kzalloc */
			return p;
		}
	}
	/* nothing fit — grow and use the fresh block */
	struct blk *b = grow(size);
	if (!b)
		return 0;
	split(b, size);
	b->free = 0;
	used_bytes += b->size;
	void *p = (void *)((uintptr_t)b + ALIGN8(HDR));
	memset(p, 0, b->size);
	return p;
}

void *kzalloc(uint32_t size) { return kmalloc(size); }   /* kmalloc zeroes */

void kfree(void *ptr)
{
	if (!ptr)
		return;
	struct blk *b = (struct blk *)((uintptr_t)ptr - ALIGN8(HDR));
	b->free = 1;
	if (used_bytes >= b->size)
		used_bytes -= b->size;
	/* forward-coalesce: merge with the successor if it's free and adjacent */
	struct blk *n = b->next;
	if (n && n->free && (uintptr_t)n == (uintptr_t)b + ALIGN8(HDR) + b->size) {
		b->size += ALIGN8(HDR) + n->size;
		b->next = n->next;
		if (tail == n)
			tail = b;
	}
}

void *krealloc(void *ptr, uint32_t newsize)
{
	if (!ptr)
		return kmalloc(newsize);
	struct blk *b = (struct blk *)((uintptr_t)ptr - ALIGN8(HDR));
	uint32_t old = b->size;
	if (ALIGN8(newsize) <= old)
		return ptr;                          /* shrink/in-place: keep it simple */
	void *n = kmalloc(newsize);
	if (!n)
		return 0;
	memcpy(n, ptr, old);
	kfree(ptr);
	return n;
}

uint32_t kmalloc_used(void) { return used_bytes; }
