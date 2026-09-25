@ Data relocation operators `sym(OP)` — exactly GNU as 2.42s table (PLT on data is R_ARM_ABS32; TLS IE/LE are
@ GOTTPOFF/TPOFF), with addends and a local target. Compared: .data bytes (relocs are checked by kernel-parity/gas-suite).
.data
.word sym(GOT)
.word sym(GOT_PREL)
.word sym(GOTOFF)
.word sym(TARGET1)
.word sym(TARGET2)
.word sym(SBREL)
.word sym(PLT)
.word sym(TLSGD)
.word sym(TLSLDM)
.word sym(TLSLDO)
.word sym(GOTTPOFF)
.word sym(TPOFF)
.word sym(TLSDESC)
.word sym(TLSCALL)
.word sym(GOT) + 4
.word foo(TARGET2) + 0x1234
.word loc(GOTOFF)
loc: .word 0
