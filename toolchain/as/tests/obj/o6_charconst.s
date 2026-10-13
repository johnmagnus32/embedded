@ o6_charconst.s — a character constant is ONE token and ONE macro argument, blanks and commas included.
@ GAS also accepts a bare 'c with no closing quote. The kernel's decompressor hits all of this under
@ CONFIG_DEBUG_UNCOMPRESS (arch/arm/boot/compressed/head.S: `kputc #' '`, nested inside dbgadtb), and
@ getting it wrong split one operand into two: "macro 'kputc': too many arguments".
	.macro	kputc,val
	mov	r0, \val
	.endm
	.macro	kphex,val,len
	mov	r1, \val
	add	r1, r1, #\len
	.endm
	.macro	dbgadtb, begin, size
	kputc	#'D'
	kputc	#':'
	kphex	\begin, 8
	kputc	#' '                   @ a BLANK inside the constant
	kputc	#','                   @ a COMMA inside the constant
	kputc	#'('
	kputc	#')'
	kputc	#'\n'                  @ an escape, inside a macro body (where \n could look like a parameter)
	kphex	\size, 8
	.endm
	.text
	.global charconst
charconst:
	dbgadtb r4, r5
	mov	r2, #'A'
	mov	r2, #'a                 @ bare, no closing quote (GAS)
	mov	r2, #'\t'
	mov	r3, #' ' + 1
	add	r3, r3, #',' * 2
	.byte	'x', ',', ' ', '\n'
	.word	'Z'
