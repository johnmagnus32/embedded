/*
 * common/elfutil.h — small format-level helpers shared by the toolchain: little-endian word access,
 * alignment, and a growable string-table builder (used to assemble .strtab/.shstrtab). Pure utilities,
 * no tool logic — the reusable primitives underneath both the assembler's and linker's ELF writers.
 */
#ifndef FORGE_ELFUTIL_H
#define FORGE_ELFUTIL_H
#include <stddef.h>
#include "elf.h"

u32  rd32(const u8 *p);          /* read a little-endian 32-bit word */
void wr32(u8 *p, u32 v);         /* write a little-endian 32-bit word */
u32  alignup(u32 x, u32 a);      /* round x up to a multiple of a (a=power of two; a<=1 => x) */

/* Growable string table: str_add appends name (with its NUL) and returns its byte offset. Call once
 * with "" first so offset 0 is the empty string (ELF's sh_name/st_name = 0 must read as ""). */
typedef struct { char *b; size_t len, cap; } Strtab;
u32 str_add(Strtab *s, const char *name);

#endif
