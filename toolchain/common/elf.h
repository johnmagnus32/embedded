/*
 * common/elf.h — the ELF32 (little-endian) FILE FORMAT: on-disk structures + the standard constants.
 * Shared by every tool in our toolchain (as, ld, …) so the format is defined ONCE — our miniature of
 * binutils' BFD (the object-file library `as`/`ld`/`objdump`/`nm`/… all link). This header is pure
 * format: no tool logic, no architecture specifics (a machine's relocation TYPES live in that tool's
 * arch backend; only the generic EM_/ET_/SHT_/… enumerations that are part of the ELF standard are here).
 */
#ifndef FORGE_ELF_H
#define FORGE_ELF_H
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

/* --- e_type / e_machine -------------------------------------------------------------------------- */
#define ET_REL   1
#define ET_EXEC  2
#define EM_ARM   40

/* --- section header: sh_type / sh_flags / special indices ---------------------------------------- */
#define SHT_PROGBITS 1
#define SHT_SYMTAB   2
#define SHT_STRTAB   3
#define SHT_NOBITS   8
#define SHT_REL      9
#define SHF_WRITE      1
#define SHF_ALLOC      2
#define SHF_EXECINSTR  4
#define SHN_UNDEF 0
#define SHN_ABS   0xfff1

/* --- symbol table: st_info bind/type ------------------------------------------------------------- */
#define STB_LOCAL  0
#define STB_GLOBAL 1
#define STT_NOTYPE  0
#define STT_FUNC    2
#define STT_SECTION 3
#define ELF32_ST_INFO(b,t) (((b)<<4)|((t)&0xf))
#define ELF32_ST_BIND(i)   ((i)>>4)
#define ELF32_ST_TYPE(i)   ((i)&0xf)

/* --- relocation: r_info sym/type; program header p_type/p_flags ---------------------------------- */
#define ELF32_R_SYM(i)   ((i)>>8)
#define ELF32_R_TYPE(i)  ((i)&0xff)
#define ELF32_R_INFO(s,t) (((s)<<8)|((t)&0xff))
#define PT_LOAD 1
#define PF_X 1
#define PF_W 2
#define PF_R 4

#endif
