#!/usr/bin/env bash
# run.sh — host unit/differential tests for the libc's pure-C code (no target needed).
#
# Compiles libc/src/stdio.c on the x86 host with its public symbols renamed to cust_*
# (via -D), so the test can pull in glibc's <stdio.h> as the ORACLE and diff the custom
# format engine against it. Same spirit as ld/test/run.sh. Add more diff tests here as
# the pure-C surface grows (libm, string, strtod, ...).
set -e
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIBC="$(cd "${HERE}/../.." && pwd)"
REPO="$(cd "${LIBC}/.." && pwd)"
CC="${CC:-cc}"
WARN="-std=gnu11 -Wall -Wextra"
INC="-I${LIBC}/include -I${REPO}/kernel/include/uapi"
# rename every public symbol stdio.c exports so none clash with glibc's at link time.
REN="-Dsnprintf=cust_snprintf -Dvsnprintf=cust_vsnprintf -Dprintf=cust_printf \
-Ddprintf=cust_dprintf -Dvprintf=cust_vprintf -Dvdprintf=cust_vdprintf \
-Dputs=cust_puts -Dputchar=cust_putchar"

tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT

echo "### format engine (snprintf) vs glibc ###"
# -fno-builtin matches the real cross build (libc-profile.sh) + avoids host builtin-decl noise.
$CC $WARN $INC $REN -fno-builtin -c "${LIBC}/src/printf.c" -o "${tmp}/stdio_host.o"
# -Wno-format: printf_diff.c DELIBERATELY exercises degenerate/edge formats (null %s, '0' flag
# with precision) — we compare runtime behavior, not gcc's static -Wformat opinion.
$CC $WARN -Wno-format -c "${HERE}/printf_diff.c" -o "${tmp}/printf_diff.o"
$CC "${tmp}/printf_diff.o" "${tmp}/stdio_host.o" -o "${tmp}/printf_diff"
"${tmp}/printf_diff"
