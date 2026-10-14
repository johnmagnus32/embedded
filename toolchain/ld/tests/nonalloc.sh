#!/bin/bash
# nonalloc.sh — NON-ALLOCATED output sections (.comment, .ARM.attributes, every .debug_*) must survive the
# link, as GNU ld's do. They used to be dropped wholesale: placeable() required SHF_ALLOC, so they reached
# neither pattern matching nor orphan placement, and a `gcc -c -g` object's eleven of them came out as two.
# That silently cost the producer identity, the EABI build attributes a consumer reads, and any possibility
# of debug information surviving a link at all.
#
# What is checked here:
#   1. the output's section NAME LIST equals GNU ld's, for the same inputs and the default script
#   2. every non-allocated section has address 0 (they take file space, not memory)
#   3. none of them lands in a PT_LOAD segment
#   4. their relocations ARE applied — .debug_aranges must name the real .text address, not zero
#   5. `objcopy -O binary` output is unchanged by their presence (the loaded image is allocated content only)
#   6. _end / __end__ / _bss_end__ are attributed to an allocated section, type 'D' as GNU reports it --
#      a non-alloc section sits at vaddr 0, so a "nearest section below this symbol" search picks it unless
#      the search skips non-alloc
#   7. GNU readelf -A parses our .ARM.attributes without complaint (concatenating two inputs' attribute
#      sections does not produce a valid one; we keep the first until a real tag merge exists)
# Env: GNU=<cross prefix>, LD=<our ld>.
set -u
: "${GNU:?}" "${LD:?}"
t=$(mktemp -d); trap 'rm -rf $t' EXIT
pass=0; fail=0
ok() { pass=$((pass+1)); echo "PASS $*"; }
no() { fail=$((fail+1)); echo "FAIL $*"; }

printf 'int g = 1;\nstatic int h(int x) { return x + g; }\nint main(void) { return h(2); }\n' > "$t/a.c"
"${GNU}gcc" -c -g -O0 -marm -mcpu=cortex-a7 "$t/a.c" -o "$t/a.o"
printf '.text\n.globl _start\n_start: bl main\n b .\n' > "$t/s.s"
"${GNU}as" -mcpu=cortex-a7 -o "$t/s.o" "$t/s.s"

names() { "${GNU}readelf" -SW "$1" | sed -n 's/^ *\[ *[0-9][0-9]*\] \([^ ]*\).*/\1/p' | tr '\n' ' '; }

"$LD"     -Ttext 0x40000000 -o "$t/o.elf" "$t/s.o" "$t/a.o" 2>/dev/null || { echo "FAIL: our ld failed"; exit 1; }
"${GNU}ld" -Ttext 0x40000000 -o "$t/g.elf" "$t/s.o" "$t/a.o" 2>/dev/null || { echo "SKIP: GNU ld failed"; exit 0; }

# 1. the same sections, in the same order
if [ "$(names "$t/o.elf")" = "$(names "$t/g.elf")" ]; then ok "section name list identical to GNU ld"
else no "section list differs:"; echo "    gnu : $(names "$t/g.elf")"; echo "    ours: $(names "$t/o.elf")"; fi

# 2. non-allocated sections carry no address
bad=$("${GNU}readelf" -SW "$t/o.elf" | sed -n 's/^ *\[ *[0-9][0-9]*\] //p' \
      | awk '$1 ~ /^\.(comment|ARM\.attributes|debug)/ && $3 != "00000000" {printf "%s@%s ", $1, $3}')
[ -z "$bad" ] && ok "non-allocated sections are at address 0" || no "addressed non-alloc sections: $bad"

# 3. and in no segment
secs_in_load=$("${GNU}readelf" -lW "$t/o.elf" | awk '/^   [0-9][0-9]*     /{print}' | grep -cE '\.(comment|ARM\.attributes|debug)')
[ "$secs_in_load" = 0 ] && ok "no non-allocated section is mapped into a segment" || no "$secs_in_load non-alloc sections appear in a segment"

# 4. their relocations are applied: .debug_aranges holds the text address, not zero
"${GNU}objcopy" --dump-section ".debug_aranges=$t/ar" "$t/o.elf" /dev/null 2>/dev/null
txt=$("${GNU}readelf" -SW "$t/o.elf" | sed -n 's/^ *\[ *[0-9][0-9]*\] \.text *[A-Z]* *\([0-9a-f]*\).*/\1/p')
if [ -s "$t/ar" ]; then
	# the CU's address field is the 4 bytes at offset 16 of the aranges header (little-endian)
	got=$(od -An -tx4 -j16 -N4 "$t/ar" | tr -d ' \n')
	if [ "$got" = "00000000" ]; then no ".debug_aranges address is zero: relocations not applied to non-alloc sections"
	else ok ".debug_aranges relocated (0x$got, .text at 0x$txt)"; fi
else no ".debug_aranges missing from the output"; fi

# 5. the loaded image is unaffected by their presence. Compared against OUR OWN link of the same objects with
# the non-allocated sections stripped out first -- not against GNU's raw binary, whose size differs for an
# unrelated reason (we map the ELF headers into the first segment, so our .text starts above the load address).
"${GNU}objcopy" -R .comment -R .ARM.attributes --remove-section='.debug*' "$t/a.o" "$t/a-bare.o"
"${GNU}objcopy" -R .comment -R .ARM.attributes --remove-section='.debug*' "$t/s.o" "$t/s-bare.o"
"$LD" -Ttext 0x40000000 -o "$t/bare.elf" "$t/s-bare.o" "$t/a-bare.o" 2>/dev/null
"${GNU}objcopy" -O binary "$t/o.elf" "$t/o.bin"
"${GNU}objcopy" -O binary "$t/bare.elf" "$t/bare.bin"
if cmp -s "$t/o.bin" "$t/bare.bin"; then ok "objcopy -O binary identical with and without the non-alloc sections ($(stat -c%s "$t/o.bin") bytes)"
else no "the loaded image changed: $(stat -c%s "$t/o.bin") bytes with, $(stat -c%s "$t/bare.bin") without"; fi

# 6. script symbols belong to an allocated section
badt=$("${GNU}nm" "$t/o.elf" | awk '$3 ~ /^(_end|__end__|_bss_end__|__bss_end__)$/ && $2 != "D" {printf "%s=%s ", $3, $2}')
[ -z "$badt" ] && ok "_end/__end__/_bss_end__ attributed to an allocated section (type D, as GNU)" \
               || no "script symbols in a non-alloc section: $badt"

# 7. the attribute section we emit is parseable
if "${GNU}readelf" -A "$t/o.elf" > "$t/attr" 2>"$t/attrerr" && ! grep -qi 'error' "$t/attrerr"; then
	ok "GNU readelf -A parses our .ARM.attributes"
else no "our .ARM.attributes is malformed: $(head -2 "$t/attrerr")"; fi

echo "ld non-alloc sections: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
