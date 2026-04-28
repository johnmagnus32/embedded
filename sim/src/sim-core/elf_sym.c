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

/* Compilation directory from DWARF (DW_AT_comp_dir) */
char elf_comp_dir[512] = "";

/* Scoped variable info — mirrors DWARF's scope tree */
#define MAX_SCOPES 256
#define MAX_VARS 512

struct var_info {
    char name[32];
    int scope_idx;       /* index into scopes[] */
    uint32_t cu_base;
    int is_const;
    uint32_t const_val;
    uint32_t loc_offset;
    int has_loc;         /* 0=none, 1=loclist, 2=reg, 3=fbreg */
    uint32_t type_die;
};

struct scope_info {
    uint32_t low_pc;
    uint32_t high_pc;
    int parent;          /* parent scope index, -1 for global */
};

static struct scope_info scopes[MAX_SCOPES];
static int nscopes = 0;
static struct var_info vars[MAX_VARS];
static int nvars = 0;

/* Type info from DWARF */
#define MAX_TYPES 512
struct type_info {
    uint32_t die_offset;
    uint16_t tag;
    uint32_t byte_size;
    uint32_t type_ref;   /* referenced type (pointers, typedefs, arrays) */
    char name[32];
    struct { char name[32]; uint32_t offset; uint32_t type_die; } members[16];
    int nmembers;
    struct { char name[32]; uint32_t value; } enumerators[16];
    int nenumerators;
    int array_count;
};
static struct type_info types[MAX_TYPES];
static int ntypes = 0;

static struct type_info *find_type(uint32_t die_offset) {
    for (int i = 0; i < ntypes; i++)
        if (types[i].die_offset == die_offset) return &types[i];
    return NULL;
}

/* Location list data */
static uint8_t *loclists_data = NULL;
static uint32_t loclists_size = 0;

int elf_get_sections(const struct elf_section **out) { *out = elf_sections; return n_elf_sections; }

static void parse_debug_line(const uint8_t *data, uint32_t size);
static void parse_debug_frame(const uint8_t *data, uint32_t size);
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

int sym_in_range(uint32_t lo, uint32_t hi, struct sym_entry *out, int max)
{
    int n = 0;
    for (int i = 0; i < nsyms && n < max; i++) {
        if (syms[i].addr >= lo && syms[i].addr < hi && syms[i].size > 0) {
            out[n].addr = syms[i].addr;
            out[n].size = syms[i].size;
            strncpy(out[n].name, syms[i].name, 63);
            out[n].name[63] = 0;
            n++;
        }
    }
    return n;
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
            } else if (strcmp(sn, ".debug_loclists") == 0) {
                loclists_data = malloc(shdrs[i].sh_size);
                loclists_size = shdrs[i].sh_size;
                fseek(f, shdrs[i].sh_offset, SEEK_SET); fread(loclists_data, loclists_size, 1, f);
            } else if (strcmp(sn, ".debug_frame") == 0) {
                uint8_t *df = malloc(shdrs[i].sh_size);
                uint32_t df_sz = shdrs[i].sh_size;
                fseek(f, shdrs[i].sh_offset, SEEK_SET); fread(df, df_sz, 1, f);
                parse_debug_frame(df, df_sz);
                free(df);
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
#define DW_AT_comp_dir          0x1b
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
    case DW_FORM_sec_offset: case DW_FORM_ref_addr: case DW_FORM_addr:
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

    while (p < info_end) {
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
            uint32_t low_pc;
            uint32_t high_pc;
            uint32_t loc_offset;
            uint32_t const_val;
            int has_loc;
            int has_const;
            int has_high_pc;
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
            d->low_pc = 0; d->high_pc = 0; d->has_high_pc = 0;
            d->loc_offset = 0; d->has_loc = 0;
            d->const_val = 0; d->has_const = 0;
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
                } else if (attr == DW_AT_comp_dir && !elf_comp_dir[0]) {
                    /* Extract compilation directory from first CU */
                    if (form == DW_FORM_strp) {
                        uint32_t off = *(uint32_t *)scan; scan += 4;
                        if (off < str_size) strncpy(elf_comp_dir, str + off, 511);
                    } else if (form == DW_FORM_string) {
                        strncpy(elf_comp_dir, (const char *)scan, 511);
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
                } else if (attr == 0x11) { /* DW_AT_low_pc */
                    d->low_pc = read_form_u32(&scan, form, a->attrs[i].implicit_val);
                } else if (attr == 0x12) { /* DW_AT_high_pc */
                    d->high_pc = read_form_u32(&scan, form, a->attrs[i].implicit_val);
                    d->has_high_pc = 1;
                } else if (attr == DW_AT_location) {
                    if (form == DW_FORM_sec_offset || form == DW_FORM_loclistx) {
                        d->loc_offset = read_form_u32(&scan, form, a->attrs[i].implicit_val);
                        d->has_loc = 1;
                    } else if (form == DW_FORM_exprloc) {
                        uint32_t len = read_uleb(&scan);
                        const uint8_t *expr_start = scan;
                        if (len >= 1 && *scan >= 0x50 && *scan <= 0x6f) {
                            /* DW_OP_reg0..DW_OP_reg31 */
                            d->loc_offset = *scan - 0x50;
                            d->has_loc = 2; /* simple register */
                        } else if (len >= 2 && *scan == 0x91) {
                            /* DW_OP_fbreg: signed offset from frame base */
                            const uint8_t *ep = scan + 1;
                            d->loc_offset = (uint32_t)read_sleb(&ep);
                            d->has_loc = 3; /* 3 = fbreg */
                        }
                        scan = expr_start + len;
                    } else {
                        skip_form(&scan, form, 4, 0);
                    }
                } else if (attr == 0x1c || attr == 0x2f) { /* DW_AT_const_value or DW_AT_upper_bound */
                    d->const_val = read_form_u32(&scan, form, a->attrs[i].implicit_val);
                    d->has_const = 1;
                } else {
                    skip_form(&scan, form, 4, 0);
                }
            }
            ndies++;
        }

        /* Extract types from all DIEs */
        for (int i = 0; i < ndies && ntypes < MAX_TYPES; i++) {
            uint16_t tag = dies[i].tag;
            if (tag == 0x24 || tag == 0x0F || tag == 0x16 || tag == 0x13 ||
                tag == 0x04 || tag == 0x01 || tag == 0x26 || tag == 0x35) {
                /* base_type(0x24), pointer(0x0F), typedef(0x16), struct(0x13),
                   enum(0x04), array(0x01), const(0x26), volatile(0x35) */
                struct type_info *t = &types[ntypes++];
                memset(t, 0, sizeof(*t));
                t->die_offset = dies[i].offset;
                t->tag = tag;
                t->byte_size = dies[i].byte_size;
                t->type_ref = dies[i].type_ref ? cu_start + dies[i].type_ref : 0;
                if (dies[i].name) strncpy(t->name, dies[i].name, 31);

                /* Collect struct members */
                if (tag == 0x13) {
                    for (int j = i + 1; j < ndies && t->nmembers < 16; j++) {
                        if (dies[j].tag != 0x0D) break; /* DW_TAG_member */
                        if (dies[j].name) strncpy(t->members[t->nmembers].name, dies[j].name, 31);
                        t->members[t->nmembers].offset = dies[j].member_loc;
                        t->members[t->nmembers].type_die = dies[j].type_ref ? cu_start + dies[j].type_ref : 0;
                        t->nmembers++;
                    }
                }
                /* Collect enum values */
                if (tag == 0x04) {
                    for (int j = i + 1; j < ndies && t->nenumerators < 16; j++) {
                        if (dies[j].tag != 0x28) break; /* DW_TAG_enumerator */
                        if (dies[j].name) strncpy(t->enumerators[t->nenumerators].name, dies[j].name, 31);
                        t->enumerators[t->nenumerators].value = dies[j].const_val;
                        t->nenumerators++;
                    }
                }
                /* Array element count */
                if (tag == 0x01) {
                    for (int j = i + 1; j < ndies; j++) {
                        if (dies[j].tag != 0x21) break;
                        t->array_count = dies[j].const_val + 1;
                        break;
                    }
                }
            }
        }

        /* Extract local variables from subprograms */
        /* Find CU's low_pc */
        uint32_t cu_low_pc = 0;
        for (int i = 0; i < ndies; i++) {
            if (dies[i].tag == DW_TAG_compile_unit && dies[i].low_pc) {
                cu_low_pc = dies[i].low_pc;
                break;
            }
        }

        /* Build scope tree and extract variables */
        /* Global scope for this CU */
        int global_scope = -1;
        if (nscopes < MAX_SCOPES) {
            global_scope = nscopes;
            scopes[nscopes++] = (struct scope_info){0, 0xFFFFFFFF, -1};
        }

        int scope_stack[32];
        int scope_depth = 0;
        scope_stack[0] = global_scope;

        for (int i = 0; i < ndies; i++) {
            uint16_t tag = dies[i].tag;

            /* Track scope nesting */
            if (tag == 0x2E || tag == 0x0B) { /* subprogram or lexical_block */
                uint32_t lo = dies[i].low_pc;
                uint32_t hi = dies[i].has_high_pc ?
                    (dies[i].high_pc < 0x1000 ? lo + dies[i].high_pc : dies[i].high_pc) : 0;
                if (lo && hi && nscopes < MAX_SCOPES) {
                    int parent = scope_depth > 0 ? scope_stack[scope_depth] : global_scope;
                    int idx = nscopes;
                    scopes[nscopes++] = (struct scope_info){lo, hi, parent};
                    if (scope_depth < 31) scope_stack[++scope_depth] = idx;
                }
            }

            /* Pop scopes that ended (heuristic: if current DIE is a new subprogram, reset) */
            if (tag == 0x2E && scope_depth > 0) {
                scope_depth = 0; /* reset to global for new function */
                uint32_t lo = dies[i].low_pc;
                uint32_t hi = dies[i].has_high_pc ?
                    (dies[i].high_pc < 0x1000 ? lo + dies[i].high_pc : dies[i].high_pc) : 0;
                if (lo && hi && nscopes < MAX_SCOPES) {
                    int idx = nscopes;
                    scopes[nscopes++] = (struct scope_info){lo, hi, global_scope};
                    scope_stack[++scope_depth] = idx;
                }
            }

            /* Extract variables */
            if ((tag == DW_TAG_variable || tag == 0x05) && dies[i].name && nvars < MAX_VARS) {
                struct var_info *v = &vars[nvars++];
                strncpy(v->name, dies[i].name, 31); v->name[31] = '\0';
                v->scope_idx = scope_depth > 0 ? scope_stack[scope_depth] : global_scope;
                v->cu_base = cu_low_pc;
                v->is_const = dies[i].has_const;
                v->const_val = dies[i].const_val;
                v->loc_offset = dies[i].loc_offset;
                v->has_loc = dies[i].has_loc;
                v->type_die = dies[i].type_ref ? cu_start + dies[i].type_ref : 0;
            }
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
    fprintf(stderr, "[sim-core] DWARF: %d vars, %d types extracted\n", nvars, ntypes);
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

/* Find the address of the next source line after pc (different line number).
 * Returns 0 if not found. */
/* ── Breakpoint resolver ── */

/* Get the type DIE offset for a variable at the given PC */
/* Check if PC is within a scope */
static int pc_in_scope(int scope_idx, uint32_t pc)
{
    if (scope_idx < 0 || scope_idx >= nscopes) return 0;
    return pc >= scopes[scope_idx].low_pc && pc < scopes[scope_idx].high_pc;
}

/* Find variable by name, searching innermost scope first */
static struct var_info *find_var_in_scope(const char *name, uint32_t pc)
{
    /* Collect scopes containing PC, innermost first (highest index = most recently added) */
    int best_var = -1;
    int best_depth = -1;

    for (int v = 0; v < nvars; v++) {
        if (strcmp(vars[v].name, name) != 0) continue;
        int si = vars[v].scope_idx;
        if (!pc_in_scope(si, pc)) continue;

        /* Count depth: how many ancestors does this scope have? */
        int depth = 0;
        for (int s = si; s >= 0 && depth < 30; s = scopes[s].parent) depth++;

        if (depth > best_depth) {
            best_depth = depth;
            best_var = v;
        }
    }
    return best_var >= 0 ? &vars[best_var] : NULL;
}

uint32_t var_type_die(const char *name, uint32_t pc)
{
    struct var_info *v = find_var_in_scope(name, pc);
    return v ? v->type_die : 0;
}

uint32_t type_deref(uint32_t type_die)
{
    struct type_info *resolve_type(uint32_t);
    struct type_info *t = resolve_type(type_die);
    if (t && t->tag == 0x0F && t->type_ref) return t->type_ref;
    return 0;
}

uint32_t type_byte_size(uint32_t type_die)
{
    struct type_info *resolve_type(uint32_t);
    struct type_info *t = resolve_type(type_die);
    if (!t) return 4;
    if (t->byte_size) return t->byte_size;
    if (t->tag == 0x01 && t->array_count && t->type_ref) { /* array */
        return t->array_count * type_byte_size(t->type_ref);
    }
    if (t->tag == 0x0F) return 4; /* pointer */
    return 4;
}

uint32_t type_member(uint32_t struct_type_die, const char *member_name,
                     uint32_t *offset_out, uint32_t *member_type_out)
{
    struct type_info *resolve_type(uint32_t);
    struct type_info *t = resolve_type(struct_type_die);
    if (!t || t->tag != 0x13) return 0; /* not a struct */
    for (int i = 0; i < t->nmembers; i++) {
        if (strcmp(t->members[i].name, member_name) == 0) {
            *offset_out = t->members[i].offset;
            *member_type_out = t->members[i].type_die;
            return 1;
        }
    }
    return 0;
}

uint32_t type_array_elem(uint32_t array_type_die, uint32_t *elem_size_out)
{
    struct type_info *resolve_type(uint32_t);
    struct type_info *t = resolve_type(array_type_die);
    if (!t || t->tag != 0x01) return 0; /* not an array */
    struct type_info *elem = t->type_ref ? resolve_type(t->type_ref) : NULL;
    *elem_size_out = elem ? (elem->byte_size ? elem->byte_size : 4) : 4;
    return t->type_ref;
}

/* ── .debug_frame CFA table ── */
#define MAX_FDE 128
#define MAX_CFA_ROWS 8
struct cfa_row { uint32_t pc; uint32_t offset; };
static struct { uint32_t start, end; struct cfa_row rows[MAX_CFA_ROWS]; int nrows; } fdes[MAX_FDE];
static int nfdes;

static void parse_debug_frame(const uint8_t *data, uint32_t size)
{
    const uint8_t *p = data, *end = data + size;
    /* First pass: find CIE defaults */
    int cie_code_align = 1;
    int cie_data_align = -4;
    uint32_t cie_cfa_offset = 0;

    while (p < end) {
        const uint8_t *entry = p;
        uint32_t len = *(uint32_t*)p; p += 4;
        if (len == 0 || len == 0xFFFFFFFF) break;
        const uint8_t *next = p + len;
        uint32_t cie_id = *(uint32_t*)p; p += 4;

        if (cie_id == 0xFFFFFFFF) {
            /* CIE */
            p++; /* version */
            while (*p) p++; p++; /* augmentation string */
            cie_code_align = read_uleb(&p);
            cie_data_align = (int)read_sleb(&p);
            read_uleb(&p); /* return address column */
            /* Parse initial instructions for default CFA */
            while (p < next) {
                uint8_t op = *p++;
                if ((op & 0xC0) == 0x40) continue; /* DW_CFA_advance_loc */
                if ((op & 0xC0) == 0x80) { read_uleb(&p); continue; } /* DW_CFA_offset */
                if ((op & 0xC0) == 0xC0) continue; /* DW_CFA_restore */
                if (op == 0x0C) { /* DW_CFA_def_cfa */
                    read_uleb(&p); /* register */
                    cie_cfa_offset = read_uleb(&p);
                } else if (op == 0) break; /* DW_CFA_nop or padding */
                else if (op == 0x0E) { cie_cfa_offset = read_uleb(&p); } /* DW_CFA_def_cfa_offset */
            }
        } else if (nfdes < MAX_FDE) {
            /* FDE */
            uint32_t pc_begin = *(uint32_t*)p; p += 4;
            uint32_t pc_range = *(uint32_t*)p; p += 4;
            struct cfa_row *rows = fdes[nfdes].rows;
            fdes[nfdes].start = pc_begin;
            fdes[nfdes].end = pc_begin + pc_range;
            rows[0].pc = pc_begin;
            rows[0].offset = cie_cfa_offset;
            int nr = 1;
            uint32_t loc = pc_begin;
            while (p < next && nr < MAX_CFA_ROWS) {
                uint8_t op = *p++;
                if ((op & 0xC0) == 0x40) { loc += (op & 0x3F) * cie_code_align; continue; }
                if ((op & 0xC0) == 0x80) { read_uleb(&p); continue; }
                if ((op & 0xC0) == 0xC0) continue;
                if (op == 0x0E) { /* DW_CFA_def_cfa_offset */
                    uint32_t off = read_uleb(&p);
                    rows[nr].pc = loc; rows[nr].offset = off; nr++;
                } else if (op == 0x0C) { read_uleb(&p); rows[nr].pc = loc; rows[nr].offset = read_uleb(&p); nr++; }
                else if (op == 0x02) { loc += read_uleb(&p) * cie_code_align; }
                else if (op == 0) continue; /* nop */
                else if (op == 0x05) { read_uleb(&p); read_uleb(&p); } /* DW_CFA_offset_extended */
                else break;
            }
            fdes[nfdes].nrows = nr;
            nfdes++;
        }
        p = next;
    }
}

/* Return CFA offset from SP at a given PC. Returns 0 if unknown. */
uint32_t cfa_offset_at_pc(uint32_t pc)
{
    for (int i = 0; i < nfdes; i++) {
        if (pc >= fdes[i].start && pc < fdes[i].end) {
            uint32_t off = 0;
            for (int r = 0; r < fdes[i].nrows; r++) {
                if (fdes[i].rows[r].pc <= pc) off = fdes[i].rows[r].offset;
            }
            return off;
        }
    }
    return 0;
}

/* Return all stack-resident variables in scope at PC.
 * Fills out[] with {name, sp_offset, type_die}. Returns count. */
int vars_on_stack(uint32_t pc, struct stack_var *out, int max)
{
    uint32_t cfa_off = cfa_offset_at_pc(pc);
    int n = 0;
    for (int v = 0; v < nvars && n < max; v++) {
        int si = vars[v].scope_idx;
        if (!pc_in_scope(si, pc)) continue;
        int32_t off;
        if (vars[v].has_loc == 3) {
            off = (int32_t)vars[v].loc_offset + (int32_t)cfa_off;
        } else if (vars[v].has_loc == 1 && loclists_data && vars[v].loc_offset < loclists_size) {
            /* Scan loclist for fbreg entry covering this PC */
            const uint8_t *p = loclists_data + vars[v].loc_offset;
            const uint8_t *end = loclists_data + loclists_size;
            uint32_t base = vars[v].cu_base;
            int found = 0;
            while (p < end) {
                uint8_t ek = *p++;
                if (ek == 0) break;
                if (ek == 6) { base = *(uint32_t*)p; p += 4; continue; }
                uint32_t s0, s1; uint16_t elen;
                if (ek == 8) { s0 = *(uint32_t*)p; p += 4; uint32_t len = read_uleb(&p); s1 = s0 + len; }
                else if (ek == 7) { s0 = *(uint32_t*)p; p += 4; s1 = *(uint32_t*)p; p += 4; }
                else if (ek == 4) { s0 = base + read_uleb(&p); s1 = base + read_uleb(&p); }
                else if (ek == 9) { read_uleb(&p); read_uleb(&p); continue; }
                else break;
                elen = read_uleb(&p);
                if (pc >= s0 && pc < s1 && elen > 0 && *p == 0x91) {
                    const uint8_t *ep = p + 1;
                    off = (int32_t)read_sleb(&ep) + (int32_t)cfa_off;
                    found = 1;
                }
                p += elen;
                if (found) break;
            }
            if (!found) continue;
        } else continue;
        /* Deduplicate: keep innermost scope (highest depth) */
        int dup = -1;
        for (int j = 0; j < n; j++) {
            if (strcmp(out[j].name, vars[v].name) == 0) { dup = j; break; }
        }
        if (dup >= 0) {
            /* Replace if this var is in a deeper scope */
            int d1 = 0, d2 = 0;
            for (int s = vars[v].scope_idx; s >= 0; s = scopes[s].parent) d1++;
            /* find original var's scope depth — stored scope_idx not saved, just skip */
            continue; /* keep first found, good enough */
        }
        strncpy(out[n].name, vars[v].name, 31); out[n].name[31] = 0;
        out[n].sp_offset = off;
        out[n].type_die = vars[v].type_die;
        n++;
    }
    return n;
}

/* Look up a local variable by name at the given PC.
 * Returns: 0 = not found, 1 = register (reg_out), 2 = constant (val_out),
 *          3 = memory address (val_out) */
int var_lookup(const char *name, uint32_t pc, int *reg_out, uint32_t *val_out)
{
    struct var_info *v = find_var_in_scope(name, pc);
    if (!v) {
        /* Debug: check if variable exists at all */
        for (int i = 0; i < nvars; i++) {
            if (strcmp(vars[i].name, name) == 0) {
                int si = vars[i].scope_idx;
                fprintf(stderr, "[dbg] var '%s' in scope %d [0x%08x-0x%08x], pc=0x%08x %s\n",
                    name, si, scopes[si].low_pc, scopes[si].high_pc, pc,
                    pc_in_scope(si, pc) ? "IN" : "OUT");
            }
        }
        return 0;
    }

    /* Constant value */
    if (v->is_const) {
        *val_out = v->const_val;
        return 2;
    }

    /* Simple register (from inline exprloc) */
        if (v->has_loc == 2) {
            *reg_out = v->loc_offset;
            return 1;
        }

        /* Frame base relative (from inline exprloc DW_OP_fbreg) */
        if (v->has_loc == 3) {
            *val_out = v->loc_offset; /* signed offset from SP */
            return 3;
        }

        /* Location list */
        if (v->has_loc == 1 && loclists_data && v->loc_offset < loclists_size) {
            const uint8_t *p = loclists_data + v->loc_offset;
            const uint8_t *end = loclists_data + loclists_size;
            uint32_t base_addr = v->cu_base;
            fprintf(stderr, "[dbg] loclist for '%s' at offset 0x%x, first byte=0x%02x\n",
                    name, v->loc_offset, *p);  /* CU's low_pc for offset_pair */

            while (p < end) {
                uint8_t entry_kind = *p++;
                if (entry_kind == 0) break; /* DW_LLE_end_of_list */
                if (entry_kind == 9) { /* DW_LLE_GNU_view_pair — skip */
                    read_uleb(&p); read_uleb(&p);
                    continue;
                }
                if (entry_kind == 6) { /* DW_LLE_base_address */
                    base_addr = *(uint32_t*)p; p += 4;
                    continue;
                }
                if (entry_kind == 8) { /* DW_LLE_start_length */
                    uint32_t start = *(uint32_t*)p; p += 4;
                    uint32_t length = read_uleb(&p);
                    uint16_t expr_len = read_uleb(&p);
                    fprintf(stderr, "[dbg] start_length: 0x%08x len=%u expr_len=%d op=0x%02x pc=0x%08x\n",
                            start, length, expr_len, expr_len > 0 ? *p : 0, pc);
                    if (pc >= start && pc < start + length && expr_len > 0) {
                        uint8_t op = *p;
                        if (op >= 0x50 && op <= 0x6f) { *reg_out = op - 0x50; return 1; }
                        if (op == 0x91) { const uint8_t *ep = p+1; *val_out = (uint32_t)read_sleb(&ep); return 3; }
                    }
                    p += expr_len;
                    continue;
                }
                if (entry_kind == 7) { /* DW_LLE_start_end */
                    uint32_t start = *(uint32_t*)p; p += 4;
                    uint32_t e = *(uint32_t*)p; p += 4;
                    uint16_t expr_len = read_uleb(&p);
                    if (pc >= start && pc < e && expr_len > 0) {
                        uint8_t op = *p;
                        if (op >= 0x50 && op <= 0x6f) { *reg_out = op - 0x50; return 1; }
                        if (op == 0x91) { const uint8_t *ep = p+1; *val_out = (uint32_t)read_sleb(&ep); return 3; }
                    }
                    p += expr_len;
                    continue;
                }
                if (entry_kind == 4) { /* DW_LLE_offset_pair */
                    uint32_t start_off = read_uleb(&p);
                    uint32_t end_off = read_uleb(&p);
                    uint16_t expr_len = read_uleb(&p);
                    uint32_t start = base_addr + start_off;
                    uint32_t e = base_addr + end_off;
                    if (pc >= start && pc < e && expr_len > 0) {
                        uint8_t op = *p;
                        if (op >= 0x50 && op <= 0x6f) { *reg_out = op - 0x50; return 1; }
                        if (op == 0x91) { const uint8_t *ep = p+1; *val_out = (uint32_t)read_sleb(&ep); return 3; }
                    }
                    p += expr_len;
                    continue;
                }
                break; /* unknown entry */
            }
        }
        return 0;
}

/* Follow type chain through typedefs, const, volatile to the real type */
struct type_info *resolve_type(uint32_t die_offset) {
    for (int depth = 0; depth < 10; depth++) {
        struct type_info *t = find_type(die_offset);
        if (!t) return NULL;
        /* Follow typedefs, const, volatile, pointer qualifiers to base */
        if (t->tag == 0x16 || t->tag == 0x26 || t->tag == 0x35) { /* typedef, const, volatile */
            if (t->type_ref) { die_offset = t->type_ref; continue; }
        }
        return t;
    }
    return NULL;
}

/* Format a typed value into a JSON string. addr = memory address of the value.
 * Writes to buf, returns chars written. */
int type_format(uint32_t type_die, uint32_t addr, uint8_t *ram, uint8_t *flash,
                char *buf, int bufsize)
{
    struct type_info *t = resolve_type(type_die);
    if (!t) {
        uint32_t val = 0;
        if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE)
            val = *(uint32_t*)(ram + (addr - RAM_BASE));
        return snprintf(buf, bufsize, "%u", val);
    }

    int n = 0;

    if (t->tag == 0x24) { /* base_type (int, char, etc.) */
        uint32_t val = 0;
        if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE) {
            if (t->byte_size == 1) val = ram[addr - RAM_BASE];
            else if (t->byte_size == 2) val = *(uint16_t*)(ram + (addr - RAM_BASE));
            else val = *(uint32_t*)(ram + (addr - RAM_BASE));
        } else if (addr >= FLASH_BASE && addr < FLASH_BASE + FLASH_SIZE) {
            val = *(uint32_t*)(flash + (addr - FLASH_BASE));
        }
        n += snprintf(buf+n, bufsize-n, "%u", val);

    } else if (t->tag == 0x0F) { /* pointer_type */
        uint32_t val = 0;
        if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE)
            val = *(uint32_t*)(ram + (addr - RAM_BASE));
        else if (addr >= FLASH_BASE && addr < FLASH_BASE + FLASH_SIZE)
            val = *(uint32_t*)(flash + (addr - FLASH_BASE));
        /* If it's a char pointer, try to show the string */
        struct type_info *pointee = t->type_ref ? resolve_type(t->type_ref) : NULL;
        if (pointee && pointee->byte_size == 1 && val >= FLASH_BASE && val < FLASH_BASE + FLASH_SIZE) {
            const char *s = (const char*)(flash + (val - FLASH_BASE));
            n += snprintf(buf+n, bufsize-n, "0x%08x \\\"%.20s\\\"", val, s);
        } else {
            n += snprintf(buf+n, bufsize-n, "0x%08x", val);
        }

    } else if (t->tag == 0x04) { /* enumeration_type */
        uint32_t val = 0;
        if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE)
            val = *(uint32_t*)(ram + (addr - RAM_BASE));
        const char *ename = NULL;
        for (int i = 0; i < t->nenumerators; i++)
            if (t->enumerators[i].value == val) { ename = t->enumerators[i].name; break; }
        if (ename) n += snprintf(buf+n, bufsize-n, "%s (%u)", ename, val);
        else n += snprintf(buf+n, bufsize-n, "%u", val);

    } else if (t->tag == 0x13) { /* structure_type */
        n += snprintf(buf+n, bufsize-n, "{");
        for (int i = 0; i < t->nmembers && n < bufsize - 50; i++) {
            if (i) n += snprintf(buf+n, bufsize-n, ", ");
            n += snprintf(buf+n, bufsize-n, "%s=", t->members[i].name);
            n += type_format(t->members[i].type_die, addr + t->members[i].offset,
                           ram, flash, buf+n, bufsize-n);
        }
        n += snprintf(buf+n, bufsize-n, "}");

    } else if (t->tag == 0x01) { /* array_type */
        struct type_info *elem = t->type_ref ? resolve_type(t->type_ref) : NULL;
        int elem_size = elem ? (elem->byte_size ? elem->byte_size : 4) : 4;
        int count = t->array_count ? t->array_count : 1;
        if (count > 8) count = 8; /* limit display */
        n += snprintf(buf+n, bufsize-n, "[");
        for (int i = 0; i < count && n < bufsize - 20; i++) {
            if (i) n += snprintf(buf+n, bufsize-n, ", ");
            n += type_format(t->type_ref, addr + i * elem_size, ram, flash, buf+n, bufsize-n);
        }
        if (t->array_count > 8) n += snprintf(buf+n, bufsize-n, ", ...");
        n += snprintf(buf+n, bufsize-n, "]");

    } else {
        uint32_t val = 0;
        if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE)
            val = *(uint32_t*)(ram + (addr - RAM_BASE));
        n += snprintf(buf+n, bufsize-n, "0x%08x", val);
    }

    return n;
}

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
                /* Match suffix: "main.c" matches "src/main.c"
                 * Also try: "main" matches "src/main.c" (strip .c/.h) */
                if (slen >= flen && memcmp(fname + slen - flen, spec, flen) == 0)
                    return lines[i].addr;
                /* Try matching basename without extension */
                const char *base = fname;
                for (const char *p = fname; *p; p++) if (*p == '/') base = p + 1;
                int blen = strlen(base);
                const char *dot = strrchr(base, '.');
                int bname_len = dot ? (int)(dot - base) : blen;
                if (bname_len == flen && memcmp(base, spec, flen) == 0)
                    return lines[i].addr;
            }
        }
        return 0;
    }

    /* Try function name — find address that the loop branches back to */
    for (int i = 0; i < nsyms; i++) {
        if (strcmp(syms[i].name, spec) == 0) {
            uint32_t addr = syms[i].addr;
            uint32_t end = addr + syms[i].size;
            /* Find first line entry strictly after the symbol start */
            for (int j = 0; j < nlines; j++) {
                if (lines[j].addr > addr && lines[j].addr < end)
                    return lines[j].addr;
            }
            return addr;
        }
    }
    return 0;
}
