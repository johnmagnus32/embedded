/*
 * common/elfver.h — a file's SYMBOL VERSIONS (.gnu.version + .gnu.version_d / .gnu.version_r), shared by the
 * tools that report them (objdump, readelf). A file carrying a version symbol table AND some version definitions
 * or needs gives every dynamic symbol a version: index 0 none, index 1 the base (the soname, which reads "Base"),
 * anything up to the number of definitions names that definition, and beyond them it is a NEEDED version found by
 * its vna_other. VERSYM_HIDDEN marks a symbol bound to an old version — the callers render that differently
 * (objdump "(name)" in a column, readelf a single "@" instead of "@@").
 */
#ifndef OS_ELFVER_H
#define OS_ELFVER_H
#include "elfread.h"

#define VERSYM_HIDDEN 0x8000u

typedef struct { const u16 *versym; char **verdef; int nverdef; char **verneed; int nverneed; int active; } Versions;

/* Parse the three sections; `active` is 0 when the file carries no versions at all. */
void versions_load(const ElfFile *f, Versions *v);
/* The version name for dynamic-symbol index `i` ("" when it has none), or NULL when the file has no versions.
 * *hidden is set when the symbol is bound to a non-default version. */
const char *version_of(const Versions *v, int i, int *hidden);
#endif
