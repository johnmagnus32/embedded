/*
 * stdint.h — C99 fixed-width integer types (ARM EABI, ILP32).
 *
 * Self-contained: built entirely on the compiler's own width macros (__INT8_TYPE__,
 * __INT8_MAX__, __INT64_C, ...), so it needs no other header. That lets ONE header serve both
 * builds of this libc:
 *   - LIBC=custom (hermetic -nostdinc): our -I libc/include is searched first, so <stdint.h>
 *     resolves HERE instead of the compiler's freestanding copy.
 *   - toolchain-bootstrap sysroot: GCC's arm-linux stdint.h is the `wrap` variant
 *     (#include_next <stdint.h>) — it chains to THIS file, so a NORMAL cross-link + the pass-2
 *     libgcc build (which pulls <stdint.h> via <abi.h>) both find a complete stdint.h.
 */
#ifndef _LIBC_STDINT_H
#define _LIBC_STDINT_H

typedef __INT8_TYPE__    int8_t;
typedef __INT16_TYPE__   int16_t;
typedef __INT32_TYPE__   int32_t;
typedef __INT64_TYPE__   int64_t;
typedef __UINT8_TYPE__   uint8_t;
typedef __UINT16_TYPE__  uint16_t;
typedef __UINT32_TYPE__  uint32_t;
typedef __UINT64_TYPE__  uint64_t;

typedef __INTPTR_TYPE__  intptr_t;
typedef __UINTPTR_TYPE__ uintptr_t;
typedef __INTMAX_TYPE__  intmax_t;
typedef __UINTMAX_TYPE__ uintmax_t;

/* least/fast widths map to the exact-width types (adequate for ILP32 ARM). */
typedef int8_t   int_least8_t;   typedef uint8_t   uint_least8_t;
typedef int16_t  int_least16_t;  typedef uint16_t  uint_least16_t;
typedef int32_t  int_least32_t;  typedef uint32_t  uint_least32_t;
typedef int64_t  int_least64_t;  typedef uint64_t  uint_least64_t;
typedef int32_t  int_fast8_t;    typedef uint32_t  uint_fast8_t;
typedef int32_t  int_fast16_t;   typedef uint32_t  uint_fast16_t;
typedef int32_t  int_fast32_t;   typedef uint32_t  uint_fast32_t;
typedef int64_t  int_fast64_t;   typedef uint64_t  uint_fast64_t;

#define INT8_MAX    __INT8_MAX__
#define INT16_MAX   __INT16_MAX__
#define INT32_MAX   __INT32_MAX__
#define INT64_MAX   __INT64_MAX__
#define INT8_MIN    (-INT8_MAX  - 1)
#define INT16_MIN   (-INT16_MAX - 1)
#define INT32_MIN   (-INT32_MAX - 1)
#define INT64_MIN   (-INT64_MAX - 1)
#define UINT8_MAX   __UINT8_MAX__
#define UINT16_MAX  __UINT16_MAX__
#define UINT32_MAX  __UINT32_MAX__
#define UINT64_MAX  __UINT64_MAX__

#define INT_LEAST8_MAX   INT8_MAX
#define INT_LEAST16_MAX  INT16_MAX
#define INT_LEAST32_MAX  INT32_MAX
#define INT_LEAST64_MAX  INT64_MAX
#define UINT_LEAST8_MAX  UINT8_MAX
#define UINT_LEAST16_MAX UINT16_MAX
#define UINT_LEAST32_MAX UINT32_MAX
#define UINT_LEAST64_MAX UINT64_MAX

#define INTPTR_MAX  __INTPTR_MAX__
#define INTPTR_MIN  (-INTPTR_MAX - 1)
#define UINTPTR_MAX __UINTPTR_MAX__
#define INTMAX_MAX  __INTMAX_MAX__
#define INTMAX_MIN  (-INTMAX_MAX - 1)
#define UINTMAX_MAX __UINTMAX_MAX__
#define SIZE_MAX    __SIZE_MAX__
#define PTRDIFF_MAX __PTRDIFF_MAX__
#define PTRDIFF_MIN (-PTRDIFF_MAX - 1)

#define INT8_C(c)    c
#define INT16_C(c)   c
#define INT32_C(c)   c
#define INT64_C(c)   __INT64_C(c)
#define UINT8_C(c)   c
#define UINT16_C(c)  c
#define UINT32_C(c)  c ## U
#define UINT64_C(c)  __UINT64_C(c)
#define INTMAX_C(c)  __INTMAX_C(c)
#define UINTMAX_C(c) __UINTMAX_C(c)

#endif /* _LIBC_STDINT_H */
