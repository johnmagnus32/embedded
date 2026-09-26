@ VFPv4 (UAL): every encoder, byte-compared with GNU as (-mcpu=cortex-a7); .fpu sets the FP build attributes.
	.text
	.cpu cortex-a7
	.fpu vfpv4
	vmov s0, r0
	vmov r0, s0
	vmov d0, r0, r1
	vmov r0, r1, d0
	vcvt.f32.s32 s0, s0
	vcvt.f64.s32 d0, s0
	vcvt.f64.u32 d0, s0
	vcvt.s32.f64 s0, d0
	vcvt.u32.f32 s0, s0
	vcvt.f64.f32 d0, s0
	vcvt.f32.f64 s0, d0
	vadd.f64 d0, d0, d1
	vsub.f32 s0, s0, s1
	vmul.f64 d0, d0, d1
	vdiv.f32 s0, s0, s1
	vneg.f64 d0, d0
	vabs.f32 s0, s0
	vsqrt.f64 d0, d0
	vcmp.f64 d0, d1
	vcmpe.f32 s0, s1
	vcmp.f64 d0, #0
	vmrs APSR_nzcv, fpscr
	vpush {d0-d7}
	vpop {d0-d7}
	vldm ip, {d0-d7}
	vstm ip, {d0-d7}
	vldr d0, [r0]
	vstr s0, [r0, #4]
	vmov.f64 d1, d0
	vmov.f32 s1, s0
	vaddeq.f32 s31, s30, s29
	vmla.f64 d31, d16, d15
	vnmul.f32 s3, s4, s5
	vfma.f64 d1, d2, d3
	vcvtr.s32.f64 s2, d17
	vcvt.u32.f64 s31, d31
	vcvt.f32.u32 s5, s7
	vcvtb.f32.f16 s1, s2
	vcvtt.f16.f32 s3, s4
	vmov.f32 s0, #1.0
	vmov.f64 d1, #-2.5
	vmov.f32 s2, #0.125
	vmov s2, s3, r4, r5
	vmov r4, r5, s2, s3
	vmov.f64 d16, d17
	vmsr fpexc, r0
	vmrs r1, fpscr
	vldr d16, [r1, #-1020]
	vstr s31, [sp]
	vldmia r2!, {s0-s3}
	vstmdb r3!, {d8-d15}
	vldm r4, {d0}
	vcmpe.f64 d3, #0.0
	vsub.f64 d0, d1
	vsqrteq.f32 s1, s2
