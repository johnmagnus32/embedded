/*
 * mm_syscall.h — S10 memory syscalls (mmap2/munmap/mprotect/brk).
 *
 * These give static musl its allocator: brk grows the heap, and mmap2 provides
 * anonymous private mappings (musl's malloc + thread setup). All operate on the
 * CURRENT process's address space + its brk/mmap_top bump pointers (see proc.h,
 * S10_DESIGN.md). Return values follow the kernel convention the dispatcher
 * passes back verbatim: brk returns the resulting break (NOT -errno); mmap2
 * returns the VA or a value in -4095..-1 (musl reads that as MAP_FAILED).
 */
#ifndef GV3K_MM_SYSCALL_H
#define GV3K_MM_SYSCALL_H

#include <stdint.h>

long sys_brk(uint32_t addr);
long sys_mmap2(uint32_t addr, uint32_t len, int prot, int flags, int fd, uint32_t pgoff);
long sys_munmap(uint32_t addr, uint32_t len);
long sys_mprotect(uint32_t addr, uint32_t len, int prot);

#endif /* GV3K_MM_SYSCALL_H */
