/*
 * stddef.h — freestanding fundamental types/macros. We DON'T use the compiler's
 * own <stddef.h> here so the header set is fully self-contained and obviously
 * ours; the definitions match the ARM EABI (size_t = unsigned int, etc.).
 */
#ifndef _GV3_STDDEF_H
#define _GV3_STDDEF_H

typedef unsigned int   size_t;    /* ARM32: 32-bit */
typedef int            ssize_t;
typedef long           ptrdiff_t;

#ifndef NULL
#define NULL ((void *)0)
#endif

#define offsetof(t, m) __builtin_offsetof(t, m)

#endif /* _GV3_STDDEF_H */
