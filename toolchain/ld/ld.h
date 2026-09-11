/*
 * ld.h — the interface between the linker's three layers (mirrors the assembler's split, and GNU's):
 *
 *   FRONT-END  (ld.c)   generic linking ALGORITHM: load objects, lay out sections at addresses, build +
 *                       resolve the global symbol table, drive the relocation pass. Calls the hooks below.
 *   OBJ BACKEND (elf.c) object-FORMAT layer: parse an ELF32 relocatable into an Obj, and serialize the
 *                       output ET_EXEC. Everything that knows the on-disk ELF layout lives here.  (~BFD)
 *   MD BACKEND (arm.c)  machine-dependent: apply ONE relocation of a given type + the ELF machine id.
 *                       THE only architecture-specific file.  (~bfd/elf32-arm reloc handling)
 *
 * The ELF format itself (structs + constants) + low-level helpers are shared with `as` via
 * common/elf.h + common/elfutil.h — our miniature libbfd. Another CPU = a new arm.c; another object
 * format = a new elf.c.
 */
#ifndef LD_H
#define LD_H
#include "elf.h"        /* shared ELF32 format: structs + constants (toolchain/common) */
#include "elfutil.h"    /* shared helpers: rd32/wr32, alignup, Strtab */

#define LOAD_BASE 0x00010000u   /* where the image maps (GNU ld's ARM static default region) */

typedef struct {
	const char *path; u8 *data; long size;      /* whole file (mutable — relocations patch it in place) */
	Elf32_Ehdr *eh; Elf32_Shdr *sh; int nsh;
	Elf32_Sym *sym; int nsym; const char *strtab;
	u32 *sec_vaddr;                              /* [nsh] assigned virtual address of each allocated section */
} Obj;
#define MAXOBJ 32
extern Obj objs[]; extern int nobj;

void die(const char *fmt, ...);                                    /* front-end (ld.c) */

/* ---- OBJECT-FORMAT backend (elf.c) --------------------------------------------------------------- */
Obj *elf_load(const char *path);                                   /* parse one .o into objs[] */
void elf_write_exec(const char *out, u32 entry, u32 filesz_end, u32 memsz_end);

/* ---- MACHINE-DEPENDENT backend (arm.c) ----------------------------------------------------------- */
extern const u16 md_e_machine;                                     /* EM_ARM — checked on load, stamped on write */
void md_apply_reloc(Obj *o, u32 type, u8 *loc, u32 S, u32 P);      /* patch one relocation in place */
#endif
