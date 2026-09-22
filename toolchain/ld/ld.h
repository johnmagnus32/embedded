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
extern int shared;              /* -shared: emit an ET_DYN LIBRARY (exports .dynsym/.hash; no entry point) */
extern const char *soname;      /* -soname NAME (default: output basename) -> DT_SONAME */
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
	u32 *sec_vaddr;                              /* [nsh] assigned virtual (run) address of each allocated section */
	u32 *sec_lma;                                /* [nsh] assigned load address (== sec_vaddr unless AT> in the script) */
	int active;                                  /* 1 = contributes to output; archive members start 0 (lazy) */
} Obj;
#define MAXOBJ 32
extern Obj objs[]; extern int nobj;

/* -shared exports: the global/weak DEFINED symbols this .so publishes into .dynsym/.hash, so other
 * objects (and our runtime loader's dso_lookup) can resolve against it. Collected before layout (names
 * size the tables); each symbol's final vaddr is read from its obj at write time. */
typedef struct { const char *name; Obj *obj; int symidx; } Export;
#define MAXEXPORT 8192
extern Export exports[]; extern int nexport;

/* -l/-L: a shared library we link AGAINST (a provider). We read its exports + soname; we do NOT lay out
 * its sections. An undefined ref matching one of its exports becomes an IMPORT resolved at runtime. */
typedef struct { const char *soname; int used; } ShLib;    /* used=1 -> emit a DT_NEEDED for it */
#define MAXSHLIB 16
extern ShLib shlibs[]; extern int nshlib;
typedef struct { const char *name; int lib; u32 size; } ShExport;  /* a symbol a provider exports (+ its size) */
#define MAXSHEXPORT 16384
extern ShExport shexports[]; extern int nshexport;

/* An IMPORT the consumer resolves at runtime: an undefined symbol found in a provider. A CALLED import
 * gets a PLT stub + GOT slot + JUMP_SLOT reloc (plt_index >= 0). A DATA import (addressed via an ABS32
 * literal) gets a copy of the variable in the exe's own .dynbss + an R_ARM_COPY (is_data=1). dynsym_index
 * is its slot in the output .dynsym. */
typedef struct { const char *name; int lib; int plt_index; int dynsym_index; int is_data; u32 copy_vaddr, copy_size; } Import;
#define MAXIMPORT 8192
extern Import imports[]; extern int nimport;
extern int nplt;                /* PLT/GOT/rel.plt entry count (== number of distinct CALLED imports) */
#define PLTENT 16               /* bytes per PIC .plt stub: ldr ip,[pc,#4]; add ip,pc,ip; ldr pc,[ip]; .word got-pc
                                 * (PC-relative to the GOT slot -> bias-invariant, no reloc, W^X-clean in a .so) */

/* Copy relocations (.rel.dyn, R_ARM_COPY): one per DATA import — the loader memcpy's the variable from
 * its provider into the exe's .dynbss slot at r_offset. Collected by relocate(), emitted after RELATIVE. */
typedef struct { u32 offset; u32 dynsym_index; } CopyRel;
#define MAXCOPYREL 4096
extern CopyRel copyrel[]; extern int ncopyrel;

/* PIC GOT (from R_ARM_GOT_PREL): one slot per distinct referenced symbol. A LOCAL-defined symbol's slot
 * holds its link-time address + gets an R_ARM_RELATIVE (loader adds the bias); an IMPORTED symbol's slot
 * is 0 + gets an R_ARM_GLOB_DAT against its .dynsym entry (loader writes the resolved address). Built
 * before layout so the count sizes .got; each slot's vaddr is assigned in layout. */
typedef struct { const char *name; int is_import; int dynsym_index; u32 vaddr; u32 symval;
                 Obj *def_obj; int def_symidx; } GotEnt;   /* def_obj/def_symidx: a LOCAL def, to resolve symval post-layout */
#define MAXGOT 8192
extern GotEnt gotents[]; extern int ngotent;
/* GLOB_DAT relocations (.rel.dyn, R_ARM_GLOB_DAT): one per IMPORTED GOT slot. */
typedef struct { u32 offset; u32 dynsym_index; } GlobDat;
#define MAXGLOBDAT 8192
extern GlobDat globdat[]; extern int nglobdat;

/* Linker-script driven layout (subset): when `-T script.ld` is given, the script (not the built-in
 * 2-segment model) places sections + defines symbols. outsecs[] is the resulting output-section list
 * (name + placement), used to emit section headers; the actual bytes come from each input section's
 * assigned sec_vaddr, exactly as the normal path. */
typedef struct { char name[64]; u32 vaddr, lma, size; int nobits; int exec, write; } OutSec;   /* lma: load addr (== vaddr unless AT>) */
#define MAXOUTSEC 64
extern OutSec outsecs[]; extern int noutsec;
extern int scripted;                                               /* 1 = a linker script drives layout */
int  script_run(const char *path);                                 /* parse + lay out per the script; 1 if used */
void elf_write_script(const char *out, u32 entry);                 /* write the ET_EXEC from the script layout */

void die(const char *fmt, ...);                                    /* front-end (ld.c) */
u32  pick_nbucket(u32 nsyms);                                      /* .hash bucket count (ld.c; used by elf.c) */

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
	u32 dynamic_count;                          /* number of Elf32_Dyn entries (varies: pie vs shared)       */
	/* -shared / dynamic consumer: the dynamic symbol table, also in the read-only seg-0 tail. */
	u32 hash_vaddr, hash_off, hash_sz;          /* .hash: SysV [nbucket, nchain, bucket[], chain[]]          */
	u32 dynsym_vaddr, dynsym_off, dynsym_sz;    /* .dynsym: (1 + nimport + nexport) * sizeof(Elf32_Sym)      */
	u32 dynstr_vaddr, dynstr_off, dynstr_sz;    /* .dynstr: '\0' + import/export names + needed sonames      */
	/* dynamic CONSUMER only (imports from a .so): PT_INTERP + a PLT (R-X) with its GOT (R-W). */
	u32 interp_vaddr, interp_off, interp_sz;    /* .interp: the ld.so path string                            */
	u32 plt_vaddr, plt_off, plt_sz;             /* .plt: nplt * PLTENT  (R-X code)                           */
	u32 relplt_vaddr, relplt_off, relplt_sz;    /* .rel.plt: nplt * sizeof(Elf32_Rel) (JUMP_SLOTs -> DT_JMPREL) */
	u32 gotplt_vaddr, gotplt_off, gotplt_sz;    /* .got.plt: nplt * 4  (R-W: loader writes resolved addrs)   */
	u32 got_vaddr, got_off, got_sz;             /* .got: ngotent * 4 (R-W PIC slots; RELATIVE/GLOB_DAT)      */
	u32 dynbss_vaddr, dynbss_sz;                /* .dynbss: copy-reloc slots for DATA imports (R-W NOBITS)   */
} Layout;

#define INTERP_PATH "/lib/ld.so.1"   /* the runtime loader a dynamic consumer names in PT_INTERP */

/* ---- OBJECT-FORMAT backend (elf.c) --------------------------------------------------------------- */
Obj *elf_load(const char *path);                                   /* parse one .o file into objs[] (active) */
Obj *elf_parse(const char *path, u8 *data, long size, int active); /* parse an in-memory ELF image into objs[] */
void ar_load(const char *path);                                    /* split a .a into objs[] as LAZY members  */
void load_shared(const char *path);                                /* read a .so's exports+soname (a provider) */
void elf_write_exec(const char *out, u32 entry, const Layout *L);

/* ---- MACHINE-DEPENDENT backend (arm.c) ----------------------------------------------------------- */
extern const u16 md_e_machine;                                     /* EM_ARM — checked on load, stamped on write */
extern const u32 md_r_relative;                                    /* the arch's base-fixup reloc (R_ARM_RELATIVE) */
extern const u32 md_r_jump_slot;                                   /* the arch's PLT reloc (R_ARM_JUMP_SLOT) */
extern const u32 md_r_copy;                                        /* the arch's data-import reloc (R_ARM_COPY) */
extern const u32 md_r_got_prel;                                    /* the arch's PIC GOT-entry reloc (R_ARM_GOT_PREL) */
extern const u32 md_r_glob_dat;                                    /* the arch's GOT-import reloc (R_ARM_GLOB_DAT) */
void md_apply_reloc(Obj *o, u32 type, u8 *loc, u32 S, u32 P);      /* patch one relocation in place */
int  md_needs_dynamic_reloc(u32 type);                             /* 1 if this reloc must become a runtime RELATIVE */
int  md_is_call_reloc(u32 type);                                   /* 1 if a CALL-type reloc (route via PLT if imported) */
int  md_is_got_reloc(u32 type);                                    /* 1 if a PIC GOT-entry reloc (resolve to GOT slot) */
#endif
