@ Floating-point data (checked vs GNU as): .float/.single, .double, .float16 (ieee + alternative), .bfloat16 —
@ RNE rounding, subnormals, overflow to Inf, and GAS's NaN encoding (all mantissa bits set).
.data
	.float16 12.0
	.float16 0.123
	.float16 0.004
	.float16 65504
	.float16 5.9605e-8
	.float16 6.0976e-5
	.float16 6.1035e-5
	.float16 1
	.float16 1.001
	.float16 NaN
	.float16 +Inf
	.float16 -Inf
	.float16 +0
	.float16 -0
	.float16 -1
	.float16 -0.98765
	.float16 -65504
	.float16 3.0, 12.0, 543.123

.float16_format alternative
	.float16 0.542
	.float16 131008.0
	.float16 -131008.0
	.float16 12323.1234
	.bfloat16 12.0
	.bfloat16 0.123
	.bfloat16 +0.0
	.bfloat16 123.4
	.bfloat16 -0.0
	.bfloat16 -123.4
	.bfloat16 NaN
	.bfloat16 Inf
	.bfloat16 -Inf
	.bfloat16 3.390e+38
	.bfloat16 -3.390e+38
	.bfloat16 1.175e-38
	.bfloat16 -1.175e-38
	.bfloat16 9.194e-41
	.bfloat16 -9.194e-41
	.bfloat16 1.167e-38
	.bfloat16 -1.167e-38
	.bfloat16 1.0, -1, 2.0, -2, 0
.float 1.5, -0.1, 3.4e38, 1e-45, nan, -inf
.double 0.1, -2.5e-300, 0d1.25
.single 7
.float -nan
.double nan, -nan
