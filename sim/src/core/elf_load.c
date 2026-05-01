/*
 * elf_load.c — Stripped ELF loader: program segments only
 *
 * Loads PT_LOAD segments into flash/RAM. No .symtab, no .debug_*.
 * Used by sim-core which doesn't need symbols or DWARF.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "armv7m_cpu.h"
#include "elf_load.h"

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type, e_machine;
    uint32_t e_version, e_entry, e_phoff, e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize, e_phentsize, e_phnum;
    uint16_t e_shentsize, e_shnum, e_shstrndx;
} Elf32_Ehdr;

typedef struct {
    uint32_t p_type, p_offset, p_vaddr, p_paddr;
    uint32_t p_filesz, p_memsz, p_flags, p_align;
} Elf32_Phdr;

#define PT_LOAD 1

int elf_load_segments(const char *path, uint8_t *flash, uint8_t *ram)
{
    FILE *f = fopen(path, "rb");
    if (!f) { perror("open elf"); return -1; }

    Elf32_Ehdr eh;
    fread(&eh, sizeof(eh), 1, f);

    if (memcmp(eh.e_ident, "\x7f""ELF", 4) != 0 || eh.e_ident[4] != 1) {
        fclose(f);
        return -1;
    }

    for (int i = 0; i < eh.e_phnum; i++) {
        fseek(f, eh.e_phoff + i * eh.e_phentsize, SEEK_SET);
        Elf32_Phdr ph;
        fread(&ph, sizeof(ph), 1, f);
        if (ph.p_type != PT_LOAD || ph.p_filesz == 0) continue;

        uint8_t *dest = NULL;
        uint32_t off = 0;
        if (ph.p_paddr >= FLASH_BASE && ph.p_paddr < FLASH_BASE + FLASH_SIZE) {
            dest = flash; off = ph.p_paddr - FLASH_BASE;
        } else if (ph.p_paddr >= RAM_BASE && ph.p_paddr < RAM_BASE + RAM_SIZE) {
            dest = ram; off = ph.p_paddr - RAM_BASE;
        }
        if (dest) {
            fseek(f, ph.p_offset, SEEK_SET);
            fread(dest + off, 1, ph.p_filesz, f);
        }
    }

    fclose(f);
    return 0;
}
