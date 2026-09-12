@ m2_secsym.s — .word <local label> in a default-flags .rodata: our as must reduce the reloc to a
@ section symbol + in-place value (GNU parity), so the .rodata jump-table bytes match GNU as.
.text
f0:
	mov r0, #0
	bx  lr
.L1:
	mov r0, #1
	bx  lr
.section .rodata
jt:
	.word f0
	.word .L1
	.word .L1+4
