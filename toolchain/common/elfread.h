/*
 * common/elfread.h — a VALIDATED view of an ELF32 little-endian file image, shared by the tools that read ELF
 * (ld, ar). elf_open checks everything a reader will index — the header, every section's file range, the
 * section-name and symbol string tables, each symbol's name and section, each relocation's symbol and target —
 * once, and dies (through the tool's own die()) on the first inconsistency. After that the tables are plain
 * arrays over the image. The image must be 4-byte aligned (a fresh malloc/copy, not a pointer into an archive).
 *
 * The structs are used in place, so the HOST must be little-endian like the files (checked at compile time).
 */
#ifndef OS_ELFREAD_H
#define OS_ELFREAD_H
#include "elf.h"

typedef struct {
	const char *path; u8 *data; long size;
	Elf32_Ehdr *eh; Elf32_Shdr *sh; int nsh; const char *shstr;
	Elf32_Sym *sym; int nsym; const char *strtab;   /* the SHT_SYMTAB and its strings (NULL/0 if none) */
	int symtab;                                     /* its section index (0 if none)                   */
} ElfFile;

/* type: the e_type required (ET_REL, ET_DYN, …); machine: the e_machine required (0 = any). */
void elf_open(ElfFile *f, const char *path, u8 *data, long size, u16 type, u16 machine);
/* A string in section `strsec` (validated STRTAB) at offset `off`, bounds-checked. */
const char *elf_string(const ElfFile *f, int strsec, u32 off);

void die(const char *fmt, ...);                 /* provided by each tool (its own prefix) */
#endif
