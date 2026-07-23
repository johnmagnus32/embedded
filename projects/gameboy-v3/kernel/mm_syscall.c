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
#include "file.h"
#include "ramfs.h"

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

/* Map `npages` fresh zeroed frames at [base, base+npages*PAGE). If a page is
 * already mapped (MAP_FIXED replacing an earlier reservation), free the old
 * frame first. Returns 0 or -ENOMEM (rolling back the frames it added). */
static int map_zeroed_range(struct proc *p, uint32_t base, uint32_t npages)
{
	for (uint32_t i = 0; i < npages; i++) {
		uint32_t va = base + i * PAGE_SIZE;
		uint32_t old = vm_walk(p->l1_pa, va);   /* MAP_FIXED over a reservation */
		if (old) { vm_unmap_page(p->l1_pa, va); pmm_free_page(old); }
		uint32_t pa = pmm_alloc_page();         /* zeroed */
		if (!pa || vm_map_page(p->l1_pa, va, pa) != 0) {
			if (pa) pmm_free_page(pa);
			for (uint32_t j = 0; j < i; j++) {  /* roll back what we added */
				uint32_t v = base + j * PAGE_SIZE, rp = vm_walk(p->l1_pa, v);
				if (rp) { vm_unmap_page(p->l1_pa, v); pmm_free_page(rp); }
			}
			return -1;
		}
	}
	return 0;
}

/* Copy up to `len` file bytes from fd's inode at byte offset `foff` into the
 * user pages just mapped at `va` (which are zero-filled, so any tail past EOF or
 * past filesz stays zero — the bss of a data segment). Writes go through the
 * identity-mapped physical frames. Returns 0, or -errno for a bad fd. */
static int copy_file_into(struct proc *p, uint32_t va, uint32_t len,
                          int fd, uint32_t foff)
{
	struct file *f = fd_get(proc_fds(), fd);
	if (!f || !f->inode || f->inode->type != RF_REG)
		return -K_EACCES;                       /* not a regular file */
	/* Read directly into each mapped page's physical frame (identity-mapped). */
	uint32_t done = 0;
	while (done < len) {
		uint32_t p_va = va + done;
		uint32_t pa   = vm_walk(p->l1_pa, p_va & ~PAGE_MASK);
		if (!pa) break;                         /* shouldn't happen (just mapped) */
		uint32_t in_pg = p_va & PAGE_MASK;
		uint32_t chunk = PAGE_SIZE - in_pg;
		if (chunk > len - done) chunk = len - done;
		long n = ramfs_read(f->inode, foff + done,
		                    (void *)(uintptr_t)(pa + in_pg), chunk);
		if (n <= 0) break;                      /* EOF: leave the rest zero */
		done += (uint32_t)n;
		if ((uint32_t)n < chunk) break;         /* short read = hit EOF */
	}
	return 0;
}

/* ---- mmap2 (anonymous + file-backed private, incl. MAP_FIXED) ------------- *
 * Supports what musl's malloc AND its dynamic linker need:
 *  - anonymous private (fd==-1): zero-filled pages (malloc, thread stacks).
 *  - file-backed private (fd>=0): pages zero-filled then file bytes copied in
 *    (COW-equivalent: we always copy, we have no shared page cache); the tail
 *    past EOF/filesz stays zero (a data segment's bss).
 *  - MAP_FIXED: map at the caller's exact addr, replacing any existing mapping
 *    (ld.so reserves a library span, then MAP_FIXEDs each segment over it).
 * Non-FIXED requests are placed by a downward bump from p->mmap_top. `prot` is
 * not enforced per-page (our L2 pages are RWX) — see sys_mprotect. */
long sys_mmap2(uint32_t addr, uint32_t len, int prot, int flags, int fd, uint32_t pgoff)
{
	(void)prot;
	if (len == 0 || len > (USER_VA_MAX - USER_VA_MIN))
		return -K_EINVAL;                  /* 0 or absurd len (PAGE_UP would wrap) */
	if ((flags & MAP_TYPE) != MAP_PRIVATE)
		return -K_ENOSYS;                  /* only PRIVATE (no shared page cache) */

	int file_backed = !(flags & MAP_ANONYMOUS);
	if (file_backed && fd < 0)
		return -K_EBADF;                   /* file mapping needs a real fd */

	struct proc *p = proc_current();
	uint32_t npages = PAGE_UP(len) / PAGE_SIZE;
	uint32_t span   = npages * PAGE_SIZE;
	/* mmap2 offset is in 4096-byte units; compute in 64-bit to catch overflow. */
	uint64_t foff64 = (uint64_t)pgoff << 12;
	if (foff64 > 0xFFFFFFFFull)
		return -K_EINVAL;
	uint32_t foff = (uint32_t)foff64;

	uint32_t base;
	if (flags & MAP_FIXED) {
		if (addr & PAGE_MASK) return -K_EINVAL;    /* must be page-aligned */
		if (addr < USER_VA_MIN || addr + span > USER_VA_MAX || addr + span < addr)
			return -K_EINVAL;
		base = addr;                                /* caller chooses the address */
	} else {
		if (span > p->mmap_top - USER_VA_MIN)
			return -K_ENOMEM;
		base = p->mmap_top - span;                  /* grow downward */
		if (base < PAGE_UP(p->brk))
			return -K_ENOMEM;                       /* ran into the heap */
	}

	if (map_zeroed_range(p, base, npages) != 0)
		return -K_ENOMEM;

	if (file_backed) {
		/* copy filesz-worth of bytes (clamped to the mapping length); ramfs_read
		 * stops at EOF so a mapping that runs past the file keeps zeros there. */
		int rc = copy_file_into(p, base, len, fd, foff);
		if (rc != 0) {                              /* bad fd/inode: undo the map */
			for (uint32_t i = 0; i < npages; i++) {
				uint32_t v = base + i * PAGE_SIZE, rp = vm_walk(p->l1_pa, v);
				if (rp) { vm_unmap_page(p->l1_pa, v); pmm_free_page(rp); }
			}
			return rc;
		}
	}

	if (!(flags & MAP_FIXED))
		p->mmap_top = base;                         /* next mapping goes below this */
	return (long)base;
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
