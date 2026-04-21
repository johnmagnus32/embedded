/*
 * elfloader.c — Minimal ELF loader for 32-bit ARM
 *
 * Parses the ELF header and program headers, copies PT_LOAD
 * segments into allocated RAM, and starts a task at e_entry.
 *
 * ELF structure:
 *   [ELF header]           ← magic, entry point, program header offset
 *   [Program header table] ← array of segments (PT_LOAD = loadable)
 *   [Section data]         ← .text, .data, .rodata, .bss
 *
 * We only care about PT_LOAD segments — copy them to RAM and jump.
 *
 * Linux equivalent: fs/binfmt_elf.c (load_elf_binary)
 * Zephyr equivalent: subsys/llext/llext.c
 */

#include <stdint.h>
#include "config.h"

#ifdef CONFIG_ELFLOADER

#include "elfloader.h"
#include "sched.h"
#include "heap.h"

/* ---- ELF32 structures (from the ELF spec) ---- */

#define EI_NIDENT 16

struct elf32_ehdr {
    uint8_t  e_ident[EI_NIDENT];  /* magic: 0x7f 'E' 'L' 'F' */
    uint16_t e_type;               /* ET_EXEC=2, ET_DYN=3 */
    uint16_t e_machine;            /* EM_ARM=40 */
    uint32_t e_version;
    uint32_t e_entry;              /* entry point address */
    uint32_t e_phoff;              /* program header table offset */
    uint32_t e_shoff;              /* section header table offset */
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;          /* size of one program header */
    uint16_t e_phnum;              /* number of program headers */
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct elf32_phdr {
    uint32_t p_type;    /* PT_LOAD=1 */
    uint32_t p_offset;  /* offset in file */
    uint32_t p_vaddr;   /* virtual address (= physical for us) */
    uint32_t p_paddr;   /* physical address */
    uint32_t p_filesz;  /* size in file */
    uint32_t p_memsz;   /* size in memory (may be > filesz for .bss) */
    uint32_t p_flags;   /* PF_X=1, PF_W=2, PF_R=4 */
    uint32_t p_align;
};

#define PT_LOAD    1
#define EM_ARM     40
#define ELF_MAGIC  0x464C457F  /* "\x7fELF" as little-endian uint32 */

/* ---- Helpers ---- */

static void mcpy(void *d, const void *s, uint32_t n)
{
    uint8_t *dd = d; const uint8_t *ss = s;
    while (n--) *dd++ = *ss++;
}

static void mzero(void *d, uint32_t n)
{
    uint8_t *dd = d;
    while (n--) *dd++ = 0;
}

/* ---- Loader ---- */

int elf_load_and_run(const void *elf_data, uint32_t elf_size,
                     const char *name, uint8_t priority)
{
    const uint8_t *base = (const uint8_t *)elf_data;
    const struct elf32_ehdr *ehdr = (const struct elf32_ehdr *)base;

    /* Validate ELF magic */
    if (elf_size < sizeof(struct elf32_ehdr))
        return -1;
    if (*(const uint32_t *)ehdr->e_ident != ELF_MAGIC)
        return -1;

    /* Validate it's a 32-bit ARM ELF */
    if (ehdr->e_machine != EM_ARM)
        return -1;

    /*
     * Calculate total memory needed for all PT_LOAD segments.
     * For position-independent code (-fPIC), we allocate one
     * contiguous block and load everything relative to it.
     */
    uint32_t total_memsz = 0;
    uint32_t min_vaddr = 0xFFFFFFFF;
    uint32_t max_vaddr = 0;

    const struct elf32_phdr *phdr =
        (const struct elf32_phdr *)(base + ehdr->e_phoff);

    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            if (phdr[i].p_vaddr < min_vaddr)
                min_vaddr = phdr[i].p_vaddr;
            uint32_t end = phdr[i].p_vaddr + phdr[i].p_memsz;
            if (end > max_vaddr)
                max_vaddr = end;
        }
    }

    if (min_vaddr >= max_vaddr)
        return -1;  /* no loadable segments */

    total_memsz = max_vaddr - min_vaddr;

    /* Allocate memory for the program */
    uint8_t *load_base = heap_alloc(total_memsz);
    if (!load_base)
        return -1;  /* out of memory */

    /* Zero the entire region (handles .bss) */
    mzero(load_base, total_memsz);

    /*
     * Load each PT_LOAD segment.
     * Copy p_filesz bytes from the ELF file, leave the rest as zero (.bss).
     * Adjust addresses relative to our allocated base.
     */
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD)
            continue;

        uint32_t offset_in_load = phdr[i].p_vaddr - min_vaddr;
        uint32_t copy_size = phdr[i].p_filesz;

        if (phdr[i].p_offset + copy_size > elf_size)
            continue;  /* segment extends past file — skip */

        mcpy(load_base + offset_in_load,
             base + phdr[i].p_offset,
             copy_size);
    }

    /*
     * Calculate entry point relative to our load base.
     * For PIC code: entry = load_base + (e_entry - min_vaddr)
     * The +1 sets the Thumb bit (ARM Cortex-M requires it).
     */
    uint32_t entry = (uint32_t)load_base + (ehdr->e_entry - min_vaddr);
    entry |= 1;  /* Thumb bit */

    /* Create a task at the entry point */
    sched_create_task((task_fn)entry, name, priority);

    return 0;
}

#endif /* CONFIG_ELFLOADER */
