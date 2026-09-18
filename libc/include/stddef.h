/*
 * stddef.h — freestanding fundamental types/macros. We DON'T use the compiler's
 * own <stddef.h> here so the header set is fully self-contained and obviously
 * ours; the definitions match the ARM EABI (size_t = unsigned int, etc.).
 */
#ifndef _LIBC_STDDEF_H
#define _LIBC_STDDEF_H

typedef unsigned int   size_t;    /* ARM32: 32-bit */
typedef long           ptrdiff_t;
/* NOTE: ssize_t is a POSIX type and lives in <sys/types.h>, NOT here. Keeping it out of stddef.h
 * matters for the conforming toolchain-bootstrap sysroot: there the compiler's own <stddef.h>
 * shadows this one, and it has no ssize_t — so anything needing ssize_t must include sys/types.h. */

#ifndef NULL
#define NULL ((void *)0)
#endif

#define offsetof(t, m) __builtin_offsetof(t, m)

#endif /* _LIBC_STDDEF_H */
