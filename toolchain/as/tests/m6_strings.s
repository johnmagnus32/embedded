@ String escapes, byte-compared against GNU as: octal (max 3 digits), \x hex (GAS: ALL following hex
@ digits, low byte kept), the named control escapes, and escaped quote/backslash.
	.data
	.ascii "\x7f\xff\x41B\x1234"
	.ascii "\101\0612\7"
	.asciz "\n\t\r\b\f\\\"q"
	.string "end"
