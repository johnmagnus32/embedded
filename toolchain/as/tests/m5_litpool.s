@ Literal pools (ldr =), .ltorg, ldr/adr expression operands, mapping symbols around pools
.arch armv7-a
.syntax unified
f: ldr r0, =0xff
ldr r1, =0xffffff00
ldr r2, =0x12345678
ldr r3, =sym
ldr r4, =0x12345678
ldr r5, =sym + 8
b 1f
.ltorg
1: ldr r6, =0xabcdef01
mov r0, r0
ldr r7, =0x11112222
g: ldr r0, l3
ldr r1, l3 + 4
ldr r2, .
ldr r3, . + 16
l3: .word 1, 2
