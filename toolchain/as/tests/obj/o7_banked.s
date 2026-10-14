@ ARMv7 MRS/MSR (banked): another mode's register, read or written without switching mode. All 33 names in
@ both directions, plus the conditional forms U-Boot's nonsec_virt.S:83-84 actually uses. The whole object is
@ compared with GNU as, so every SYSm/R field here is checked rather than asserted.
	.arch armv7-a
	.arch_extension sec
	.arch_extension virt
	.text
	mrs r0, r8_usr
	mrs r1, r9_usr
	mrs r2, r10_usr
	mrs r3, r11_usr
	mrs r4, r12_usr
	mrs r5, sp_usr
	mrs r6, lr_usr
	mrs r7, r8_fiq
	mrs r8, r9_fiq
	mrs r9, r10_fiq
	mrs r10, r11_fiq
	mrs r11, r12_fiq
	mrs r12, sp_fiq
	mrs lr, lr_fiq
	mrs r0, lr_irq
	mrs r0, sp_irq
	mrs r0, lr_svc
	mrs r0, sp_svc
	mrs r0, lr_abt
	mrs r0, sp_abt
	mrs r0, lr_und
	mrs r0, sp_und
	mrs r0, lr_mon
	mrs r0, sp_mon
	mrs r0, elr_hyp
	mrs r0, sp_hyp
	mrs r0, spsr_fiq
	mrs r0, spsr_irq
	mrs r0, spsr_svc
	mrs r0, spsr_abt
	mrs r0, spsr_und
	mrs r0, spsr_mon
	mrs r0, spsr_hyp
	msr r8_usr, r0
	msr r9_usr, r1
	msr r10_usr, r2
	msr r11_usr, r3
	msr r12_usr, r4
	msr sp_usr, r5
	msr lr_usr, r6
	msr r8_fiq, r7
	msr r9_fiq, r8
	msr r10_fiq, r9
	msr r11_fiq, r10
	msr r12_fiq, r11
	msr sp_fiq, r12
	msr lr_fiq, lr
	msr lr_irq, r0
	msr sp_irq, r1
	msr lr_svc, r0
	msr sp_svc, r0
	msr lr_abt, r0
	msr sp_abt, r0
	msr lr_und, r0
	msr sp_und, r0
	msr lr_mon, r0
	msr sp_mon, r0
	msr elr_hyp, r0
	msr sp_hyp, r3
	msr spsr_fiq, r0
	msr spsr_irq, r0
	msr spsr_svc, r1
	msr spsr_abt, r0
	msr spsr_und, r0
	msr spsr_mon, r0
	msr spsr_hyp, r0
@ the conditional forms, and the non-banked ones that must keep working beside them
	mrseq r3, sp_svc
	msreq sp_hyp, r3
	mrsne r0, spsr_irq
	msrmi sp_irq, r1
	mrs r6, spsr
	mrs r0, cpsr
	msr spsr_cxfs, ip
	msr spsr_c, r13
	msr cpsr_fc, r1
	msr cpsr, r0
