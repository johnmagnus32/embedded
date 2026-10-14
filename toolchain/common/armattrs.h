/*
 * common/armattrs.h — reading the ARM EABI build-attributes section (.ARM.attributes, ARM IHI 0045).
 *
 * The section records what an object was built FOR: CPU architecture, FP unit, and — the part that changes
 * whether a link is valid — the procedure-call conventions. Two objects that disagree about how floating-point
 * arguments are passed cannot call each other: one puts a double in a VFP register, the other in a register
 * pair. The result is not a crash, it is wrong arithmetic, so a linker must refuse it.
 *
 * Format: a leading 'A', then one or more subsections of <u32 length><vendor-name NUL>. Inside a vendor
 * subsection, sub-subsections of <u8 scope><u32 length>; scope 1 is "whole file", which is all GCC and our as
 * emit. Then tag/value pairs, each an unsigned LEB128 tag followed by either a LEB128 integer or a NUL string,
 * decided per tag. Only the "aeabi" vendor means anything to us; others are skipped, as GNU does.
 */
#ifndef OS_ARMATTRS_H
#define OS_ARMATTRS_H
#include <stddef.h>

#define SHT_ARM_ATTRIBUTES 0x70000003u   /* the section's sh_type */
#define ARM_ATTR_MAX 128                 /* tags above this are not ones any rule here looks at */

/* The tags this header names, by number (IHI 0045 table 2). */
#define Tag_CPU_raw_name          4
#define Tag_CPU_name              5
#define Tag_CPU_arch              6
#define Tag_CPU_arch_profile      7
#define Tag_ARM_ISA_use           8
#define Tag_THUMB_ISA_use         9
#define Tag_FP_arch              10
#define Tag_ABI_PCS_wchar_t      18
#define Tag_ABI_FP_number_model  23
#define Tag_ABI_enum_size        26
#define Tag_ABI_VFP_args         28
#define Tag_nodefaults           64
#define Tag_conformance          67

typedef struct {
    int  have;                           /* an "aeabi" file-scope subsection was found at all */
    int  set[ARM_ATTR_MAX];              /* this tag appeared explicitly */
    long val[ARM_ATTR_MAX];              /* its integer value (0 for a string tag) */
} ArmAttrs;

/* Parse `n` bytes of a .ARM.attributes section. Returns 0 and leaves *out zeroed if the bytes are not a
 * well-formed "aeabi" section (a caller may treat that as "declares nothing"). Never reads past data+n. */
int armattrs_parse(const unsigned char *data, size_t n, ArmAttrs *out);

/* An object takes part in float-ABI compatibility checking only if it declares an FP number model.
 * This is GNU ld's rule, established by experiment rather than from the spec: of Tag_ABI_FP_number_model,
 * Tag_ABI_FP_denormal, Tag_ABI_FP_exceptions, Tag_FP_arch and an explicit Tag_ABI_VFP_args = 0, only
 * Tag_ABI_FP_number_model makes GNU enforce the check. It is why a hand-written assembly object — which
 * declares no FP model — links happily into either a hard-float or a soft-float program, while a C object
 * compiled -mfloat-abi=soft does not link into a hard-float one even though neither carries the VFP tag. */
int armattrs_declares_fp_abi(const ArmAttrs *a);

/* The float ABI an object asks for: 0 = base PCS (integer registers), 1 = VFP registers. Absent means 0. */
int armattrs_vfp_args(const ArmAttrs *a);

#endif
