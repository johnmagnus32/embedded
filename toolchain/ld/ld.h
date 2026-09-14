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

extern u32 load_base;           /* where the image maps: default 0x10000 (hosted), override with -Ttext <addr> */
extern int pie;                 /* -pie: emit ET_DYN with self-relocation metadata (base 0, load-bias fixups) */
#define PAGE      0x1000u       /* segment alignment: each PT_LOAD maps on its own page => W^X enforceable */

/* Dynamic relocation table (PIE only): the virtual addresses of every word that holds an absolute
 * reference. relocate() collects them; the writer emits one R_ARM_RELATIVE per entry into .rel.dyn,
 * and the runtime crt walks that table adding the load bias to each. */
#define MAXDYNREL 16384
extern u32 dynrel[MAXDYNREL]; extern int ndynrel;
#define NDYNENT 5               /* .dynamic entries: DT_REL, DT_RELSZ, DT_RELENT, DT_RELCOUNT, DT_NULL */

typedef struct {
	const char *path; u8 *data; long size;      /* whole file (mutable — relocations patch it in place) */
	Elf32_Ehdr *eh; Elf32_Shdr *sh; int nsh;
	Elf32_Sym *sym; int nsym; const char *strtab;
	u32 *sec_vaddr;                              /* [nsh] assigned virtual address of each allocated section */
	int active;                                  /* 1 = contributes to output; archive members start 0 (lazy) */
} Obj;
#define MAXOBJ 32
extern Obj objs[]; extern int nobj;

void die(const char *fmt, ...);                                    /* front-end (ld.c) */

/* The result of layout(): two loadable segments (W^X). Segment 0 is R-X (headers + .text + .rodata);
 * segment 1 is R-W (.data then .bss). Each is page-aligned so it maps with its own permissions; within
 * a segment we keep vaddr == LOAD_BASE + file-offset (identity map), so the writer needs no offset table.
 * A segment is absent when its *_memsz is 0 (e.g. a program with no writable data). */
typedef struct {
	u32 rx_filesz;                              /* seg 0 size from LOAD_BASE (== memsz; headers included) */
	u32 rw_vaddr, rw_off, rw_filesz, rw_memsz;  /* seg 1: vaddr, file offset, on-disk size, in-mem size  */
	/* PIE only: the .rel.dyn (R_ARM_RELATIVE table) and .dynamic array live at the tail of seg 0. */
	u32 text_size;                              /* .text+.rodata size (seg 0 minus headers and the two below) */
	u32 reldyn_vaddr, reldyn_off, reldyn_sz;    /* .rel.dyn: ndynrel * sizeof(Elf32_Rel)                     */
	u32 dynamic_vaddr, dynamic_off, dynamic_sz; /* .dynamic: the DT_* array + PT_DYNAMIC target              */
} Layout;

/* ---- OBJECT-FORMAT backend (elf.c) --------------------------------------------------------------- */
Obj *elf_load(const char *path);                                   /* parse one .o file into objs[] (active) */
Obj *elf_parse(const char *path, u8 *data, long size, int active); /* parse an in-memory ELF image into objs[] */
void ar_load(const char *path);                                    /* split a .a into objs[] as LAZY members  */
void elf_write_exec(const char *out, u32 entry, const Layout *L);

/* ---- MACHINE-DEPENDENT backend (arm.c) ----------------------------------------------------------- */
extern const u16 md_e_machine;                                     /* EM_ARM — checked on load, stamped on write */
extern const u32 md_r_relative;                                    /* the arch's base-fixup reloc (R_ARM_RELATIVE) */
void md_apply_reloc(Obj *o, u32 type, u8 *loc, u32 S, u32 P);      /* patch one relocation in place */
int  md_needs_dynamic_reloc(u32 type);                             /* 1 if this reloc must become a runtime RELATIVE */
#endif
