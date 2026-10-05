/* stdarg.h — the compiler's (C11 7.16): the variable-argument builtins. */
#ifndef __os_stdarg_h
#define __os_stdarg_h
#ifndef __GNUC_VA_LIST
#define __GNUC_VA_LIST
typedef __builtin_va_list __gnuc_va_list;
#endif
typedef __gnuc_va_list va_list;
#define va_start(v, l) __builtin_va_start(v, l)
#define va_end(v) __builtin_va_end(v)
#define va_arg(v, t) __builtin_va_arg(v, t)
#define va_copy(d, s) __builtin_va_copy(d, s)
#define __va_copy(d, s) __builtin_va_copy(d, s)
#endif
