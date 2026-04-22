/*
 * elf_sym.c — Minimal ELF parser: load segments into flash/RAM + extract symbol table
 *
 * Supports 32-bit ARM ELF only. Parses program headers to load code,
 * and section headers to find .symtab/.strtab for function name lookup.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "cpu.h"
#include "elf_sym.h"

/* ── Minimal ELF32 structures ── */
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

typedef struct {
    uint32_t sh_name, sh_type, sh_flags, sh_addr;
    uint32_t sh_offset, sh_size, sh_link, sh_info;
    uint32_t sh_addralign, sh_entsize;
} Elf32_Shdr;

typedef struct {
    uint32_t st_name, st_value, st_size;
    uint8_t  st_info, st_other;
    uint16_t st_shndx;
} Elf32_Sym;

#define PT_LOAD    1
#define SHT_SYMTAB 2
#define STT_FUNC   2
#define ELF32_ST_TYPE(i) ((i) & 0xF)

/* ── Symbol table (sorted by address) ── */
static struct sym { uint32_t addr; uint32_t size; char name[64]; } *syms;
static int nsyms;

static int sym_cmp(const void *a, const void *b)
{
    return (int)((const struct sym *)a)->addr - (int)((const struct sym *)b)->addr;
}

const char *sym_lookup(uint32_t pc, uint32_t *offset)
{
    if (!syms || nsyms == 0) { *offset = 0; return NULL; }

    /* Binary search for the largest addr <= pc */
    int lo = 0, hi = nsyms - 1, best = -1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (syms[mid].addr <= pc) { best = mid; lo = mid + 1; }
        else hi = mid - 1;
    }
    if (best < 0) { *offset = 0; return NULL; }
    *offset = pc - syms[best].addr;
    return syms[best].name;
}

/* ── ELF loader ── */
int elf_load(const char *path, uint8_t *flash, uint8_t *ram)
{
    FILE *f = fopen(path, "rb");
    if (!f) { perror("open elf"); return -1; }

    Elf32_Ehdr eh;
    fread(&eh, sizeof(eh), 1, f);

    /* Validate ELF magic */
    if (memcmp(eh.e_ident, "\x7f""ELF", 4) != 0 || eh.e_ident[4] != 1) {
        /* Not an ELF — treat as raw binary */
        fclose(f);
        return -1;
    }

    /* Load PT_LOAD segments into flash/RAM */
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

    /* Find .symtab section */
    Elf32_Shdr *shdrs = malloc(eh.e_shnum * sizeof(Elf32_Shdr));
    fseek(f, eh.e_shoff, SEEK_SET);
    fread(shdrs, sizeof(Elf32_Shdr), eh.e_shnum, f);

    for (int i = 0; i < eh.e_shnum; i++) {
        if (shdrs[i].sh_type != SHT_SYMTAB) continue;

        int n = shdrs[i].sh_size / shdrs[i].sh_entsize;
        Elf32_Sym *raw = malloc(shdrs[i].sh_size);
        fseek(f, shdrs[i].sh_offset, SEEK_SET);
        fread(raw, shdrs[i].sh_size, 1, f);

        /* Load string table (sh_link points to it) */
        Elf32_Shdr *strtab_sh = &shdrs[shdrs[i].sh_link];
        char *strtab = malloc(strtab_sh->sh_size);
        fseek(f, strtab_sh->sh_offset, SEEK_SET);
        fread(strtab, strtab_sh->sh_size, 1, f);

        /* Extract FUNC symbols */
        syms = malloc(n * sizeof(struct sym));
        nsyms = 0;
        for (int j = 0; j < n; j++) {
            if (ELF32_ST_TYPE(raw[j].st_info) != STT_FUNC) continue;
            if (raw[j].st_value == 0) continue;
            syms[nsyms].addr = raw[j].st_value & ~1u; /* strip thumb bit */
            syms[nsyms].size = raw[j].st_size;
            strncpy(syms[nsyms].name, strtab + raw[j].st_name, 63);
            syms[nsyms].name[63] = '\0';
            nsyms++;
        }
        qsort(syms, nsyms, sizeof(struct sym), sym_cmp);

        free(strtab);
        free(raw);
        break;  /* only need first .symtab */
    }

    free(shdrs);
    fclose(f);
    return 0;
}
