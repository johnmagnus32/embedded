/*
 * ld.h — the interface between the linker's layers (mirrors the assembler's split, and GNU's):
 *
 *   FRONT-END  (ld.c)       generic linking ALGORITHM: load inputs, resolve the global symbol table, drive
 *                           layout, apply relocations (via the md backend).
 *   LAYOUT     (script.c)   the ONE layout engine: a linker script (-T, or the built-in default script)
 *                           maps input sections to output sections, assigns run/load addresses, and groups
 *                           output sections into loadable segments.
 *   DYNAMIC    (dynamic.c)  dynamic-linking tables (imports, PLT, GOT, .dynsym/.hash/.dynstr/.dynamic,
 *                           dynamic relocations): built ONCE from one relocation scan, carried in linker-made
 *                           sections that the layout engine places like any input section.
 *   OBJ BACKEND (elf.c)     object-FORMAT layer: read ELF32 relocatables / archives / shared objects, and
 *                           serialize the output image.  (~BFD)
 *   MD BACKEND (arm.c)      machine-dependent: relocation encodings, PLT stubs, the ELF machine id.
 *                           THE only architecture-specific file.  (~bfd/elf32-arm)
 *
 * The ELF format itself (structs + constants) + low-level helpers are shared with `as` via
 * common/elf.h + common/elfutil.h — our miniature libbfd.
 */
#ifndef LD_H
#define LD_H
#include "elf.h"        /* shared ELF32 format: structs + constants (toolchain/common) */
#include "elfutil.h"    /* shared helpers: rd32/wr32, alignup, Strtab */
#include "strmap.h"     /* shared string-keyed hash map */

extern u32 load_base;           /* default layout: where the image (headers included) starts; -Ttext <addr> */
extern int pie;                 /* -pie: ET_DYN at base 0; absolute refs become load-bias fixups */
extern int shared;              /* -shared: an ET_DYN LIBRARY (exports .dynsym/.hash; no required entry) */
extern const char *soname;      /* -soname NAME (default: output basename) -> DT_SONAME */
extern const char *entry_sym;   /* entry symbol: -e wins over the script's ENTRY() */
#define PAGE 0x1000u            /* segment alignment: each PT_LOAD maps on its own pages => W^X enforceable */

void  die(const char *fmt, ...) __attribute__((noreturn));
void *grow(void *v, int n, int *cap, size_t esz);   /* ensure room for element n (doubling); returns v */

/* ---- inputs ------------------------------------------------------------------------------------ */
struct OutSec;
typedef struct {
	const char *path; u8 *data; long size;      /* whole file (mutable — relocations patch it in place) */
	Elf32_Ehdr *eh; Elf32_Shdr *sh; int nsh; const char *shstr;
	Elf32_Sym *sym; int nsym; const char *strtab;
	u32 *sec_vaddr;                              /* [nsh] run address of each placed section               */
	u32 *sec_lma;                                /* [nsh] load address (== sec_vaddr unless AT/AT> moves it) */
	struct OutSec **sec_out;                     /* [nsh] its output section; NULL = unplaced; &os_discard  */
	int active;                                  /* 1 = contributes to output; archive members join when pulled */
} Obj;
extern Obj **objs; extern int nobj;              /* every loaded object; archive members join when pulled */
Obj *obj_new(void);
static inline const char *sec_name(const Obj *o, int j) { return o->shstr + o->sh[j].sh_name; }

Obj *elf_load(const char *path);                 /* elf.c: an always-linked object file            */
void ar_load(const char *path);                  /* elf.c: register an archive's symbol index      */
Obj *ar_pull(const char *sym);                   /* elf.c: load the member defining sym, or NULL    */
void load_shared(const char *path);              /* elf.c: read a .so's exports + soname (a provider) */

/* -l: a shared library we link AGAINST (a provider): its exports + soname; its sections are not laid out. */
typedef struct { const char *soname; int used; } ShLib;           /* used -> a DT_NEEDED */
typedef struct { const char *name; int lib; u32 size; } ShExport; /* a provider's exported symbol (+ its size) */
extern ShLib *shlibs; extern int nshlib;
extern ShExport *shexports; extern int nshexport;

/* ---- the global symbol table (hashed) ---------------------------------------------------------- */
/* An object definition records where (obj/symidx) until layout gives it an address. A LINKER-SCRIPT symbol
 * (obj == NULL) is declared when the script is read (linker = 1) and gets its value during layout; abs = it was
 * assigned outside SECTIONS (an absolute value, not an image address); os = the output section it follows. */
typedef struct { const char *name; u32 vaddr; int defined, weak, strong_ref; Obj *obj; int symidx;
                 int linker, abs, hidden; struct OutSec *os; } GSym;
GSym *gsym_find(const char *name);
GSym *gsym_get(const char *name);                /* find or create (an undefined, unreferenced entry) */
int   defined_locally(const char *name);         /* defined by one of our objects or by the script */
u32   sym_addr(const Obj *o, int symidx);        /* a DEFINED object symbol's final address */
int   sym_is_abs(const Obj *o, int symidx);      /* does the symbol (after resolution) have an absolute value? */

/* ---- output sections + segments (script.c) ------------------------------------------------------ */
typedef struct { Obj *obj; int shndx; } InSec;   /* an input section placed in an output section */
typedef struct OutSec {
	const char *name;
	u32 type, flags, entsize;                    /* from the inputs: PROGBITS/NOBITS/…; SHF_* union     */
	u32 vaddr, lma, size, off;                   /* run address, load address, size, file offset        */
	InSec first;                                 /* its first input (sh_link/sh_info carry over from it) */
	int index;                                   /* section-header index in the output (0 = not emitted) */
	int seg;                                     /* its PT_LOAD (segs[] index)                          */
	int done;                                    /* addresses assigned (this layout pass)               */
} OutSec;
extern OutSec os_discard;                        /* sec_out marker: matched by /DISCARD/                 */
extern OutSec **outsecs; extern int noutsec;     /* emitted output sections, in address-assignment order */

typedef struct { u32 vaddr, lma, off, filesz, memsz, flags; } Seg;   /* one PT_LOAD */
extern Seg *segs; extern int nseg;
extern Elf32_Phdr *phdrs; extern int nphdr;      /* the program header table (PT_LOADs + PHDR/INTERP/DYNAMIC/…) */
extern u32 hdrsz;                                /* ELF header + program header table bytes            */

void script_read(const char *path);              /* -T script (NULL = the built-in default script)    */
extern GSym **declared; extern int ndeclared;    /* the symbols the script defines, in script order     */
void layout_match(Obj *o);                       /* map o's allocatable sections to output sections   */
void layout_run(void);                           /* orphans, addresses, segments, output indices       */
OutSec *outsec_named(const char *name);          /* an emitted output section by name, or NULL        */

/* ---- dynamic linking (dynamic.c) ---------------------------------------------------------------- */
extern Obj *linker_obj;                          /* the linker-made sections (.got, .plt, .dynamic, …)  */
enum { L_NULL, L_INTERP, L_HASH, L_DYNSYM, L_DYNSTR, L_RELDYN, L_RELPLT, L_PLT, L_DYNAMIC, L_GOTPLT, L_GOT, L_DYNBSS, L_NSEC };
void dyn_init(void);                             /* create linker_obj (its sections empty)             */
void dyn_scan(void);                             /* ONE relocation scan -> imports / PLT / GOT / dynamic relocs */
void dyn_size(void);                             /* size linker_obj's sections from the tables built    */
void dyn_fill(void);                             /* after layout: their contents                       */
int  dyn_target(Obj *o, int symidx, u32 type, u32 *S);   /* relocation target inside a linker-made table */

/* ---- output (elf.c) ------------------------------------------------------------------------------ */
void elf_write(const char *out, u32 entry);

/* ---- MACHINE-DEPENDENT backend (arm.c) ----------------------------------------------------------- */
extern const u16 md_e_machine;                   /* EM_ARM — checked on load, stamped on write */
extern const u32 md_r_relative, md_r_jump_slot, md_r_copy, md_r_glob_dat, md_r_abs32;   /* dynamic relocation types */
extern const u32 md_plt_entsize;                 /* bytes per PLT stub */
void md_apply_reloc(Obj *o, u32 type, u8 *loc, u32 S, u32 P);   /* patch one relocation in place */
void md_plt_entry(u8 *p, u32 stub, u32 slot);    /* one PLT stub at vaddr `stub` jumping through GOT `slot` */
int  md_needs_dynamic_reloc(u32 type);           /* an absolute word: a runtime RELATIVE in a PIE/.so  */
int  md_is_abs_nonword(u32 type);                /* movw/movt absolute halves: not position-independent */
int  md_is_call_reloc(u32 type);                 /* a call (routed via the PLT when imported)          */
int  md_is_got_reloc(u32 type);                  /* a PIC GOT-entry reference (S = the symbol's GOT slot) */
#endif
