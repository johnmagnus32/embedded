@ Pre-UAL (divided-syntax) mnemonics, ldm/stm stack aliases, ldc/stc, legacy forms
.arch armv7-a
ldmfd sp!, {r0, r1}
ldmed r0, {r2}
ldmfa r1!, {r3, r4}
ldmea r2, {r5}
stmfd sp!, {r0, lr}
stmed r0, {r2}
stmfa r1!, {r3}
stmea r2, {r5, r6}
ldmeqfd sp!, {r4, pc}
stmneia r0!, {r1}
ldreqb r0, [r1]
strneh r2, [r3, #2]
ldrgtsb r4, [r5]
swpgeb r0, r1, [r2]
umlaleqs r0, r1, r2, r3
moveqs r0, r1
addnes r2, r3, #1
teqp r0, #0
cmpeqp r1, r2
ldc p5, c3, [r0, #16]
ldcl p5, c3, [r0, #-8]!
stc p6, cr1, [r2], #4
stcl p6, c1, [r2], {7}
ldc2 p7, c2, [r3]
stc2l p7, c2, [r3, #1020]
ldcpl p1, c0, [r4]
bkpt #0x1234
ldrexd r0, [r2]
strexd r3, r0, [r2]
dmb sh
dsb un
dmb unst
bfi r0, #0, #3, #4
mrrc p13, 0, r0, r1, c5
mcrr 13, 0, r0, r1, cr5
