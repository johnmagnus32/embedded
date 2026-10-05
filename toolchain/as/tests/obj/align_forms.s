@ alignment forms (GAS s_align): .align 0 is 2 on ARM; a FILL byte or a .balignw/.balignl/.p2alignw/.p2alignl pattern
@ (a $d frag even in code); padding over MAX skipped; .balign/.p2align with no operand: nothing; .bss ignores the fill
.text
.byte 1
.align 0
.byte 2,0,0,0
nop
.balignl 16,0xdeadbeef
.word 8
.align 4,,4
nop
.p2align 4,,12
nop
.balign 8,0x77
.data
.byte 3
.balign 8,0x55,3
.byte 4
.balign 8,0x66,7
.byte 5,5
.p2alignw 3,0x1234
.byte 6,6,6,6
.balignl 16,0xdeadbeef
.word 7
.balign
.p2align
.section .bss
.byte 0
.balign 4,0
.byte 0
