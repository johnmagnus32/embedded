@ ARMv7-A ARM-state instructions added for the binutils gas/arm suite (byte-compared with GNU as)
.arch armv7-a
.arch_extension mp
.syntax unified
clrex
bxj r3
bfc r0, #4, #8
bfi r1, r2, #0, #32
sbfx r3, r4, #5, #10
ubfxne r5, r6, #31, #1
swp r0, r1, [r2]
swpbeq r3, r4, [r5]
mcrr p15, 0, r0, r1, c2
mrrc p15, 1, r2, r3, c14
mcrr2 p7, 15, r4, r5, c9
mrrc2 p7, 3, r4, r5, c9
mcr2 p6, 1, r0, c1, c2, 3
mrc2 p6, 7, r0, c1, c2
cdp p5, 2, c1, c2, c3, 4
cdp2 p5, 15, c1, c2, c3
cpy r0, r1
cpyne r2, r3
qadd r0, r1, r2
qsub r0, r1, r2
qdadd r0, r1, r2
qdsubgt r0, r1, r2
smlabb r0, r1, r2, r3
smlatt r0, r1, r2, r3
smlabtgt r0, r1, r2, r3
smlawb r0, r1, r2, r3
smlawt r0, r1, r2, r3
smulbb r0, r1, r2
smultb r0, r1, r2
smulwb r0, r1, r2
smulwt r0, r1, r2
smlalbb r0, r1, r2, r3
smlaltt r0, r1, r2, r3
smulls r0, r1, r2, r3
umlalseq r0, r1, r2, r3
ldrt r0, [r1]
ldrt r0, [r1], #4
strbt r2, [r3], #-1
ldrbt r2, [r3], r4
strt r0, [r1], -r2, lsl #2
cps #19
cpsie if
cpsid aif, #17
pld [r0]
pld [r1, #-12]
pldw [r2, #4095]
pli [r3, r4]
pld [r5, -r6, lsl #3]
add r0, r1
push {r0}
pop {lr}
push {r4, lr}
mov r0, r1, rrx
