/*
 * elfloader.h — Load and execute ELF binaries
 *
 * Parses a 32-bit ARM ELF file, loads PT_LOAD segments into RAM,
 * and creates a task at the entry point.
 *
 * Like Linux's fs/binfmt_elf.c or Zephyr's llext (linkable loadable extensions).
 *
 * Limitations vs real Linux:
 *   - No dynamic linking (no .so, no ld.so)
 *   - No virtual memory (loaded at physical address)
 *   - Programs must be position-independent (-fPIC)
 *   - No separate address space (shares memory with OS)
 */

#ifndef ELFLOADER_H
#define ELFLOADER_H

#include <stdint.h>

/* Load an ELF from a memory buffer and create a task for it.
 * Returns 0 on success, -1 on failure. */
int elf_load_and_run(const void *elf_data, uint32_t elf_size,
                     const char *name, uint8_t priority);

#endif
