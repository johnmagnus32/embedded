/*
 * common/elf.h — the ELF32 (little-endian) FILE FORMAT: on-disk structures + the standard constants.
 * Shared by every tool in our toolchain (as, ld, …) so the format is defined ONCE — our miniature of
 * binutils' BFD (the object-file library `as`/`ld`/`objdump`/`nm`/… all link). This header is pure
 * format: no tool logic, no architecture specifics (a machine's relocation TYPES live in that tool's
 * arch backend; only the generic EM_/ET_/SHT_/… enumerations that are part of the ELF standard are here).
 */
#ifndef OS_ELF_H
#define OS_ELF_H
#include <stdint.h>

typedef uint32_t u32; typedef uint16_t u16; typedef uint8_t u8; typedef int32_t s32;

/* --- on-disk structures ---------------------------------------------------------------------------- */
typedef struct { u8 e_ident[16]; u16 e_type, e_machine; u32 e_version, e_entry, e_phoff, e_shoff, e_flags;
                 u16 e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx; } Elf32_Ehdr;
typedef struct { u32 sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size, sh_link, sh_info,
                 sh_addralign, sh_entsize; } Elf32_Shdr;
typedef struct { u32 st_name, st_value, st_size; u8 st_info, st_other; u16 st_shndx; } Elf32_Sym;
typedef struct { u32 r_offset, r_info; } Elf32_Rel;
typedef struct { u32 p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align; } Elf32_Phdr;
typedef struct { u32 d_tag, d_val; } Elf32_Dyn;   /* .dynamic entry (d_val doubles as d_ptr) */

/* --- e_type / e_machine -------------------------------------------------------------------------- */
#define ET_REL   1
#define ET_EXEC  2
#define ET_DYN   3       /* shared object / PIE: has a load bias, self-relocated via .dynamic */
#define EM_ARM   40

/* --- section header: sh_type / sh_flags / special indices ---------------------------------------- */
#define SHT_PROGBITS 1
#define SHT_SYMTAB   2
#define SHT_STRTAB   3
#define SHT_NOBITS   8
#define SHT_REL      9
#define SHT_DYNAMIC  6
#define SHT_HASH     5
#define SHT_DYNSYM   11
#define SHF_WRITE      1
#define SHF_ALLOC      2
#define SHF_EXECINSTR  4
#define SHN_UNDEF 0
#define SHN_ABS   0xfff1

/* --- symbol table: st_info bind/type ------------------------------------------------------------- */
#define STB_LOCAL  0
#define STB_GLOBAL 1
#define STB_WEAK   2
#define STT_NOTYPE  0
#define STT_OBJECT  1
#define STT_FUNC    2
#define STT_SECTION 3
#define ELF32_ST_INFO(b,t) (((b)<<4)|((t)&0xf))
#define ELF32_ST_BIND(i)   ((i)>>4)
#define ELF32_ST_TYPE(i)   ((i)&0xf)

/* --- relocation: r_info sym/type; program header p_type/p_flags ---------------------------------- */
#define ELF32_R_SYM(i)   ((i)>>8)
#define ELF32_R_TYPE(i)  ((i)&0xff)
#define ELF32_R_INFO(s,t) (((s)<<8)|((t)&0xff))
#define PT_LOAD    1
#define PT_DYNAMIC 2     /* points the loader/self-relocator at the .dynamic array */
#define PT_INTERP  3     /* names the runtime loader (ld.so) for a dynamically-linked consumer */
#define PF_X 1
#define PF_W 2
#define PF_R 4

/* --- .dynamic tags (generic subset a self-relocating PIE needs) ---------------------------------- */
#define DT_NULL     0            /* end of the .dynamic array                     */
#define DT_REL      17           /* address of the Elf32_Rel relocation table     */
#define DT_RELSZ    18           /* total size of that table, in bytes            */
#define DT_RELENT   19           /* size of one Elf32_Rel entry (8)               */
#define DT_RELCOUNT 0x6ffffffau  /* number of leading R_ARM_RELATIVE entries      */
/* .dynamic tags for a shared object's exported symbol table (SysV hash) */
#define DT_HASH     4            /* address of the SysV symbol hash table         */
#define DT_STRTAB   5            /* address of .dynstr                            */
#define DT_SYMTAB   6            /* address of .dynsym                            */
#define DT_STRSZ    10           /* size of .dynstr, in bytes                     */
#define DT_SYMENT   11           /* size of one Elf32_Sym (16 bytes)              */
#define DT_SONAME   14           /* .dynstr offset of this object's soname        */
/* .dynamic tags a dynamically-linked CONSUMER carries: its dependencies + PLT relocations */
#define DT_NEEDED   1            /* .dynstr offset of a needed library's soname   */
#define DT_PLTGOT   3            /* address of the PLT's GOT (.got.plt)           */
#define DT_PLTRELSZ 2            /* total size of the PLT relocation table        */
#define DT_PLTREL   20           /* type of the PLT relocs: DT_REL (our ARM REL)  */
#define DT_JMPREL   23           /* address of the PLT relocation table (JUMP_SLOTs) */

#endif
