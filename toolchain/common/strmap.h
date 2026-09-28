/*
 * common/strmap.h — a string-keyed hash map (open addressing, FNV-1a), shared by the tools for their name
 * tables (the linker's global symbols, the compiler's typedefs/tags/globals). Keys are NOT copied: the caller
 * keeps each key string alive as long as the map. O(1) lookups where the tools used to scan linearly.
 */
#ifndef OS_STRMAP_H
#define OS_STRMAP_H
#include <stddef.h>

typedef struct { const char **keys; void **vals; size_t cap, n; } StrMap;   /* zero-initialized = empty */
void *strmap_get(const StrMap *m, const char *key);             /* the value, or NULL if absent */
void  strmap_put(StrMap *m, const char *key, void *val);         /* insert, or replace an existing key's value */
void  strmap_clear(StrMap *m);                                   /* forget everything (keeps no memory) */

#endif
