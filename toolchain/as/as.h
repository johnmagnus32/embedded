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
typedef struct { char *name; u32 type, flags; u8 *data; size_t len, cap; int shndx; u32 align, entsize;
                 int mapstate, lastmap, secsym, link;
                 int base, subsec, merged, lead, fragmode, first_align_mode; u32 first_align, pad_align;
                 int nsub; } Section;   /* nsub: how many SUBSECTIONS name this section as their base (see merge_subsections) */   /* base >= 0: an internal SUBSECTION of secs[base] (merged
                                                                          * into it at the end, then skipped); first_align: an
                                                                          * .align before its first byte; lead: that .align's
                                                                          * own mapping symbol (-1 none); fragmode: the ARM(0)/
                                                                          * Thumb(1) state of the last instruction in its current
                                                                          * GAS frag, -1 none yet (the current state then) — it
                                                                          * picks an alignment's NOPs and mapping symbol */   /* align: sh_addralign (GAS: max requested; an ARM insn = 4);
                                                                 * entsize: M sections; mapstate/lastmap: its mapping-symbol
                                                                 * state + latest $a/$d (-1 none); secsym: its STT_SECTION symbol */
typedef struct { char *name; int sec; u32 value, size; int global, type, defined, weak, sized, vis, thumb, thumb_func; } Sym;   /* sized: had a .size;
                                                                                                         * vis: STV_* (.hidden …);
                                                                                                         * thumb: a label in Thumb code
                                                                                                         * (a FUNC one gets value|1);
                                                                                                         * thumb_func: .thumb_func's */
/* global: 1 if .global'd. A symbol is emitted LOCAL iff (defined && !global); undefined or .global'd
 * symbols are GLOBAL. So compiler-internal labels (.L…, not .global'd) are local, like GNU as. */
typedef struct { int sec; u32 off; int symidx; u32 type; u32 seq; int sub, dir; } Reloc;   /* sub: the subsection it was made in;
                                                                             * dir: from a .reloc directive (GAS's separate list) */   /* type: an md-supplied reloc code; seq: when GAS would
                                                                             * have made its fixup (the .rel section is in that order) */
extern u32 parse_seq;                                   /* the statement being assembled (fixup creation order) */
extern long reloc_seq;                                  /* >= 0: add_reloc uses this seq (a fixup recorded earlier, placed now) */

/* The tables grow as needed (a big preprocessed kernel .c emits tens of thousands of .L labels + symbols); names are
 * found through a hash index, so interning stays O(1) per symbol. */
extern Section *secs; extern int nsec, cursec;   /* cursec = active section index into secs[] */
extern Sym *syms; extern int nsym;
extern Reloc *rels; extern int nrel;
int sym_add(Sym s);
void *grow(void *v, int n, int *cap, size_t esz);   /* room for element n of a growable table (doubling) */                              /* append a symbol; a named one joins the name index (first wins) */

/* ---- FRONT-END services (as.c), called by both backends ------------------------------------------ */
__attribute__((noreturn, format(printf, 1, 2))) void die(const char *fmt, ...);
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
/* An instruction's constant operand that may name later labels (`#((end - start + 256) & ~255)`): 1 and *v if it's
 * known now, 0 if it must wait — then const_expr_at evaluates the text, `.` = at in sec, once every label is known. */
int const_expr_soft(const char *s, long *v);
long const_expr_at(const char *s, int sec, u32 at);
/* A whole operand expression, unresolved: c + sym - msym - dot*P (sym/msym -1: none; dot: +1 `.`, -1 `- .`, where P is
 * the location it is used at). expr_fold: a `sym - msym` now both defined in one section becomes part of c. */
typedef struct { long c; int sym, dot, msym; u32 rtype; } XVal;
XVal expr_x(const char *s);
void expr_fold(XVal *v);
void eval_reloc_expr(const char *s, long *c, int *sym, int *dot);   /* c + sym - dot*P, junk is an error */
extern int thumb_mode;                                  /* 0: ARM state, 1: Thumb (.thumb / .code 16) */
void emit16(u32 h);                                      /* append a little-endian halfword (a Thumb instruction) */
void mode_switch(int to_thumb);                          /* .arm / .thumb: GAS's opcode_select */
void thumb_func_next_label(void);                        /* .thumb_func: the next (non-.L) label is a Thumb function */
void map_insn(void); void map_pool_data(void);                    /* a constant expression (`.`, same-section `a - b`, abs syms); dies otherwise */
int  section_symbol(int sec);                           /* find-or-create sec's STT_SECTION symbol (reloc target) */
void map_section_data(void);                            /* data follows in cursec: a $d here unless it is already data */
int  parse_local_ref(const char *s, int *n, char *dir); /* "123b"/"7f" (then a non-ident char): bytes consumed, else 0 */

/* ---- MACHINE-DEPENDENT backend (arm.c) ----------------------------------------------------------- */
void md_assemble(char **toks, int ntok);   /* encode ONE instruction (toks[0]=mnemonic) into cursec */
int  md_directive(char **toks, int ntok);   /* arch pseudo-ops (.cpu/.fpu/…); 1 = handled, 0 = not ours */
int  md_parse_option(const char *opt);        /* an arch command-line option (-march= ...); 1 = handled */
u32  md_reloc_by_name(const char *name);       /* .reloc: R_ARM_NAME -> its number (dies if unknown) */
void md_remap_section(int from, int to, u32 delta);   /* a subsection merged: its fixups now at to:off+delta */
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
int  md_is_tls_reloc(u32 type);                  /* a thread-local relocation: its symbol is STT_TLS */
void md_req(const char *alias, const char *regname); void md_unreq(const char *alias);

/* ---- OBJECT backend (elf.c) ---------------------------------------------------------------------- */
void obj_write(const char *path);
extern int file_front;                                   /* the .file symbol GAS puts first (-1: none) */

/* ---- DWARF line table (dwline.c): `.file N "name"` / `.loc N LINE` -> .debug_line ---------------- */
void dwline_file(int n, const char *name);
void dwline_loc(int file, int line, int col, int sec, u32 off);
int  dwline_have_files(void);     /* a numbered .file was seen, so `.file "x"` is the DWARF form, not a symbol */
void dwline_emit(void);           /* build the section; call once, after every address is known */
void dwinfo_emit(void);           /* a minimal CU so addr2line can find the line program at all */

#endif
