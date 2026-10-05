#!/usr/bin/env python3
"""gen-has.py GNU-PREFIX [SRC-DIR...] > has.h — what GCC answers to __has_attribute / __has_c_attribute /
__has_builtin, for our cpp.

The answer is a property of the compiler, so it's measured, not guessed: every name below (GCC's documented
attributes, the builtins, and whatever the given source trees ask about) goes through `gcc -E`, and the table records
GCC's value — the zeros too. cpp dies on a name the table doesn't hold ("regenerate with it"), so a new question is
never silently answered 0. Generated, never hand-edited."""
import os, re, subprocess, sys

gnu = sys.argv[1]
ATTRS = """access alias aligned alloc_align alloc_size always_inline artificial assume assume_aligned btf_decl_tag
btf_type_tag cleanup cmse_nonsecure_call cmse_nonsecure_entry cold common const constructor copy counted_by
deprecated designated_init destructor diagnose_as_builtin disable_sanitizer_instrumentation disable_tail_calls error
expected_throw externally_visible fallthrough fd_arg fd_arg_read fd_arg_write flag_enum flatten format format_arg
gnu_inline hardbool hot ifunc interrupt isr leaf long_call malloc may_alias maybe_unused mode model naked
no_address_safety_analysis no_caller_saved_registers no_icf no_instrument_function no_profile_instrument_function
no_reorder no_sanitize no_sanitize_address no_sanitize_coverage no_sanitize_thread no_sanitize_undefined
no_split_stack no_stack_limit no_stack_protector noclone nocommon nodiscard noinit noinline noipa nonnull nonstring
noplt noreturn nothrow null_terminated_string_arg optimize overloadable packed pass_dynamic_object_size
pass_object_size patchable_function_entry pcs persistent preserve_most pure retain returns_nonnull returns_twice
scalar_storage_order section sentinel short_call simd stack_protect strict_flex_array symver target target_clones
tls_model transparent_union unavailable uninitialized unused used vector_size visibility warn_if_not_aligned
warn_unused warn_unused_result warning weak weakref zero_call_used_regs""".split()
C_ATTRS = "deprecated fallthrough maybe_unused nodiscard noreturn reproducible unsequenced _Noreturn".split()
BUILTINS = """__builtin_add_overflow __builtin_add_overflow_p __builtin_alloca __builtin_alloca_uninitialized
__builtin_alloca_with_align __builtin_assume __builtin_assume_aligned __builtin_bitreverse32 __builtin_bswap16
__builtin_bswap32 __builtin_bswap64 __builtin_btf_type_id __builtin_call_with_static_chain __builtin_choose_expr
__builtin_classify_type __builtin_clrsb __builtin_clz __builtin_clzl __builtin_clzll __builtin_complex
__builtin_constant_p __builtin_counted_by_ref __builtin_ctz __builtin_ctzl __builtin_ctzll __builtin_dynamic_object_size
__builtin_expect __builtin_expect_with_probability __builtin_extract_return_addr __builtin_ffs __builtin_ffsl
__builtin_ffsll __builtin_frame_address __builtin_has_attribute __builtin_huge_val __builtin_inf __builtin_is_constant_evaluated
__builtin_isdigit __builtin_isinf __builtin_isnan __builtin_launder __builtin_memcmp __builtin_memcpy
__builtin_memmove __builtin_memset __builtin_mul_overflow __builtin_mul_overflow_p __builtin_nan __builtin_object_size
__builtin_offsetof __builtin_parity __builtin_popcount __builtin_popcountl __builtin_popcountll __builtin_prefetch
__builtin_preserve_access_index __builtin_preserve_enum_value __builtin_preserve_field_info __builtin_preserve_type_info
__builtin_return_address __builtin_shufflevector __builtin_signbit __builtin_speculation_safe_value __builtin_strcmp
__builtin_strlen __builtin_sub_overflow __builtin_sub_overflow_p __builtin_thread_pointer __builtin_trap
__builtin_types_compatible_p __builtin_unreachable __builtin_va_arg __builtin_va_copy __builtin_va_end
__builtin_va_start __builtin_clear_padding __builtin_convertvector __builtin_elementwise_abs""".split()

def scan(dirs):   # names the sources ask about (a macro parameter like `x` gets probed too: harmless)
	found = {"attr": set(), "c_attr": set(), "builtin": set()}
	pat = re.compile(r"__has_(c_attribute|attribute|builtin)\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)")
	for d in dirs:
		for root, _, files in os.walk(d):
			for f in files:
				if not f.endswith((".c", ".h", ".S")): continue
				try: text = open(os.path.join(root, f), errors="replace").read()
				except OSError: continue
				for kind, name in pat.findall(text):
					found["attr" if kind == "attribute" else "c_attr" if kind == "c_attribute" else "builtin"].add(name)
	return found

def norm(a): return a[2:-2] if len(a) > 4 and a.startswith("__") and a.endswith("__") else a

def probe(op, names):
	src = "".join("%s: %s(%s)\n" % (n, op, n) for n in names)
	out = subprocess.run([gnu + "gcc", "-E", "-P", "-x", "c", "-"], input=src, capture_output=True, text=True, check=True).stdout
	vals = {}
	for line in out.splitlines():
		n, _, v = line.partition(": ")
		if n: vals[n] = int(v.strip().rstrip("L"))
	assert set(vals) == set(names), set(names) ^ set(vals)
	return vals

s = scan(sys.argv[2:])
attrs = sorted({norm(a) for a in ATTRS} | {norm(a) for a in s["attr"]})
c_attrs = sorted({norm(a) for a in C_ATTRS} | {norm(a) for a in s["c_attr"]})
builtins = sorted(set(BUILTINS) | s["builtin"])
version = subprocess.run([gnu + "gcc", "-dumpfullversion"], capture_output=True, text=True).stdout.strip()
print("/* has.h — GENERATED by gen-has.py from GCC %s: its __has_attribute / __has_c_attribute / __has_builtin answers. */" % version)
for tab, op, names in (("has_attr", "__has_attribute", attrs), ("has_c_attr", "__has_c_attribute", c_attrs), ("has_builtin", "__has_builtin", builtins)):
	vals = probe(op, names)
	print("static const struct { const char *name; long val; } %s_tab[] = {" % tab)
	for n in names: print('\t{ "%s", %d },' % (n, vals[n]))
	print("};")
