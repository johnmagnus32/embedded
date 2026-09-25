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
typedef struct { char *name; int sec; u32 value, size; int global, type, defined, weak; } Sym;
/* global: 1 if .global'd. A symbol is emitted LOCAL iff (defined && !global); undefined or .global'd
 * symbols are GLOBAL. So compiler-internal labels (.L…, not .global'd) are local, like GNU as. */
typedef struct { int sec; u32 off; int symidx; u32 type; } Reloc;   /* type is an md-supplied reloc code */

#define MAXSEC 32
#define MAXSYM 65536  /* a big preprocessed kernel .c emits tens of thousands of .L labels + symbols */
#define MAXREL 65536
extern Section secs[]; extern int nsec, cursec;   /* cursec = active section index into secs[] */
extern Sym syms[]; extern int nsym;
extern Reloc rels[]; extern int nrel;

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
/* Numeric local labels (GAS "fb" labels): each definition of N: is a hidden local symbol `.Lfb<N>$<k>` (k = the
 * definition's ordinal); `Nb` = the latest one, `Nf` = the next. fb_symbol returns the symbol index. */
int  fb_symbol(int n, char dir);
#define SEC_ABS (-2)                                    /* Sym.sec of an absolute symbol (`.equ N, 16`) */
long eval_const_expr(const char *s);
void eval_reloc_expr(const char *s, long *c, int *sym, int *dot);   /* c + sym - dot*P, junk is an error */
void map_insn(void); void map_pool_data(void);                    /* a constant expression (`.`, same-section `a - b`, abs syms); dies otherwise */
int  section_symbol(int sec);                           /* find-or-create sec's STT_SECTION symbol (reloc target) */
int  parse_local_ref(const char *s, int *n, char *dir); /* "123b"/"7f" (then a non-ident char): bytes consumed, else 0 */

/* ---- MACHINE-DEPENDENT backend (arm.c) ----------------------------------------------------------- */
void md_assemble(char **toks, int ntok);   /* encode ONE instruction (toks[0]=mnemonic) into cursec */
int  md_directive(char **toks, int ntok);   /* arch pseudo-ops (.cpu/.fpu/…); 1 = handled, 0 = not ours */
void md_flush_pools(void);                   /* end of parsing: dump pending literal pools into their sections */
int  md_is_branch_reloc(u32 type);            /* b/bl reloc (imm24 addend in place)? */
void md_finish(void);
void md_emit_attributes(void);                /* after all symbols: write .ARM.attributes (arch backend) */                       /* end of pass: resolve arch-internal fixups (ldr literals) */
extern const u16 md_e_machine;              /* ELF e_machine (EM_ARM) */
extern const u32 md_e_flags;                /* ELF e_flags (EABI version) */
extern const u32 md_r_abs32;                /* the arch's 32-bit absolute reloc (for `.word <symbol>`) */
extern const u32 md_r_rel32;                /* the arch's 32-bit PC-relative reloc (for `.word <symbol> - .`) */
int  md_reloc_operator(const char *op, u32 *type);    /* `sym(OP)` in a data word: arch reloc type, 0 if unknown */
u32  md_data_reloc_for(const char *sym, u32 dflt); /* e.g. `.word _GLOBAL_OFFSET_TABLE_` -> GOTPC */
void md_req(const char *alias, const char *regname); void md_unreq(const char *alias);

/* ---- OBJECT backend (elf.c) ---------------------------------------------------------------------- */
void obj_write(const char *path);
#endif
