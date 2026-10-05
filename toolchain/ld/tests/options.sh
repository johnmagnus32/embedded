#!/usr/bin/env bash
# options.sh — the GNU ld command-line behaviors builds probe and rely on: -v (and ld-option's `ld -v OPT` probe),
# --defsym, --build-id (sha1/md5 recomputed here over GNU's canonical stream: the ELF header with e_phoff/e_shoff 0,
# the program headers, each section header with sh_offset 0 then its bytes, the descriptor zeroed), 0xHEX,
# --orphan-handling (GNU's messages), --no-undefined / -z defs, R_ARM_V4BX markers. Env: GNU=<cross prefix>, LD=<our ld>.
set -u
t=$(mktemp -d); trap 'rm -rf $t' EXIT
pass=0; fail=0
ok() { pass=$((pass+1)); }; no() { fail=$((fail+1)); echo "FAIL $*"; }
v=$("$LD" -v); [ "${v%% *}" = GNU ] && [ "$(echo "$v" | cut -d' ' -f2)" = ld ] && ok || no "-v: '$v' (the kernel's ld-version.sh wants 'GNU ld ... VERSION')"
"$LD" -v --no-such-option >/dev/null 2>&1 && no "-v with an unknown option succeeded (ld-option would think it works)" || ok
"$LD" -v -z noexecstack --orphan-handling=warn --build-id=sha1 --no-undefined --hash-style=sysv >/dev/null && ok || no "-v rejects an option it takes"
printf '.global _start\n_start: ldr r0, =val\nbx lr\n.data\n.word sz\n.section .stray,"a"\n.word 7\n' | "${GNU}as" -o $t/a.o
"$LD" -o $t/d.elf $t/a.o --defsym sz=0x1234 --defsym=val=sz+4 && [ "$("${GNU}nm" $t/d.elf | grep -w 'sz\|val' | tr '\n' ' ')" = "00001234 A sz 00001238 A val " ] \
  && ok || no "--defsym: $("${GNU}nm" $t/d.elf 2>&1 | grep -w 'sz\|val' | tr '\n' ' ')"
for style in sha1 md5; do
  "$LD" --build-id=$style -o $t/b.elf $t/a.o --defsym sz=1 --defsym val=2 || { no "--build-id=$style: link"; continue; }
  want=$(python3 - $t/b.elf $style <<'P'
import hashlib, struct, sys
d = bytearray(open(sys.argv[1], 'rb').read()); h = hashlib.new(sys.argv[2])
eh = bytearray(d[:52]); struct.pack_into('<II', eh, 28, 0, 0); h.update(eh)
phoff, shoff = struct.unpack_from('<II', d, 28); phnum, = struct.unpack_from('<H', d, 44); shnum, = struct.unpack_from('<H', d, 48)
h.update(d[phoff:phoff + 32 * phnum])
for i in range(shnum):
    typ, = struct.unpack_from('<I', d, shoff + 40 * i + 4); off, size = struct.unpack_from('<II', d, shoff + 40 * i + 16)
    if typ == 7 and d[off + 12:off + 16] == b'GNU\0': d[off + 16:off + size] = bytes(size - 16)
for i in range(shnum):
    sh = bytearray(d[shoff + 40 * i:shoff + 40 * i + 40]); typ, = struct.unpack_from('<I', sh, 4); off, size = struct.unpack_from('<II', sh, 16)
    struct.pack_into('<I', sh, 16, 0); h.update(sh)
    if typ != 8: h.update(d[off:off + size])
print(h.hexdigest())
P
)
  got=$("${GNU}readelf" -n $t/b.elf | sed -n 's/.*Build ID: //p')
  [ -n "$got" ] && [ "$got" = "$want" ] && ok || no "--build-id=$style: note '$got', GNU's hash '$want'"
done
"$LD" --build-id=0x0123456789abcdef -o $t/x.elf $t/a.o --defsym sz=1 --defsym val=2 && [ "$("${GNU}readelf" -n $t/x.elf | sed -n 's/.*Build ID: //p')" = 0123456789abcdef ] && ok || no "--build-id=0xHEX"
"$LD" --no-warn-rwx-segments --orphan-handling=warn -T /dev/stdin -o $t/o.elf $t/a.o --defsym sz=1 --defsym val=2 2>$t/w <<<'SECTIONS { .text : { *(.text) } .data : { *(.data) } }' \
  && grep -qF "ld: warning: orphan section \`.stray' from \`$t/a.o' being placed in section \`.stray'" $t/w && ok || no "--orphan-handling=warn: '$(cat $t/w)'"
"$LD" --no-warn-rwx-segments --orphan-handling=error -T /dev/stdin -o $t/o.elf $t/a.o --defsym sz=1 --defsym val=2 2>$t/w <<<'SECTIONS { .text : { *(.text) } .data : { *(.data) } }' \
  && no "--orphan-handling=error linked" || { grep -qF "ld: error: unplaced orphan section \`.stray' from \`$t/a.o'" $t/w && ok || no "--orphan-handling=error: '$(cat $t/w)'"; }
"$LD" --no-warn-rwx-segments --orphan-handling=discard -T /dev/stdin -o $t/o.elf $t/a.o --defsym sz=1 --defsym val=2 <<<'SECTIONS { .text : { *(.text) } .data : { *(.data) } }' \
  && ! "${GNU}readelf" -S $t/o.elf | grep -q '\.stray' && ok || no "--orphan-handling=discard kept .stray"
printf '.global f\nf: b g\n' | "${GNU}as" -o $t/s.o
"$LD" -shared -o $t/s.so $t/s.o && ok || no "-shared with an import (no provider) refused"
"$LD" -shared --no-undefined -o $t/s.so $t/s.o 2>$t/e && no "--no-undefined accepted an undefined g" || { grep -q "undefined symbol 'g'" $t/e && ok || no "--no-undefined: '$(cat $t/e)'"; }
"$LD" -shared -z defs -o $t/s.so $t/s.o 2>/dev/null && no "-z defs accepted an undefined g" || ok
printf '.global _start\n_start: bx lr\n' | "${GNU}as" -o $t/v.o   # GNU as marks the BX: R_ARM_V4BX against no symbol
"$LD" -o $t/v.elf $t/v.o && ok || no "R_ARM_V4BX: not linked"
printf '.global _start\n_start: ldr r0, =__start_myset\nldr r1, =__stop_myset\nbx lr\n.section myset,"a"\n.word 1, 2, 3\n.weak __start_absent\n.word __start_absent\n' | "${GNU}as" -o $t/ss.o
"$LD" -o $t/ss.elf $t/ss.o && "${GNU}ld" -o $t/ss.g $t/ss.o 2>/dev/null \
  && [ "$("${GNU}readelf" -sW $t/ss.elf | grep '_myset' | awk '{print $2, $5, $6, $8}' | sort)" = "$("${GNU}readelf" -sW $t/ss.g | grep '_myset' | awk '{print $2, $5, $6, $8}' | sort)" ] \
  && ok || no "__start_SEC/__stop_SEC differ from GNU ld's: $("${GNU}readelf" -sW $t/ss.elf 2>&1 | grep _myset | tr '\n' ' ')"
echo "ld options: $pass passed, $fail failed"
[ $fail = 0 ]
