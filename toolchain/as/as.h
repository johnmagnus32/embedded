/*
 * as.h — the interface between the assembler's three layers, mirroring GNU binutils' split so a second
 * architecture is just a new backend, never a front-end change:
 *
 *   FRONT-END  (as.c)   generic: lexing, directives, symbol/section/reloc/fixup tables, the driver.
 *                       Knows nothing architecture-specific — it calls the md_* hooks below.  (gas read.c)
 *   MD BACKEND (arm.c)  machine-dependent: parse+encode instructions, register/immediate syntax, the
 *                       relocation types, and the ELF machine id. THE ONLY arch-specific file.  (gas tc-arm.c)
 *   OBJ BACKEND (elf.c) object-format: serialize the tables into an ELF32 relocatable.  (gas obj-elf.c)
 *
 * Adding e.g. aarch64 = a new arm.c-shaped backend (its own md_assemble/encoders + md_e_machine) with
 * the front-end + obj backend untouched — exactly how binutils adds a tc-<arch>.c.
 */
#ifndef AS_H
#define AS_H
#include <stddef.h>
#include "elf.h"   /* shared ELF32 format: u8/u16/u32 typedefs + the generic constants (toolchain/common) */

/* The assembler's INTERNAL tables (not the ELF on-disk structs — those are Elf32_* in elf.h): the
 * section byte buffers, symbol records, relocations, and forward-local-branch fixups the front-end owns. */
typedef struct { char *name; u32 type, flags; u8 *data; size_t len, cap; int shndx; } Section;
typedef struct { char *name; int sec; u32 value, size; int global, type, defined; } Sym;
/* global: 1 if .global'd. A symbol is emitted LOCAL iff (defined && !global); undefined or .global'd
 * symbols are GLOBAL. So compiler-internal labels (.L…, not .global'd) are local, like GNU as. */
typedef struct { int sec; u32 off; int symidx; u32 type; } Reloc;   /* type is an md-supplied reloc code */
typedef struct { int sec; u32 off; int local_num; } Fixup;          /* forward local-label branch to patch */

#define MAXSEC 32
#define MAXSYM 65536  /* a big preprocessed kernel .c emits tens of thousands of .L labels + symbols */
#define MAXFIX 65536
#define MAXREL 65536
extern Section secs[]; extern int nsec, cursec;   /* cursec = active section index into secs[] */
extern Sym syms[]; extern int nsym;
extern Reloc rels[]; extern int nrel;
extern Fixup fixes[]; extern int nfix;

/* ---- FRONT-END services (as.c), called by both backends ------------------------------------------ */
void die(const char *fmt, ...);
int  sec_find(const char *name);
int  sec_get(const char *name, u32 type, u32 flags);   /* find-or-create + select */
void emit(const void *p, size_t n);                     /* append bytes to cursec */
void emit32(u32 w);                                     /* append a little-endian word to cursec */
void patch32(int sec, u32 off, u32 w);                  /* overwrite the word at sec:off */
u32  read32(int sec, u32 off);                          /* read the word at sec:off */
u32  here(void);                                        /* current offset within cursec */
int  sym_find(const char *name);
int  sym_intern(const char *name);                      /* find-or-create an (undefined) symbol */
void add_reloc(int sec, u32 off, int symidx, u32 type);
void add_fixup(int sec, u32 off, int local_num);
void local_define(int n, u32 value);                    /* numeric local label N: at value */
int  local_defined(int n);
u32  local_value(int n);

/* ---- MACHINE-DEPENDENT backend (arm.c) ----------------------------------------------------------- */
void md_assemble(char **toks, int ntok);   /* encode ONE instruction (toks[0]=mnemonic) into cursec */
void md_apply_fix(const Fixup *f);          /* patch a resolved forward-local branch (arch encoding) */
int  md_directive(char **toks, int ntok);   /* arch pseudo-ops (.cpu/.fpu/…); 1 = handled, 0 = not ours */
void md_finish(void);                       /* end of pass: resolve arch-internal fixups (ldr literals) */
extern const u16 md_e_machine;              /* ELF e_machine (EM_ARM) */
extern const u32 md_e_flags;                /* ELF e_flags (EABI version) */
extern const u32 md_r_abs32;                /* the arch's 32-bit absolute reloc (for `.word <symbol>`) */
extern const u32 md_r_got_prel;             /* the arch's PC-relative GOT-entry reloc (for `.word <symbol>(GOT)`) */

/* ---- OBJECT backend (elf.c) ---------------------------------------------------------------------- */
void obj_write(const char *path);
#endif
