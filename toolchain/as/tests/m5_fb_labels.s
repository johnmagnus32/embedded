@ GAS fb labels: forward/backward local labels in branches (incl. cross-section), adr, ldr, data words, differences, .size
.arch armv7-a
.syntax unified
f: b 1f
nop
1: adr r0, 1f
ldr r1, 1f
b 2f
.word 1b, 1f, 1f - 1b
1: .word 7
.pushsection .text.fixup, "ax"
2: b 3f
.popsection
3: bl 2b
.size f, 1b - f
.data
.word 1b, 3b
.long 9f - .
.text
9: nop
