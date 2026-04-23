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

static void parse_debug_line(const uint8_t *data, uint32_t size);

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

uint32_t sym_find_by_name(const char *name)
{
    for (int i = 0; i < nsyms; i++)
        if (strcmp(syms[i].name, name) == 0) return syms[i].addr;
    return 0;
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

        /* Extract FUNC and OBJECT symbols */
        syms = malloc(n * sizeof(struct sym));
        nsyms = 0;
        for (int j = 0; j < n; j++) {
            int type = ELF32_ST_TYPE(raw[j].st_info);
            if (type != STT_FUNC && type != 1 /* STT_OBJECT */) continue;
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

    /* Find .debug_line section (need section name strings) */
    if (eh.e_shstrndx < eh.e_shnum) {
        Elf32_Shdr *shstr_sh = &shdrs[eh.e_shstrndx];
        char *shstrtab = malloc(shstr_sh->sh_size);
        fseek(f, shstr_sh->sh_offset, SEEK_SET);
        fread(shstrtab, shstr_sh->sh_size, 1, f);
        for (int i = 0; i < eh.e_shnum; i++) {
            if (strcmp(shstrtab + shdrs[i].sh_name, ".debug_line") == 0) {
                uint8_t *dbg = malloc(shdrs[i].sh_size);
                fseek(f, shdrs[i].sh_offset, SEEK_SET);
                fread(dbg, shdrs[i].sh_size, 1, f);
                parse_debug_line(dbg, shdrs[i].sh_size);
                free(dbg);
                break;
            }
        }
        free(shstrtab);
    }

    free(shdrs);
    fclose(f);
    return 0;
}

/* ── DWARF .debug_line parser (DWARF 2/3) ── */

static struct line_entry { uint32_t addr; uint16_t line; uint16_t file_idx; } *lines;
static int nlines;
static char **line_files;
static int nline_files;

static int line_cmp(const void *a, const void *b)
{
    return (int)((const struct line_entry *)a)->addr - (int)((const struct line_entry *)b)->addr;
}

static void add_line(uint32_t addr, int file, int line)
{
    static int line_cap;
    if (nlines >= line_cap) {
        line_cap = line_cap ? line_cap * 2 : 256;
        lines = realloc(lines, line_cap * sizeof(*lines));
    }
    lines[nlines].addr = addr;
    lines[nlines].line = (uint16_t)line;
    lines[nlines].file_idx = (uint16_t)file;
    nlines++;
}

static uint32_t read_uleb(const uint8_t **p)
{
    uint32_t val = 0; int shift = 0;
    do { val |= (uint32_t)(**p & 0x7F) << shift; shift += 7; } while (*(*p)++ & 0x80);
    return val;
}

static int32_t read_sleb(const uint8_t **p)
{
    int32_t val = 0; int shift = 0; uint8_t b;
    do { b = *(*p)++; val |= (int32_t)(b & 0x7F) << shift; shift += 7; } while (b & 0x80);
    if (shift < 32 && (b & 0x40)) val |= -(1 << shift);
    return val;
}

static void parse_debug_line(const uint8_t *data, uint32_t size)
{
    const uint8_t *end = data + size;
    while (data < end) {
        uint32_t unit_len = *(uint32_t *)data; data += 4;
        const uint8_t *unit_end = data + unit_len;
        uint16_t version = *(uint16_t *)data; data += 2;
        if (version < 2 || version > 4) { data = unit_end; continue; }
        uint32_t header_len = *(uint32_t *)data; data += 4;
        const uint8_t *prog_start = data + header_len;
        uint8_t min_insn_len = *data++;
        if (version >= 4) data++; /* max_ops_per_insn */
        uint8_t default_is_stmt = *data++;
        int8_t line_base = (int8_t)*data++;
        uint8_t line_range = *data++;
        uint8_t opcode_base = *data++;
        /* skip standard opcode lengths */
        data += opcode_base - 1;
        /* include directories */
        char *dirs[64];
        int ndirs = 0;
        while (*data) {
            if (ndirs < 64) dirs[ndirs++] = strdup((const char *)data);
            while (*data) data++;
            data++;
        }
        data++; /* skip final null */
        /* file names — prepend directory from dir index */
        int file_start = nline_files;
        while (*data) {
            const char *name = (const char *)data;
            while (*data) data++; data++; /* skip name */
            uint32_t dir_idx = read_uleb(&data); /* dir index (1-based, 0=comp dir) */
            read_uleb(&data); /* time */
            read_uleb(&data); /* size */
            char fullpath[512];
            if (dir_idx > 0 && dir_idx <= (uint32_t)ndirs)
                snprintf(fullpath, sizeof(fullpath), "%s/%s", dirs[dir_idx - 1], name);
            else
                snprintf(fullpath, sizeof(fullpath), "%s", name);
            line_files = realloc(line_files, (nline_files + 1) * sizeof(char *));
            line_files[nline_files++] = strdup(fullpath);
        }
        data++; /* skip final null */
        for (int d = 0; d < ndirs; d++) free(dirs[d]);

        /* Run the line number state machine */
        uint32_t addr = 0;
        int file = 1, line = 1;
        data = prog_start;
        while (data < unit_end) {
            uint8_t op = *data++;
            if (op == 0) { /* extended opcode */
                uint32_t ext_len = read_uleb(&data);
                const uint8_t *ext_end = data + ext_len;
                uint8_t ext_op = *data++;
                if (ext_op == 1) { /* DW_LNE_end_sequence */
                    /* Don't add — end markers aren't real source locations */
                    addr = 0; file = 1; line = 1;
                } else if (ext_op == 2) { /* DW_LNE_set_address */
                    addr = *(uint32_t *)data;
                }
                data = ext_end;
            } else if (op < opcode_base) { /* standard opcode */
                switch (op) {
                case 1: /* DW_LNS_copy */
                    add_line(addr, file - 1 + file_start, line);
                    break;
                case 2: /* DW_LNS_advance_pc */
                    addr += read_uleb(&data) * min_insn_len;
                    break;
                case 3: /* DW_LNS_advance_line */
                    line += read_sleb(&data);
                    break;
                case 4: /* DW_LNS_set_file */
                    file = read_uleb(&data);
                    break;
                case 5: /* DW_LNS_set_column */
                    read_uleb(&data);
                    break;
                case 6: /* DW_LNS_negate_stmt */
                    break;
                case 8: /* DW_LNS_const_add_pc */
                    addr += ((255 - opcode_base) / line_range) * min_insn_len;
                    break;
                case 9: /* DW_LNS_fixed_advance_pc */
                    addr += *(uint16_t *)data; data += 2;
                    break;
                default: /* skip unknown */
                    break;
                }
            } else { /* special opcode */
                int adjusted = op - opcode_base;
                addr += (adjusted / line_range) * min_insn_len;
                line += line_base + (adjusted % line_range);
                add_line(addr, file - 1 + file_start, line);
            }
        }
        data = unit_end;
    }
    if (nlines) qsort(lines, nlines, sizeof(*lines), line_cmp);
}

const char *line_lookup(uint32_t pc, int *line_out)
{
    if (!lines || nlines == 0) { *line_out = 0; return NULL; }
    int lo = 0, hi = nlines - 1, best = -1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (lines[mid].addr <= pc) { best = mid; lo = mid + 1; }
        else hi = mid - 1;
    }
    if (best < 0) { *line_out = 0; return NULL; }
    *line_out = lines[best].line;
    int fi = lines[best].file_idx;
    return (fi >= 0 && fi < nline_files) ? line_files[fi] : NULL;
}

/* ── Breakpoint resolver ── */

uint32_t resolve_breakpoint(const char *spec)
{
    /* Try 0xADDR */
    if (spec[0] == '0' && spec[1] == 'x') return (uint32_t)strtoul(spec, NULL, 16);

    /* Try file:line */
    const char *colon = strchr(spec, ':');
    if (colon) {
        int target_line = atoi(colon + 1);
        int flen = (int)(colon - spec);
        /* Find best matching line entry */
        for (int i = 0; i < nlines; i++) {
            int fi = lines[i].file_idx;
            if (fi >= 0 && fi < nline_files && lines[i].line == target_line) {
                const char *fname = line_files[fi];
                int slen = strlen(fname);
                /* Match suffix (e.g. "test_rtos.c" matches "/path/to/test_rtos.c") */
                if (slen >= flen && memcmp(fname + slen - flen, spec, flen) == 0)
                    return lines[i].addr;
            }
        }
        return 0;
    }

    /* Try function name */
    for (int i = 0; i < nsyms; i++)
        if (strcmp(syms[i].name, spec) == 0) return syms[i].addr;
    return 0;
}
