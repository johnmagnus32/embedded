/*
 * elf32.h — the ELF32 structures + ARM relocation/dynamic constants the linker
 * needs. Kept minimal (only what ld-gv3 actually touches) and dependency-free so
 * it compiles for BOTH the ARM target and the x86 host unit tests.
 *
 * Verified against the ELF spec + arch/arm ABI (readelf on our own libc.so).
 */
#ifndef GV3_LD_ELF32_H
#define GV3_LD_ELF32_H

#include <stdint.h>

typedef uint32_t Elf32_Addr;
typedef uint32_t Elf32_Word;
typedef int32_t  Elf32_Sword;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;

/* Program header (PT_*). The linker cares about PT_LOAD, PT_DYNAMIC, PT_INTERP. */
typedef struct {
	Elf32_Word p_type;
	Elf32_Off  p_offset;
	Elf32_Addr p_vaddr;
	Elf32_Addr p_paddr;
	Elf32_Word p_filesz;
	Elf32_Word p_memsz;
	Elf32_Word p_flags;
	Elf32_Word p_align;
} Elf32_Phdr;

#define PT_LOAD     1
#define PT_DYNAMIC  2
#define PT_INTERP   3
#define PT_PHDR     6   /* describes the program header table itself */

/* Dynamic-section entry (.dynamic is an array of these, terminated by DT_NULL). */
typedef struct {
	Elf32_Sword d_tag;
	Elf32_Word  d_val;     /* union {d_val; d_ptr} — same width on 32-bit */
} Elf32_Dyn;

#define DT_NULL     0
#define DT_NEEDED   1
#define DT_PLTRELSZ 2
#define DT_PLTGOT   3
#define DT_HASH     4
#define DT_STRTAB   5
#define DT_SYMTAB   6
#define DT_STRSZ    10
#define DT_SYMENT   11
#define DT_REL      17
#define DT_RELSZ    18
#define DT_RELENT   19
#define DT_PLTREL   20
#define DT_JMPREL   23
#define DT_GNU_HASH 0x6ffffef5

/* Symbol table entry. */
typedef struct {
	Elf32_Word    st_name;   /* offset into STRTAB */
	Elf32_Addr    st_value;
	Elf32_Word    st_size;
	unsigned char st_info;   /* bind<<4 | type */
	unsigned char st_other;
	Elf32_Half    st_shndx;  /* SHN_UNDEF(0) = imported */
} Elf32_Sym;

#define SHN_UNDEF 0
#define ELF32_ST_BIND(i) ((i) >> 4)
#define ELF32_ST_TYPE(i) ((i) & 0xf)
#define STB_GLOBAL 1
#define STB_WEAK   2
#define STT_FUNC   2

/* REL relocation (ARM uses REL, not RELA — addend is in the target word). */
typedef struct {
	Elf32_Addr r_offset;   /* where to patch (a VA in the loaded object) */
	Elf32_Word r_info;     /* sym<<8 | type */
} Elf32_Rel;

#define ELF32_R_SYM(i)  ((i) >> 8)
#define ELF32_R_TYPE(i) ((i) & 0xff)

/* ARM relocation types we handle (the only ones our binaries emit). */
#define R_ARM_ABS32     2   /* S + A                                    */
#define R_ARM_GLOB_DAT  21  /* GOT data word:  S + A                    */
#define R_ARM_JUMP_SLOT 22  /* PLT slot:       S (+ A)                  */
#define R_ARM_RELATIVE  23  /* base-relative:  B + A  (no symbol)       */

#endif /* GV3_LD_ELF32_H */
