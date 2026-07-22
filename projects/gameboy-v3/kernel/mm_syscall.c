/*
 * mm_syscall.c — S10 memory syscalls, over vm.c + the per-process bump pointers.
 *
 * Scope (honest): brk heap + anonymous-private mmap, two bump pointers, no VMA
 * tree. Enough for static musl's malloc and startup; file-backed/shared mmap and
 * real per-page mprotect are later refinements. See S10_DESIGN.md.
 */

#include <stdint.h>
#include "mm_syscall.h"
#include "proc.h"
#include "vm.h"
#include "pmm.h"
#include "fs_abi.h"

int printf(const char *fmt, ...);

/* mmap flag/prot bits (ARM = asm-generic; verified). */
#define PROT_NONE   0x0
#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define PROT_EXEC   0x4
#define MAP_SHARED     0x01
#define MAP_PRIVATE    0x02
#define MAP_TYPE       0x0f
#define MAP_FIXED      0x10
#define MAP_ANONYMOUS  0x20

#define PAGE_MASK   (PAGE_SIZE - 1)
#define PAGE_UP(x)   (((x) + PAGE_MASK) & ~PAGE_MASK)
#define PAGE_DOWN(x) ((x) & ~PAGE_MASK)

/* ---- brk ----------------------------------------------------------------- *
 * Returns the RESULTING break (never -errno): brk(0)/too-low -> current break;
 * grow -> map zeroed pages up to the new page-rounded break; shrink -> unmap +
 * free. musl compares "returned < requested" to detect failure. */
long sys_brk(uint32_t addr)
{
	struct proc *p = proc_current();
	uint32_t cur_brk = p->brk;

	if (addr < p->brk_start)               /* query, or nonsense -> report current */
		return (long)cur_brk;
	if (addr >= p->mmap_top)               /* would collide with the mmap area */
		return (long)cur_brk;              /* refuse: return unchanged (musl->mmap2) */

	uint32_t old_top = PAGE_UP(cur_brk);
	uint32_t new_top = PAGE_UP(addr);

	if (new_top > old_top) {
		/* grow: map fresh zeroed pages [old_top, new_top) */
		for (uint32_t va = old_top; va < new_top; va += PAGE_SIZE) {
			uint32_t pa = pmm_alloc_page();          /* zeroed */
			if (!pa || vm_map_page(p->l1_pa, va, pa) != 0) {
				if (pa) pmm_free_page(pa);           /* free the frame we couldn't map */
				/* OOM mid-grow: roll back the pages we just added, report old */
				for (uint32_t v = old_top; v < va; v += PAGE_SIZE) {
					uint32_t rp = vm_walk(p->l1_pa, v);
					if (rp) { vm_unmap_page(p->l1_pa, v); pmm_free_page(rp); }
				}
				return (long)cur_brk;
			}
		}
	} else if (new_top < old_top) {
		/* shrink: unmap + free pages [new_top, old_top) */
		for (uint32_t va = new_top; va < old_top; va += PAGE_SIZE) {
			uint32_t pa = vm_walk(p->l1_pa, va);
			if (pa) { vm_unmap_page(p->l1_pa, va); pmm_free_page(pa); }
		}
	}
	p->brk = addr;
	return (long)addr;
}

/* ---- mmap2 (anonymous private only) -------------------------------------- */
long sys_mmap2(uint32_t addr, uint32_t len, int prot, int flags, int fd, uint32_t pgoff)
{
	(void)addr; (void)prot; (void)pgoff;
	if (len == 0 || len > (USER_VA_MAX - USER_VA_MIN))
		return -K_EINVAL;                  /* 0 or absurd len (PAGE_UP would wrap) */
	/* We only serve anonymous private mappings (what musl's malloc/threads use).
	 * File-backed or shared mappings aren't needed (we exec via read()). */
	if (!(flags & MAP_ANONYMOUS) || (flags & MAP_TYPE) != MAP_PRIVATE || fd != -1)
		return -K_ENOSYS;

	struct proc *p = proc_current();
	uint32_t npages = PAGE_UP(len) / PAGE_SIZE;
	uint32_t span = npages * PAGE_SIZE;
	if (span > p->mmap_top - USER_VA_MIN)  /* request bigger than the region -> no wrap */
		return -K_ENOMEM;
	uint32_t base = p->mmap_top - span;    /* grow downward */
	if (base < PAGE_UP(p->brk))
		return -K_ENOMEM;                  /* ran into the heap */

	/* map npages fresh zeroed frames at [base, mmap_top) */
	for (uint32_t i = 0; i < npages; i++) {
		uint32_t va = base + i * PAGE_SIZE;
		uint32_t pa = pmm_alloc_page();    /* zeroed — anon mem reads as 0 */
		if (!pa || vm_map_page(p->l1_pa, va, pa) != 0) {
			if (pa) pmm_free_page(pa);     /* free the frame we couldn't map */
			/* roll back */
			for (uint32_t j = 0; j < i; j++) {
				uint32_t v = base + j * PAGE_SIZE;
				uint32_t rp = vm_walk(p->l1_pa, v);
				if (rp) { vm_unmap_page(p->l1_pa, v); pmm_free_page(rp); }
			}
			return -K_ENOMEM;
		}
	}
	p->mmap_top = base;                    /* next mapping goes below this one */
	return (long)base;                     /* the mapped VA */
}

long sys_munmap(uint32_t addr, uint32_t len)
{
	if (addr & PAGE_MASK)
		return -K_EINVAL;
	struct proc *p = proc_current();
	uint32_t end = PAGE_UP(addr + len);
	for (uint32_t va = addr; va < end; va += PAGE_SIZE) {
		uint32_t pa = vm_walk(p->l1_pa, va);
		if (pa) { vm_unmap_page(p->l1_pa, va); pmm_free_page(pa); }
	}
	/* If we freed the lowest mmap region, reclaim the bump pointer. (Best-effort:
	 * we don't track holes, so only a free at exactly mmap_top advances it.) */
	if (addr == p->mmap_top)
		p->mmap_top = end;
	return 0;
}

/* mprotect: our L2 small pages are already user-RW + executable, so there's no
 * finer permission to set in S10. Accept anything sane as a no-op success —
 * enough for musl's RELRO / guard-page calls. (PROT_NONE guard pages remain
 * accessible; a real per-page permission change is a later refinement.) */
long sys_mprotect(uint32_t addr, uint32_t len, int prot)
{
	(void)len; (void)prot;
	if (addr & PAGE_MASK)
		return -K_EINVAL;
	return 0;
}
