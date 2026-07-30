/*
 * malloc.c — a small first-fit free-list allocator over sbrk() (the kernel's
 * brk). This is the userspace half of the brk story: sbrk grows the heap in
 * bulk; malloc carves it into blocks and reuses freed ones.
 *
 * Design (classic teaching allocator, honest scope):
 *   - Each block has an 8-byte header {size, free-flag+next}. The heap is a
 *     singly-linked list of blocks in address order.
 *   - malloc: first-fit scan; split a too-big block; else sbrk more.
 *   - free: mark free + coalesce with the immediately-following block.
 *   - No per-block alignment beyond 8 bytes; no mmap for large allocs (our
 *     kernel has mmap2, a later refinement can route big requests there).
 * Not thread-safe (single-threaded process), which matches our kernel.
 */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct block {
	size_t        size;      /* payload bytes (not counting this header)      */
	struct block *next;      /* next block in address order                   */
	int           free;      /* 1 = available                                 */
	int           _pad;      /* keep header 8-aligned (16 bytes total)        */
} block_t;

#define HDR   sizeof(block_t)
#define ALIGN8(n) (((n) + 7u) & ~7u)

static block_t *head;        /* first block, or NULL before the first malloc  */

/* Grow the heap by enough for a `need`-payload block; returns the new block. */
static block_t *grow(size_t need)
{
	size_t want = ALIGN8(need) + HDR;
	block_t *b = (block_t *)sbrk((long)want);
	if (b == (void *)-1)
		return NULL;
	b->size = ALIGN8(need);
	b->next = NULL;
	b->free = 0;
	return b;
}

void *malloc(size_t n)
{
	if (n == 0) return NULL;
	n = ALIGN8(n);

	/* first-fit over the existing list */
	block_t *prev = NULL, *b = head;
	while (b) {
		if (b->free && b->size >= n) {
			/* split if the leftover can hold a header + a little payload */
			if (b->size >= n + HDR + 8) {
				block_t *rest = (block_t *)((char *)(b + 1) + n);
				rest->size = b->size - n - HDR;
				rest->next = b->next;
				rest->free = 1;
				b->size = n;
				b->next = rest;
			}
			b->free = 0;
			return b + 1;
		}
		prev = b;
		b = b->next;
	}

	/* no fit: grow the heap and append */
	block_t *nb = grow(n);
	if (!nb) return NULL;
	if (prev) prev->next = nb; else head = nb;
	return nb + 1;
}

void free(void *p)
{
	if (!p) return;
	block_t *b = (block_t *)p - 1;
	b->free = 1;
	/* coalesce with the next block if it's free and physically adjacent */
	if (b->next && b->next->free &&
	    (char *)(b + 1) + b->size == (char *)b->next) {
		b->size += HDR + b->next->size;
		b->next = b->next->next;
	}
}

void *calloc(size_t nmemb, size_t size)
{
	size_t total = nmemb * size;
	if (nmemb && total / nmemb != size) return NULL;   /* overflow */
	void *p = malloc(total);
	if (p) memset(p, 0, total);
	return p;
}

void *realloc(void *p, size_t n)
{
	if (!p) return malloc(n);
	if (n == 0) { free(p); return NULL; }
	block_t *b = (block_t *)p - 1;
	if (b->size >= n) return p;             /* already big enough */
	void *np = malloc(n);
	if (!np) return NULL;
	memcpy(np, p, b->size);
	free(p);
	return np;
}
