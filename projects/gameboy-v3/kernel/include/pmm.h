/*
 * pmm.h — physical page allocator (4 KB pages of DRAM).
 * The foundation the MMU + everything above needs: hand out / free physical
 * frames. Simple bitmap allocator over the DRAM above the kernel.
 */
#ifndef GV3K_PMM_H
#define GV3K_PMM_H

#include <stdint.h>

#define PAGE_SIZE   4096u
#define PAGE_SHIFT  12

/* Initialize the allocator over [start, end) of physical DRAM. */
void pmm_init(uint32_t start, uint32_t end);

/* Allocate one zeroed 4 KB page; returns its physical address, or 0 if none. */
uint32_t pmm_alloc_page(void);

/* Allocate `n` contiguous 4 KB pages aligned to `align` bytes (power of two).
 * Returns the base physical address, or 0 on failure. Used for the 16 KB-aligned
 * L1 page table. */
uint32_t pmm_alloc_aligned(uint32_t n, uint32_t align);

/* Free a page previously returned by pmm_alloc_page. */
void pmm_free_page(uint32_t pa);

/* Mark every page overlapping [start, end) as used (protect a region the
 * bootloader placed in DRAM — e.g. the initramfs + DTB — from allocation). */
void pmm_reserve(uint32_t start, uint32_t end);

/* Stats. */
uint32_t pmm_free_pages(void);
uint32_t pmm_total_pages(void);

#endif /* GV3K_PMM_H */
