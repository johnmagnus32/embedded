#!/usr/bin/env bash
# predef-gate.sh — our cpp -dM == GNU gcc -nostdinc -dM (sorted, exact) over a matrix of the options that change the
# predefined macros (each group alone, overrides within a group, cross-group combinations, -undef, -D/-U order).
# GNU=<cross prefix> CPP=<our cpp> predef-gate.sh
G=${GNU:?}; C=${CPP:?}; T=$(mktemp -d); trap "rm -rf $T" EXIT
ok=0; bad=0
while IFS= read -r o; do
  ${G}gcc -nostdinc $o -dM -E -x c /dev/null 2>/dev/null | sed 's/ *$//' | sort > $T/pg.g
  $C $o -dM /dev/null 2>$T/pg.err | sed 's/ *$//' | sort > $T/pg.o
  if cmp -s $T/pg.g $T/pg.o; then ok=$((ok+1)); else bad=$((bad+1)); echo "DIFF [$o]: $(cat $T/pg.err) $(diff $T/pg.g $T/pg.o | grep '^[<>]' | head -4 | tr '\n' ' ')"; fi
done <<'LIST'

-O2
-Os
-Og
-Ofast
-O2 -O0
-fPIC
-fpic
-fPIE
-fPIC -fpie
-fPIE -fno-pie
-fno-pie -fPIC
-ffreestanding
-std=c99
-std=gnu99
-std=c89
-ansi
-std=gnu89
-std=iso9899:199409
-std=c11
-std=gnu11
-std=c17
-std=c2x
-std=gnu2x
-mfloat-abi=soft
-mfloat-abi=softfp
-mfloat-abi=soft -ffast-math
-ffast-math
-ffast-math -fno-fast-math
-fshort-wchar
-fshort-wchar -fno-short-wchar
-march=armv7-a
-mfpu=vfp
-msoft-float
-mfpu=vfp -msoft-float
-mfpu=vfp -mfloat-abi=softfp
-mno-unaligned-access
-mthumb
-mthumb -marm
-fstack-protector
-fstack-protector-strong
-fstack-protector-all
-fstack-protector-strong -fno-stack-protector
-march=armv7-a -mfpu=vfp -msoft-float -fshort-wchar -O2 -std=gnu11 -fstack-protector-strong -marm
-march=armv7-a -mfpu=vfp -msoft-float -fshort-wchar -O2 -std=gnu11 -fpic -ffreestanding -fno-stack-protector
-pthread
-fsigned-char
-undef
-undef -std=c99
-undef -ffreestanding -pthread
-std=c99 -O2 -fPIC -ffreestanding
-std=c99 -ffreestanding -Os -mfloat-abi=soft
-std=gnu11 -O2 -pthread -fPIC
-DFOO -DBAR=2 -UFOO
-D__GNUC__=4 -U__STDC_VERSION__
LIST

${G}gcc -nostdinc -x assembler-with-cpp -dM -E /dev/null | sed 's/ *$//' | sort > $T/pg.g
$C -x assembler-with-cpp -dM /dev/null | sed 's/ *$//' | sort > $T/pg.o
if cmp -s $T/pg.g $T/pg.o; then ok=$((ok+1)); else bad=$((bad+1)); echo "DIFF [-x assembler-with-cpp]"; fi
echo "predefine gate: $ok identical to GCC, $bad differ"
[ $bad = 0 ]
