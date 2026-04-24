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

/* ── ELF sections (allocated, with addresses) ── */
#define MAX_ELF_SECTIONS 32
static struct elf_section elf_sections[MAX_ELF_SECTIONS];
static int n_elf_sections;

int elf_get_sections(const struct elf_section **out) { *out = elf_sections; return n_elf_sections; }

static void parse_debug_line(const uint8_t *data, uint32_t size);
static void parse_debug_info(const uint8_t *info, uint32_t info_size,
                             const uint8_t *abbr, uint32_t abbr_size,
                             const char *str, uint32_t str_size);

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

    /* Find .debug_line section and extract allocated sections (need section name strings) */
    if (eh.e_shstrndx < eh.e_shnum) {
        Elf32_Shdr *shstr_sh = &shdrs[eh.e_shstrndx];
        char *shstrtab = malloc(shstr_sh->sh_size);
        fseek(f, shstr_sh->sh_offset, SEEK_SET);
        fread(shstrtab, shstr_sh->sh_size, 1, f);

        /* Collect allocated sections with nonzero address */
        n_elf_sections = 0;
        for (int i = 0; i < eh.e_shnum && n_elf_sections < MAX_ELF_SECTIONS; i++) {
            if (!(shdrs[i].sh_flags & 2)) continue; /* SHF_ALLOC = 2 */
            if (shdrs[i].sh_size == 0) continue;
            elf_sections[n_elf_sections].addr = shdrs[i].sh_addr;
            elf_sections[n_elf_sections].size = shdrs[i].sh_size;
            strncpy(elf_sections[n_elf_sections].name, shstrtab + shdrs[i].sh_name, 31);
            elf_sections[n_elf_sections].name[31] = '\0';
            n_elf_sections++;
        }

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

        /* Load .debug_info, .debug_abbrev, .debug_str for struct layout */
        uint8_t *di = NULL, *da = NULL; char *ds = NULL;
        uint32_t di_sz = 0, da_sz = 0, ds_sz = 0;
        for (int i = 0; i < eh.e_shnum; i++) {
            const char *sn = shstrtab + shdrs[i].sh_name;
            if (strcmp(sn, ".debug_info") == 0) {
                di = malloc(shdrs[i].sh_size); di_sz = shdrs[i].sh_size;
                fseek(f, shdrs[i].sh_offset, SEEK_SET); fread(di, di_sz, 1, f);
            } else if (strcmp(sn, ".debug_abbrev") == 0) {
                da = malloc(shdrs[i].sh_size); da_sz = shdrs[i].sh_size;
                fseek(f, shdrs[i].sh_offset, SEEK_SET); fread(da, da_sz, 1, f);
            } else if (strcmp(sn, ".debug_str") == 0) {
                ds = malloc(shdrs[i].sh_size); ds_sz = shdrs[i].sh_size;
                fseek(f, shdrs[i].sh_offset, SEEK_SET); fread(ds, ds_sz, 1, f);
            }
        }
        if (di && da && ds)
            parse_debug_info(di, di_sz, da, da_sz, ds, ds_sz);
        free(di); free(da); free(ds);

        free(shstrtab);
    }

    free(shdrs);
    fclose(f);
    return 0;
}

/* ── DWARF helpers ── */

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

/* ── DWARF .debug_info parser — extract struct member offsets ── */

/* DWARF tags */
#define DW_TAG_array_type       0x01
#define DW_TAG_structure_type   0x13
#define DW_TAG_member           0x0d
#define DW_TAG_variable         0x34
#define DW_TAG_compile_unit     0x11

/* DWARF attributes */
#define DW_AT_name              0x03
#define DW_AT_byte_size         0x0b
#define DW_AT_type              0x49
#define DW_AT_data_member_location 0x38
#define DW_AT_location          0x02
#define DW_AT_sibling           0x01

/* DWARF forms */
#define DW_FORM_addr        0x01
#define DW_FORM_block2      0x03
#define DW_FORM_block4      0x04
#define DW_FORM_data2       0x05
#define DW_FORM_data4       0x06
#define DW_FORM_data8       0x07
#define DW_FORM_string      0x08
#define DW_FORM_block       0x09
#define DW_FORM_block1      0x0a
#define DW_FORM_data1       0x0b
#define DW_FORM_flag        0x0c
#define DW_FORM_sdata       0x0d
#define DW_FORM_strp        0x0e
#define DW_FORM_udata       0x0f
#define DW_FORM_ref_addr    0x10
#define DW_FORM_ref1        0x11
#define DW_FORM_ref2        0x12
#define DW_FORM_ref4        0x13
#define DW_FORM_ref8        0x14
#define DW_FORM_ref_udata   0x15
#define DW_FORM_indirect    0x16
#define DW_FORM_sec_offset  0x17
#define DW_FORM_exprloc     0x18
#define DW_FORM_flag_present 0x19
#define DW_FORM_strx        0x1a
#define DW_FORM_addrx       0x1b
#define DW_FORM_ref_sup4    0x1c
#define DW_FORM_strp_sup    0x1d
#define DW_FORM_data16      0x1e
#define DW_FORM_line_strp   0x1f
#define DW_FORM_ref_sig8    0x20
#define DW_FORM_implicit_const 0x21
#define DW_FORM_loclistx    0x22
#define DW_FORM_rnglistx    0x23
#define DW_FORM_ref_sup8    0x24
#define DW_FORM_strx1       0x25
#define DW_FORM_strx2       0x26
#define DW_FORM_strx3       0x27
#define DW_FORM_strx4       0x28
#define DW_FORM_addrx1      0x29
#define DW_FORM_addrx2      0x2a
#define DW_FORM_addrx3      0x2b
#define DW_FORM_addrx4      0x2c

/* Abbreviation entry */
struct abbrev {
    uint32_t code;
    uint16_t tag;
    uint8_t  has_children;
    struct { uint16_t attr; uint16_t form; int64_t implicit_val; } attrs[32];
    int nattrs;
};

#define MAX_ABBREVS 128
static struct abbrev abbrevs[MAX_ABBREVS];
static int n_abbrevs;

static void parse_abbrev_table(const uint8_t *data, uint32_t size)
{
    const uint8_t *end = data + size;
    n_abbrevs = 0;
    while (data < end && n_abbrevs < MAX_ABBREVS) {
        uint32_t code = read_uleb(&data);
        if (code == 0) break;
        struct abbrev *a = &abbrevs[n_abbrevs++];
        a->code = code;
        a->tag = (uint16_t)read_uleb(&data);
        a->has_children = *data++;
        a->nattrs = 0;
        while (1) {
            uint32_t attr = read_uleb(&data);
            uint32_t form = read_uleb(&data);
            if (attr == 0 && form == 0) break;
            if (a->nattrs < 32) {
                a->attrs[a->nattrs].attr = (uint16_t)attr;
                a->attrs[a->nattrs].form = (uint16_t)form;
                a->attrs[a->nattrs].implicit_val = 0;
                if (form == DW_FORM_implicit_const)
                    a->attrs[a->nattrs].implicit_val = read_sleb(&data);
                a->nattrs++;
            }
        }
    }
}

static const struct abbrev *find_abbrev(uint32_t code)
{
    for (int i = 0; i < n_abbrevs; i++)
        if (abbrevs[i].code == code) return &abbrevs[i];
    return NULL;
}

/* Skip a DWARF form value, return bytes consumed. Returns 0 on unknown form. */
static uint32_t skip_form(const uint8_t **p, uint16_t form, uint8_t addr_size, uint8_t dwarf64)
{
    const uint8_t *start = *p;
    switch (form) {
    case DW_FORM_addr:       *p += addr_size; break;
    case DW_FORM_data1: case DW_FORM_ref1: case DW_FORM_flag: *p += 1; break;
    case DW_FORM_data2: case DW_FORM_ref2: *p += 2; break;
    case DW_FORM_data4: case DW_FORM_ref4: case DW_FORM_strp:
    case DW_FORM_sec_offset: case DW_FORM_ref_addr:
    case DW_FORM_line_strp: case DW_FORM_strp_sup:
    case DW_FORM_ref_sup4:
        *p += 4; break;
    case DW_FORM_data8: case DW_FORM_ref8: case DW_FORM_ref_sig8:
    case DW_FORM_ref_sup8:
        *p += 8; break;
    case DW_FORM_data16:     *p += 16; break;
    case DW_FORM_sdata: case DW_FORM_udata: case DW_FORM_ref_udata:
    case DW_FORM_strx: case DW_FORM_addrx:
    case DW_FORM_loclistx: case DW_FORM_rnglistx:
        read_uleb(p); break;
    case DW_FORM_strx1: case DW_FORM_addrx1: *p += 1; break;
    case DW_FORM_strx2: case DW_FORM_addrx2: *p += 2; break;
    case DW_FORM_strx3: case DW_FORM_addrx3: *p += 3; break;
    case DW_FORM_strx4: case DW_FORM_addrx4: *p += 4; break;
    case DW_FORM_block1: { uint8_t n = *(*p)++; *p += n; break; }
    case DW_FORM_block2: { uint16_t n = *(uint16_t*)*p; *p += 2 + n; break; }
    case DW_FORM_block4: { uint32_t n = *(uint32_t*)*p; *p += 4 + n; break; }
    case DW_FORM_block: case DW_FORM_exprloc: { uint32_t n = read_uleb(p); *p += n; break; }
    case DW_FORM_string: while (**p) (*p)++; (*p)++; break;
    case DW_FORM_flag_present: break; /* zero size */
    case DW_FORM_implicit_const: break; /* value in abbrev table */
    case DW_FORM_indirect: { uint32_t f = read_uleb(p); skip_form(p, f, addr_size, dwarf64); break; }
    default: return 0; /* unknown */
    }
    return (uint32_t)(*p - start);
}

/* Read a form value as uint32 (for ref4, data1, data2, data4, udata, implicit_const) */
static uint32_t read_form_u32(const uint8_t **p, uint16_t form, int64_t implicit_val)
{
    uint32_t v = 0;
    switch (form) {
    case DW_FORM_data1: case DW_FORM_ref1: case DW_FORM_flag: v = *(*p)++; break;
    case DW_FORM_data2: case DW_FORM_ref2: v = *(uint16_t*)*p; *p += 2; break;
    case DW_FORM_data4: case DW_FORM_ref4: case DW_FORM_strp:
    case DW_FORM_sec_offset: case DW_FORM_ref_addr:
        v = *(uint32_t*)*p; *p += 4; break;
    case DW_FORM_udata: case DW_FORM_ref_udata: v = read_uleb(p); break;
    case DW_FORM_sdata: v = (uint32_t)read_sleb(p); break;
    case DW_FORM_implicit_const: v = (uint32_t)implicit_val; break;
    case DW_FORM_flag_present: v = 1; break;
    default: skip_form(p, form, 4, 0); break;
    }
    return v;
}

/* Stored struct info */
struct dwarf_struct_info {
    uint32_t tcb_size;
    int32_t  sp_off;
    int32_t  name_off;
    int      valid;
};
static struct dwarf_struct_info dwarf_tcb = { .valid = 0 };

void dwarf_get_tcb_layout(uint32_t *tcb_size, int *sp_off, int *name_off)
{
    if (dwarf_tcb.valid) {
        *tcb_size = dwarf_tcb.tcb_size;
        *sp_off = dwarf_tcb.sp_off;
        *name_off = dwarf_tcb.name_off;
    } else {
        *tcb_size = 0;
        *sp_off = -1;
        *name_off = -1;
    }
}

static void parse_debug_info(const uint8_t *info, uint32_t info_size,
                             const uint8_t *abbr, uint32_t abbr_size,
                             const char *str, uint32_t str_size)
{
    const uint8_t *p = info;
    const uint8_t *info_end = info + info_size;

    while (p < info_end && !dwarf_tcb.valid) {
        if (p + 12 > info_end) break;
        uint32_t cu_start = (uint32_t)(p - info); /* offset of unit_length in .debug_info */
        uint32_t unit_len = *(uint32_t *)p; p += 4;
        const uint8_t *unit_end = p + unit_len;
        if (unit_end > info_end) break;
        uint16_t version = *(uint16_t *)p; p += 2;
        uint32_t abbr_off;
        if (version >= 5) {
            p++; /* unit_type */
            p++; /* addr_size */
            abbr_off = *(uint32_t *)p; p += 4;
        } else {
            abbr_off = *(uint32_t *)p; p += 4;
            p++; /* addr_size */
        }

        /* Parse this CU's abbreviation table */
        if (abbr_off < abbr_size)
            parse_abbrev_table(abbr + abbr_off, abbr_size - abbr_off);

        #define MAX_DIES 512
        struct die_info {
            uint32_t offset;
            uint16_t tag;
            uint32_t type_ref;
            uint32_t byte_size;
            uint32_t member_loc;
            const char *name;
        } dies[MAX_DIES];
        int ndies = 0;

        const uint8_t *scan = p;
        while (scan < unit_end && ndies < MAX_DIES) {
            uint32_t die_off = (uint32_t)(scan - info);
            uint32_t code = read_uleb(&scan);
            if (code == 0) continue;
            const struct abbrev *a = find_abbrev(code);
            if (!a) break;

            struct die_info *d = &dies[ndies];
            d->offset = die_off;
            d->tag = a->tag;
            d->type_ref = 0;
            d->byte_size = 0;
            d->member_loc = 0xFFFFFFFF;
            d->name = NULL;

            for (int i = 0; i < a->nattrs; i++) {
                uint16_t attr = a->attrs[i].attr;
                uint16_t form = a->attrs[i].form;

                if (attr == DW_AT_name) {
                    if (form == DW_FORM_strp) {
                        uint32_t off = *(uint32_t *)scan; scan += 4;
                        if (off < str_size) d->name = str + off;
                    } else if (form == DW_FORM_string) {
                        d->name = (const char *)scan;
                        while (*scan) scan++; scan++;
                    } else {
                        skip_form(&scan, form, 4, 0);
                    }
                } else if (attr == DW_AT_type) {
                    d->type_ref = read_form_u32(&scan, form, a->attrs[i].implicit_val);
                } else if (attr == DW_AT_byte_size) {
                    d->byte_size = read_form_u32(&scan, form, a->attrs[i].implicit_val);
                } else if (attr == DW_AT_data_member_location) {
                    d->member_loc = read_form_u32(&scan, form, a->attrs[i].implicit_val);
                } else {
                    skip_form(&scan, form, 4, 0);
                }
            }
            ndies++;
        }

        /* Find 'tasks' variable in this CU */
        uint32_t tasks_type_ref = 0;
        for (int i = 0; i < ndies; i++) {
            if (dies[i].tag == DW_TAG_variable && dies[i].name && strcmp(dies[i].name, "tasks") == 0) {
                tasks_type_ref = dies[i].type_ref;
                break;
            }
        }
        if (!tasks_type_ref) { p = unit_end; continue; }

        /* Follow type: if array, get element type */
        uint32_t struct_ref = cu_start + tasks_type_ref;
        for (int i = 0; i < ndies; i++) {
            if (dies[i].offset == struct_ref && dies[i].tag == DW_TAG_array_type) {
                struct_ref = cu_start + dies[i].type_ref;
                break;
            }
        }

        /* Find the struct and its members */
        int struct_idx = -1;
        for (int i = 0; i < ndies; i++) {
            if (dies[i].offset == struct_ref && dies[i].tag == DW_TAG_structure_type) {
                struct_idx = i;
                break;
            }
        }
        if (struct_idx < 0) { p = unit_end; continue; }

    dwarf_tcb.tcb_size = dies[struct_idx].byte_size;
    dwarf_tcb.sp_off = -1;
    dwarf_tcb.name_off = -1;

    /* Members follow the struct DIE until next non-member or end */
    for (int i = struct_idx + 1; i < ndies; i++) {
        if (dies[i].tag != DW_TAG_member) break;
        if (dies[i].name && strcmp(dies[i].name, "sp") == 0)
            dwarf_tcb.sp_off = (int32_t)dies[i].member_loc;
        if (dies[i].name && strcmp(dies[i].name, "name") == 0)
            dwarf_tcb.name_off = (int32_t)dies[i].member_loc;
    }

    dwarf_tcb.valid = (dwarf_tcb.sp_off >= 0 && dwarf_tcb.name_off >= 0);
    if (dwarf_tcb.valid) {
        fprintf(stderr, "[sim-core] DWARF: task_tcb size=%u sp@%d name@%d\n",
                dwarf_tcb.tcb_size, dwarf_tcb.sp_off, dwarf_tcb.name_off);
    }

        p = unit_end;
    } /* end while CU loop */
}

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
