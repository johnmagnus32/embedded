#!/usr/bin/env bash
# veneer.sh — long-branch veneers: code 64MB apart (beyond b/bl's +-32MB) calls back and forth — bl, b, a conditional
# b — through veneers, plain (ldr pc) and --pic-veneer (ldr ip / add pc), and the program runs (QEMU, semihosting
# exit code). Also: GNU's veneer names, and an out-of-reach branch with no way through is refused (no silent wrap).
# Env: GNU=<cross prefix>, LD=<our ld>, QEMU=<qemu-system-arm>.
set -u
t=$(mktemp -d); trap 'rm -rf $t' EXIT
pass=0; fail=0
ok() { pass=$((pass+1)); }; no() { fail=$((fail+1)); echo "FAIL $*"; }
cat > $t/a.s <<'S'
	.text
	.global _start
_start:	ldr sp, =0x40800000
	mov r4, #0
	bl far_add          @ r4 += 10, from 64MB up
	bl far_tail         @ far_tail branches back (b near_add): r4 += 100
	cmp r4, #110
	beq far_cond        @ a conditional branch out of reach: r4 += 1000
	b done
done:	mov r0, #0x20       @ semihosting SYS_EXIT_EXTENDED: {0x20026 = ADP_Stopped_ApplicationExit, r4}
	ldr r1, =block
	str r4, [r1, #4]
	ldr r2, =0x20026
	str r2, [r1]
	svc 0x123456
	hlt #0xf000
	.global near_add
near_add: add r4, r4, #100
	bx lr
	.global near_back
near_back: b done
	.data
block:	.word 0, 0
	.section .far,"ax"
	.global far_add, far_tail, far_cond
far_add: add r4, r4, #10
	bx lr
far_tail: b near_add
far_cond: add r4, r4, #1000
	b near_back
S
"${GNU}as" -o $t/a.o $t/a.s
printf 'SECTIONS { . = 0x40000000; .text : { *(.text) } .data : { *(.data) } . = 0x44000000; .far : { *(.far) } }\n' > $t/far.ld
for v in "" --pic-veneer; do
  "$LD" --no-warn-rwx-segments $v -T $t/far.ld -o $t/p.elf $t/a.o 2>$t/e || { no "link $v: $(cat $t/e)"; continue; }
  "${GNU}objcopy" -O binary $t/p.elf $t/p.bin 2>/dev/null
  timeout 10 "$QEMU" -M virt -cpu cortex-a7 -m 1024M -nographic -semihosting -net none -kernel $t/p.elf >/dev/null 2>&1; rc=$?
  [ $rc = $((1110 & 255)) ] && ok || no "veneers $v: exit $rc, want $((1110 & 255)) (r4 = 1110)"
  n=$("${GNU}nm" $t/p.elf | grep -c '_veneer$'); [ "$n" -ge 4 ] && ok || no "veneers $v: $n __*_veneer symbols"
  "${GNU}nm" $t/p.elf | grep -q ' t __far_add_veneer$' && ok || no "veneer names: no __far_add_veneer ($("${GNU}nm" $t/p.elf | grep veneer | tr '\n' ' '))"
done
size=$("${GNU}objdump" -d $t/p.elf | grep -A3 '<__far_add_veneer>:' | grep -c 'e59fc000\|e08ff00c')
[ "$size" = 2 ] && ok || no "--pic-veneer: not GNU's ldr ip, [pc] / add pc, pc, ip stub"
printf 'SECTIONS { . = 0x40000000; .text : { *(.text) } .data : { *(.data) } . = 0x44000000; .far : { *(.far) } }\n' > $t/far.ld
printf '.text\n.global _start\n_start: .reloc ., R_ARM_JUMP24, far\n.word 0xeafffffe\n.section .far,"ax"\n.global far\n.thumb\n.thumb_func\nfar: bx lr\n' | "${GNU}as" -o $t/th.o 2>/dev/null
"$LD" --no-warn-rwx-segments -T $t/far.ld -o $t/th.elf $t/th.o 2>$t/e && no "a branch out of reach to Thumb code linked" || { grep -q 'Thumb' $t/e && ok || no "Thumb out of reach: '$(cat $t/e)'"; }
echo "ld veneers: $pass passed, $fail failed"
[ $fail = 0 ]
