/* stddef.h — the compiler's (C11 7.19), from its predefined types. A libc's header that wants only some of it
 * (`#define __need_size_t` first, glibc-style) gets only those. */
#if !defined(__need_size_t) && !defined(__need_ptrdiff_t) && !defined(__need_wchar_t) && !defined(__need_NULL) \
    && !defined(__need_wint_t) && !defined(__need_max_align_t)
#define __os_stddef_all
#endif
#if (defined(__os_stddef_all) || defined(__need_ptrdiff_t)) && !defined(__os_ptrdiff_t)
#define __os_ptrdiff_t
typedef __PTRDIFF_TYPE__ ptrdiff_t;
#endif
#if (defined(__os_stddef_all) || defined(__need_size_t)) && !defined(__os_size_t)
#define __os_size_t
typedef __SIZE_TYPE__ size_t;
#endif
#if (defined(__os_stddef_all) || defined(__need_wchar_t)) && !defined(__os_wchar_t)
#define __os_wchar_t
typedef __WCHAR_TYPE__ wchar_t;
#endif
#if defined(__need_wint_t) && !defined(__os_wint_t)
#define __os_wint_t
typedef __WINT_TYPE__ wint_t;
#endif
#if (defined(__os_stddef_all) || defined(__need_max_align_t)) && !defined(__os_max_align_t)
#define __os_max_align_t
typedef struct {
	long long __max_align_ll __attribute__((__aligned__(__alignof__(long long))));
	long double __max_align_ld __attribute__((__aligned__(__alignof__(long double))));
} max_align_t;
#endif
#if defined(__os_stddef_all) || defined(__need_NULL)
#undef NULL
#define NULL ((void *)0)
#endif
#if defined(__os_stddef_all) && !defined(offsetof)
#define offsetof(type, member) __builtin_offsetof(type, member)
#endif
#undef __os_stddef_all
#undef __need_size_t
#undef __need_ptrdiff_t
#undef __need_wchar_t
#undef __need_NULL
#undef __need_wint_t
#undef __need_max_align_t
