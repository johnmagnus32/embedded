/*
 * pmm.c — physical page allocator.
 *
 * Manages 4 KB physical frames over a range of DRAM with a simple bitmap
 * (1 bit per page: 1 = used, 0 = free). The bitmap itself lives in a static
 * array sized for the T113's 128 MB, so the allocator needs no bootstrap
 * allocation of its own.
 *
 * This is deliberately simple (linear scan). It's enough for building page
 * tables and, later, per-process pages; a real kernel would use buddy/free-list
 * allocators, but that's an optimization, not a correctness need.
 */

#include <stdint.h>
#include "pmm.h"

int printf(const char *fmt, ...);

/* 128 MB / 4 KB = 32768 pages -> 32768 bits = 4096 bytes of bitmap. */
#define MAX_PAGES  (128u * 1024u * 1024u / PAGE_SIZE)   /* 32768 */
static uint8_t bitmap[MAX_PAGES / 8];                    /* 4 KB */

static uint32_t base_pa;      /* physical address of page index 0 */
static uint32_t num_pages;    /* pages under management */
static uint32_t used_pages;

static inline void bit_set(uint32_t i)  { bitmap[i >> 3] |=  (uint8_t)(1u << (i & 7)); }
static inline void bit_clr(uint32_t i)  { bitmap[i >> 3] &= (uint8_t)~(1u << (i & 7)); }
static inline int  bit_get(uint32_t i)  { return (bitmap[i >> 3] >> (i & 7)) & 1; }

static void *pa_to_ptr(uint32_t pa) { return (void *)(uintptr_t)pa; }

void pmm_init(uint32_t start, uint32_t end)
{
	/* Round the managed window to page boundaries. */
	start = (start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	end   = end & ~(PAGE_SIZE - 1);

	base_pa   = start;
	/* Guard against a bogus/degenerate range (e.g. a DTB /memory that reports a
	 * top at or below the kernel end) — otherwise (end-start) would wrap. */
	num_pages = (end > start) ? (end - start) / PAGE_SIZE : 0;
	if (num_pages > MAX_PAGES)
		num_pages = MAX_PAGES;
	used_pages = 0;

	/* All free to start. */
	for (uint32_t i = 0; i < num_pages; i++)
		bit_clr(i);

	printf("pmm: %u pages (%u MiB) over 0x%x..0x%x\n",
	       num_pages, (num_pages * PAGE_SIZE) >> 20, base_pa,
	       base_pa + num_pages * PAGE_SIZE);
}

static void zero_page(uint32_t pa)
{
	uint32_t *p = pa_to_ptr(pa);
	for (uint32_t i = 0; i < PAGE_SIZE / 4; i++)
		p[i] = 0;
}

uint32_t pmm_alloc_page(void)
{
	for (uint32_t i = 0; i < num_pages; i++) {
		if (!bit_get(i)) {
			bit_set(i);
			used_pages++;
			uint32_t pa = base_pa + i * PAGE_SIZE;
			zero_page(pa);
			return pa;
		}
	}
	return 0;   /* out of memory */
}

uint32_t pmm_alloc_aligned(uint32_t n, uint32_t align)
{
	uint32_t align_pages = align / PAGE_SIZE;
	if (align_pages == 0)
		align_pages = 1;

	/* Scan for `n` consecutive free pages starting at an aligned index. */
	for (uint32_t i = 0; i + n <= num_pages; i++) {
		uint32_t pa = base_pa + i * PAGE_SIZE;
		if (pa & (align - 1))
			continue;                 /* not aligned */
		uint32_t j;
		for (j = 0; j < n; j++)
			if (bit_get(i + j))
				break;
		if (j == n) {                     /* run is all free */
			for (j = 0; j < n; j++) {
				bit_set(i + j);
				zero_page(base_pa + (i + j) * PAGE_SIZE);
			}
			used_pages += n;
			return pa;
		}
	}
	return 0;
}

void pmm_free_page(uint32_t pa)
{
	if (pa < base_pa)
		return;
	uint32_t i = (pa - base_pa) / PAGE_SIZE;
	if (i < num_pages && bit_get(i)) {
		bit_clr(i);
		used_pages--;
	}
}

/* Mark every page overlapping [start, end) as USED, so the allocator won't hand
 * it out. Used to protect the bootloader-loaded initramfs + DTB (which live
 * inside the DRAM range pmm manages) until they've been consumed. Idempotent;
 * pages already used stay used (used_pages counts each page at most once). */
void pmm_reserve(uint32_t start, uint32_t end)
{
	if (end <= base_pa)
		return;
	if (start < base_pa)
		start = base_pa;
	uint32_t s = (start - base_pa) / PAGE_SIZE;                 /* round down */
	uint32_t e = (end - base_pa + PAGE_SIZE - 1) / PAGE_SIZE;   /* round up   */
	if (e > num_pages)
		e = num_pages;
	for (uint32_t i = s; i < e; i++)
		if (!bit_get(i)) { bit_set(i); used_pages++; }
}

uint32_t pmm_free_pages(void)  { return num_pages - used_pages; }
uint32_t pmm_total_pages(void) { return num_pages; }
