@ stub.s — the tiny "loader" half of the two-part boot used to give a PIE a nonzero load bias on
@ qemu -M virt. qemu loads THIS as the -kernel (a normal ET_EXEC at 0x40000000) and starts it; the PIE
@ blob is placed separately at 0x40200000 via `-device loader,file=blob.bin,addr=0x40200000`. The stub
@ just sets a stack and jumps to 0x40200000, so the PIE runs at an address different from its link base.
.text
.global _start
_start:
	movw sp, #0x0000
	movt sp, #0x4090          @ sp = 0x40900000
	movw r0, #0x0000
	movt r0, #0x4020          @ r0 = 0x40200000 (where the PIE blob was loaded)
	bx   r0                   @ enter the PIE at its runtime address
