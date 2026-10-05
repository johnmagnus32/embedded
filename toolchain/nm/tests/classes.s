@ Every symbol class GNU nm distinguishes: section kinds by flags and by (COFF) name, bindings, weak/undefined
@ objects, absolutes, commons, and the ARM mapping symbols it hides ($a.foo, $d, $x) beside one it shows ($A).
.section .init.data,"aw"
v_initdata: .word 1
.section .init.text,"ax"
v_inittext: .word 1
.section .rodata.x,"a"
v_rodata: .word 1
.section .sdata,"aw"
v_sdata: .word 1
.section .sbss,"aw",%nobits
v_sbss: .space 4
.section code,"a"
v_code: .word 1
.section vars,"a"
v_vars: .word 1
.section .rdata,"aw"
v_rdata: .word 1
.section .fini.x,"aw"
v_fini: .word 1
.section .bss.y,"aw",%nobits
v_bss: .space 4
.section .tbss,"awT",%nobits
v_tbss: .space 4
.section .tdata,"awT"
v_tdata: .word 1
.section .comment.x,""
v_comment: .word 1
.section .debug_x,""
v_debug: .word 1
.section .wnote,"w"
v_wnote: .word 1
.section .edata.x,"a"
v_edata: .word 1
.section .idata$x,"a"
v_idata: .word 1
.section .pdata.x,"a"
v_pdata: .word 1
.section .drectve,"a"
v_drectve: .word 1
.section .nbss,"",%nobits
v_nbss: .space 4
.text
.global g_text
g_text: bx lr
.weak w_def
w_def: bx lr
.weak w_undef
.word w_undef
.word u_undef
.type wo_undef,%object
.weak wo_undef
.word wo_undef
.type wo_def,%object
.weak wo_def
wo_def: .word 0
.set abs_l, 0x1234
.global abs_g
.set abs_g, 0x5678
.comm common_x, 16, 4
.local lcomm
.comm lcomm, 8
$a.foo: nop
$d: nop
$x: nop
$A: nop
