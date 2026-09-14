/*
 * stdarg.h — variadic-argument access. Self-contained (like the rest of our headers): built on the
 * compiler's va builtins, which our own cc implements, so the header set has no dependency on the
 * host compiler's <stdarg.h>.
 */
#ifndef _LIBC_STDARG_H
#define _LIBC_STDARG_H

typedef __builtin_va_list va_list;

#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)
#define va_copy(dst, src)  __builtin_va_copy(dst, src)

#endif /* _LIBC_STDARG_H */
