/*
 * vm.h — per-process virtual address spaces (ARMv7 short-descriptor, 4KB pages).
 *
 * S6's mmu.c gave us ONE identity-mapped address space via 1 MB sections. S8
 * needs MANY address spaces (one per process), each with its own L1 table that:
 *   - shares the kernel's identity mappings (so the kernel keeps running after
 *     we switch TTBR0), and
 *   - maps user pages at 4 KB granularity (via L2 tables) so each process can
 *     have the same virtual addresses backed by different physical pages.
 *
 * An "address space" is identified by its L1 table's physical address (the
 * value we load into TTBR0).
 */
#ifndef GV3K_VM_H
#define GV3K_VM_H

#include <stdint.h>

/* Create a new address space: a fresh 16 KB L1 table pre-populated with the
 * kernel's identity mappings (copied from the S6 base table). Returns the L1
 * physical address, or 0 on failure. */
uint32_t vm_create(void);

/* Map one 4 KB user page: va -> pa in address space `l1_pa`, user-accessible
 * RW (and executable). Allocates an L2 table on demand. Returns 0 on success. */
int vm_map_page(uint32_t l1_pa, uint32_t va, uint32_t pa);

/* Look up the physical page backing user VA `va` in `l1_pa` (0 if unmapped). */
uint32_t vm_walk(uint32_t l1_pa, uint32_t va);

/* Clear the mapping for `va` (leaves the physical frame for the caller to free
 * and the L2 table in place). Flushes the TLB for that VA. 0 on success. */
int vm_unmap_page(uint32_t l1_pa, uint32_t va);

/* Free every USER page + L2 table owned by `l1_pa`, then the L1 itself.
 * Leaves the kernel identity mappings alone (they're shared, not owned). */
void vm_destroy(uint32_t l1_pa);

/* Copy all USER mappings from `src_l1` into `dst_l1`, allocating fresh physical
 * pages and copying their contents (eager-copy fork). Returns 0 on success. */
int vm_copy(uint32_t dst_l1, uint32_t src_l1);

/* Switch the live address space: set TTBR0 = l1_pa, flush the TLB. */
void vm_switch(uint32_t l1_pa);

/* The user VA region we allow (everything else is kernel/identity). User pages
 * live below the kernel (kernel is at 0x41000000). */
#define USER_VA_MIN   0x00001000u   /* leave page 0 unmapped (null-deref traps) */
#define USER_VA_MAX   0x40000000u   /* up to DRAM base — kernel is above        */

/* S10 memory-map anchors within [USER_VA_MIN, USER_VA_MAX):
 *   ELF (~0x10000) + brk heap grow UP; the anon mmap area grows DOWN from
 *   MMAP_TOP; the initial stack page sits just under USER_VA_MAX. See
 *   S10_DESIGN.md. */
#define MMAP_TOP      0x38000000u   /* anon mappings placed downward from here  */

#endif /* GV3K_VM_H */
