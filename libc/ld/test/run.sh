#!/usr/bin/env bash
# run.sh — host unit tests for the linker's pure/parsing logic (no target needed).
#   L2 (test_reloc):  reloc arithmetic + SysV-hash symbol lookup, hand fixtures.
#   S1 (test_parse):  parse a REAL build/libc.so's .dynamic + resolve its symbols.
# Both run on the x86 host; the S1 test lays out the .so's LOAD segments in the
# low 4GB (MAP_32BIT) so the identical 32-bit-pointer parser runs untruncated.
set -e
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="${HERE}/../src"
RDIR="$(cd "${HERE}/../.." && pwd)"
CC="${CC:-cc}"
CFLAGS="-std=c11 -Wall -Wextra -fsanitize=address -I${SRC}"

echo "### L2: reloc + symbol logic ###"
b1="$(mktemp -u /tmp/ld-l2.XXXXXX)"
$CC $CFLAGS "${HERE}/test_reloc.c" -o "$b1"; "$b1"; rm -f "$b1"

echo; echo "### S1: parse real libc.so ###"
LIBC="${RDIR}/build/libc.so"
if [ ! -f "$LIBC" ]; then
  echo "  (building dynamic libc.so first)"
  ( cd "$RDIR" && make LINK=dynamic BOARD=virt >/dev/null 2>&1 )
fi
b2="$(mktemp -u /tmp/ld-s1.XXXXXX)"
$CC $CFLAGS "${HERE}/test_parse.c" -o "$b2"; "$b2" "$LIBC"; rm -f "$b2"
