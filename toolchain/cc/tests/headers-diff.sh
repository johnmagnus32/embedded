#!/usr/bin/env bash
# headers-diff.sh — the compiler's own headers (cc/include) against GCC's: every constant they define (limits, stdint,
# float, stddef, stdbool, iso646) emitted as a table by our cpp+cc+as and by GCC with its headers — the tables must be
# byte-identical — and every type they define the same as GCC's (size and signedness checked by GCC itself on our
# header's typedefs: _Static_assert). Twice: freestanding (-nostdinc, U-Boot's and the kernel's way — GCC's limits.h
# always defers to a C library's, so it gets an empty one) and hosted (both deferring to the musl sysroot's
# limits.h/stdint.h). Env: GNU=<cross prefix>, T=<toolchain dir>.
set -u
t=$(mktemp -d); trap 'rm -rf $t' EXIT
cat > $t/v.c <<'C'
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <float.h>
#include <stdbool.h>
#include <stdalign.h>
#include <iso646.h>
#define I(x) (unsigned long long)(x),
const unsigned long long ints[] = {
 I(CHAR_BIT) I(MB_LEN_MAX) I(SCHAR_MIN) I(SCHAR_MAX) I(UCHAR_MAX) I(CHAR_MIN) I(CHAR_MAX) I(SHRT_MIN) I(SHRT_MAX) I(USHRT_MAX)
 I(INT_MIN) I(INT_MAX) I(UINT_MAX) I(LONG_MIN) I(LONG_MAX) I(ULONG_MAX) I(LLONG_MIN) I(LLONG_MAX) I(ULLONG_MAX)
 I(INT8_MIN) I(INT8_MAX) I(UINT8_MAX) I(INT16_MIN) I(INT16_MAX) I(UINT16_MAX) I(INT32_MIN) I(INT32_MAX) I(UINT32_MAX)
 I(INT64_MIN) I(INT64_MAX) I(UINT64_MAX) I(INT_LEAST8_MIN) I(INT_LEAST8_MAX) I(UINT_LEAST8_MAX) I(INT_LEAST16_MIN)
 I(INT_LEAST16_MAX) I(UINT_LEAST16_MAX) I(INT_LEAST32_MIN) I(INT_LEAST32_MAX) I(UINT_LEAST32_MAX) I(INT_LEAST64_MIN)
 I(INT_LEAST64_MAX) I(UINT_LEAST64_MAX) I(INT_FAST8_MIN) I(INT_FAST8_MAX) I(UINT_FAST8_MAX) I(INT_FAST16_MIN)
 I(INT_FAST16_MAX) I(UINT_FAST16_MAX) I(INT_FAST32_MIN) I(INT_FAST32_MAX) I(UINT_FAST32_MAX) I(INT_FAST64_MIN)
 I(INT_FAST64_MAX) I(UINT_FAST64_MAX) I(INTPTR_MIN) I(INTPTR_MAX) I(UINTPTR_MAX) I(INTMAX_MIN) I(INTMAX_MAX) I(UINTMAX_MAX)
 I(PTRDIFF_MIN) I(PTRDIFF_MAX) I(SIG_ATOMIC_MIN) I(SIG_ATOMIC_MAX) I(SIZE_MAX) I(WCHAR_MIN) I(WCHAR_MAX) I(WINT_MIN) I(WINT_MAX)
 I(INT8_C(-5)) I(UINT16_C(7)) I(INT64_C(1) << 40) I(UINT64_C(0xffffffffffffffff)) I(INTMAX_C(-1)) I(UINTMAX_C(3))
 I(FLT_RADIX) I(FLT_MANT_DIG) I(DBL_MANT_DIG) I(LDBL_MANT_DIG) I(FLT_DIG) I(DBL_DIG) I(LDBL_DIG) I(FLT_MIN_EXP) I(DBL_MIN_EXP)
 I(LDBL_MIN_EXP) I(FLT_MIN_10_EXP) I(DBL_MIN_10_EXP) I(LDBL_MIN_10_EXP) I(FLT_MAX_EXP) I(DBL_MAX_EXP) I(LDBL_MAX_EXP)
 I(FLT_MAX_10_EXP) I(DBL_MAX_10_EXP) I(LDBL_MAX_10_EXP) I(FLT_DECIMAL_DIG) I(DBL_DECIMAL_DIG) I(LDBL_DECIMAL_DIG)
 I(DECIMAL_DIG) I(FLT_HAS_SUBNORM) I(DBL_HAS_SUBNORM) I(LDBL_HAS_SUBNORM) I(FLT_EVAL_METHOD) I(FLT_ROUNDS)
 I(sizeof(size_t)) I(sizeof(ptrdiff_t)) I(sizeof(wchar_t)) I(sizeof(max_align_t)) I(_Alignof(max_align_t)) I(alignof(double))
 I(offsetof(struct { char c; int i; }, i)) I(true) I(false) I(__bool_true_false_are_defined) I(1 and 0) I(1 bitor 6) I(not 0)
 I(NULL == (void *)0) I(__alignas_is_defined)
};
const double flts[] = { FLT_MAX, DBL_MAX, LDBL_MAX, FLT_EPSILON, DBL_EPSILON, LDBL_EPSILON, FLT_MIN, DBL_MIN, LDBL_MIN, FLT_TRUE_MIN, DBL_TRUE_MIN, LDBL_TRUE_MIN };
C
cat > $t/ty.c <<'C'
#include <stddef.h>
#include <stdint.h>
#define SAME(a, b) _Static_assert(__builtin_types_compatible_p(a, b), #a " is " #b);
SAME(size_t, __SIZE_TYPE__) SAME(ptrdiff_t, __PTRDIFF_TYPE__) SAME(wchar_t, __WCHAR_TYPE__) SAME(int8_t, signed char)
SAME(uint8_t, unsigned char) SAME(int16_t, short) SAME(uint16_t, unsigned short) SAME(int32_t, __INT32_TYPE__) SAME(uint32_t, __UINT32_TYPE__)
SAME(int64_t, long long) SAME(uint64_t, unsigned long long) SAME(intptr_t, __INTPTR_TYPE__) SAME(uintptr_t, __UINTPTR_TYPE__)
SAME(intmax_t, long long) SAME(uintmax_t, unsigned long long) SAME(int_fast8_t, signed char) SAME(int_fast16_t, __INT_FAST16_TYPE__)
SAME(int_least16_t, short) SAME(uint_fast32_t, __UINT_FAST32_TYPE__)
int x;
C
gccinc=$("${GNU}gcc" -print-file-name=include); sysroot=$("${GNU}gcc" -print-sysroot)
[ -f "${sysroot}/usr/include/limits.h" ] || { echo "FAIL: no C library headers in GCC's sysroot ${sysroot}"; exit 1; }
mkdir $t/nolibc; : > $t/nolibc/limits.h
n=0
for mode in freestanding hosted; do
  if [ $mode = freestanding ]; then
    gnu=(-ffreestanding -nostdinc -isystem "$gccinc" -isystem $t/nolibc); ours=(-ffreestanding -nostdinc -isystem "$T/cc/include")
  else
    gnu=(-O0); ours=(-nostdinc -isystem "$T/cc/include" -isystem "${sysroot}/usr/include")
  fi
  "${GNU}gcc" "${gnu[@]}" -O0 -c -o $t/g.o $t/v.c || { echo "FAIL ($mode): GCC on its own headers"; exit 1; }
  "$T/cpp/build/cpp" "${ours[@]}" -o $t/v.i $t/v.c && "$T/cc/build/cc" -o $t/v.s $t/v.i && "$T/as/build/as" -o $t/o.o $t/v.s \
    || { echo "FAIL ($mode): our tools on our headers"; exit 1; }
  "${GNU}gcc" "${ours[@]}" -fsyntax-only $t/ty.c || { echo "FAIL ($mode): our header's types aren't GCC's"; exit 1; }
  "${GNU}objcopy" -O binary -j .rodata $t/g.o $t/g.bin; "${GNU}objcopy" -O binary -j .rodata $t/o.o $t/o.bin
  cmp -s $t/g.bin $t/o.bin || { echo "FAIL compiler headers ($mode): constant tables differ"; cmp -l $t/g.bin $t/o.bin | head -5; exit 1; }
  n=$(($(stat -c %s $t/g.bin) / 8))
done
echo "PASS compiler headers: $n constants identical to GCC's headers (freestanding and hosted), types GCC's"
