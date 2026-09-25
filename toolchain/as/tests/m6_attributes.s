@ Build attributes (.ARM.attributes "aeabi"): .cpu/.arch/.arch_extension/.fpu derive the CPU/arch/ISA/FP tags,
@ .eabi_attribute overrides or adds (by number or name, integer / string / Tag_compatibility), and the section is
@ written in GAS order (Tag_conformance, Tag_nodefaults, then ascending), zero integers omitted.
	.arch armv7-a
	.arch_extension idiv
	.arch_extension mp
	.fpu neon-vfpv4
	.eabi_attribute Tag_conformance, "2.09"
	.eabi_attribute 28, 1
	.eabi_attribute Tag_ABI_PCS_wchar_t, 4
	.eabi_attribute Tag_ABI_enum_size, 1
	.eabi_attribute 32, 1, "GNU"
	.eabi_attribute 129, "vendor"
	.eabi_attribute Tag_THUMB_ISA_use, 0
	mov	r0, #0
	bx	lr
