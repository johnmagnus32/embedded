# toolchain/cc/Makefile — build our from-scratch C compiler (a HOST program; emits ARM .s text).
# `make` -> build/cc ; `make test` -> compile+assemble+link+RUN sample programs, check results ; `make clean`.
BUILD  ?= build
CC     ?= cc
CFLAGS ?= -O0 -g -Wall -Wextra -std=c11
CFLAGS += -pthread   # the compiler runs on a big-stack thread (cc.c)
COMMON := ../common
CFLAGS += -I$(COMMON)

OURAS := ../as/build/as
OURLD := ../ld/build/ld
RTLIB := ../rt/build/libosrt.a   # our compiler runtime (64-bit division, int64<->FP): every test link ends with it, as gcc's end with libgcc
GNU   ?= ../../projects/gameboy-v3/image/build/qemu/toolchain-gcc/bin/arm-forge-linux-gnueabihf-
QEMU  ?= qemu-system-arm

OBJS := $(BUILD)/cc.o $(BUILD)/lex.o $(BUILD)/parse.o $(BUILD)/gen.o $(BUILD)/type.o $(BUILD)/strmap.o

$(BUILD)/cc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(BUILD)/%.o: %.c cc.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/strmap.o: $(COMMON)/strmap.c $(COMMON)/strmap.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $(BUILD)

# Execution gate: for each tests/*.c, compile with OUR cc, assemble with OUR as, then link a tiny
# semihosting crt (tests/crt.s) that calls main() and reports its return value as the qemu exit code.
# The expected value is the number in the .c's first-line comment: `// expect: N`. OUR ld does the
# bare-metal link (-Ttext 0x40000000, virt RAM); qemu-system-arm runs it and we check the exit status.
# Each tests/reject/*.c is invalid C that cc must refuse, with the message in its `// error: TEXT` line.
test: $(BUILD)/cc
	@if [ ! -x "$$(command -v $(QEMU))" ]; then echo "SKIP: $(QEMU) not found"; exit 0; fi
	@$(MAKE) -s -C ../as >/dev/null; $(MAKE) -s -C ../ld >/dev/null; $(MAKE) -s -C ../rt >/dev/null
	@pass=0; fail=0; t=$$(mktemp -d); \
	$(OURAS) -o $$t/crt.o tests/crt.s; \
	for c in tests/*.c; do \
	  [ "$$(basename $$c)" = uart.c ] && continue; \
	  want=$$(sed -n 's|.*//[[:space:]]*expect:[[:space:]]*\([0-9]*\).*|\1|p' $$c | head -1); \
	  $(BUILD)/cc -o $$t/p.s $$c || { echo "FAIL $$c: cc errored"; fail=$$((fail+1)); continue; }; \
	  $(OURAS) -o $$t/p.o $$t/p.s || { echo "FAIL $$c: our as rejected cc output"; fail=$$((fail+1)); continue; }; \
	  $(OURLD) -Ttext 0x40000000 -o $$t/p.elf $$t/crt.o $$t/p.o $(RTLIB) 2>$$t/lderr || { echo "FAIL $$c: link: $$(cat $$t/lderr)"; fail=$$((fail+1)); continue; }; \
	  timeout 10 $(QEMU) -M virt -cpu cortex-a7 -nographic -semihosting -net none -kernel $$t/p.elf >/dev/null 2>&1; got=$$?; \
	  if [ "$$got" = "$$want" ]; then echo "PASS $$(basename $$c): main() == $$got"; pass=$$((pass+1)); \
	  else echo "FAIL $$(basename $$c): got $$got, want $$want"; fail=$$((fail+1)); fi; \
	done; \
	if GNU=$(GNU) CC=$(BUILD)/cc bash tests/inline-emit.sh > $$t/ie 2>&1; then echo "PASS $$(cat $$t/ie)"; pass=$$((pass+1)); \
	else echo "FAIL inline-emit:"; sed 's/^/    /' $$t/ie; fail=$$((fail+1)); fi; \
	for c in tests/reject/*.c; do \
	  want=$$(sed -n 's|^//[[:space:]]*error:[[:space:]]*||p' $$c | head -1); \
	  if $(BUILD)/cc -o $$t/r.s $$c 2>$$t/rerr; then echo "FAIL $$c: accepted (want an error: $$want)"; fail=$$((fail+1)); \
	  elif grep -qF "$$want" $$t/rerr; then echo "PASS $$(basename $$c): rejected ($$want)"; pass=$$((pass+1)); \
	  else echo "FAIL $$c: rejected, but not with '$$want': $$(cat $$t/rerr)"; fail=$$((fail+1)); fi; \
	done; \
	$(BUILD)/cc -fPIC -o $$t/cd.s tests/const_data.c; \
	secs=$$(awk '/\.section|^\t\.data$$/{s=$$1 == ".data" ? ".data" : $$2} /^(k|tab|fns|cv|w):/{printf "%s=%s ", $$1, s}' $$t/cd.s); \
	case "$$secs" in "k:=.rodata tab:=.data.rel.ro,\"aw\",%progbits fns:=.data.rel.ro,\"aw\",%progbits cv:=.data w:=.data ") \
	  echo "PASS const data: .rodata / .data.rel.ro (-fPIC, addresses) / .data (volatile, non-const)"; pass=$$((pass+1));; \
	  *) echo "FAIL const data placement: $$secs"; fail=$$((fail+1));; esac; \
	$(BUILD)/cc -fPIC -o $$t/pp.s tests/pic_preempt.c && $(OURAS) -o $$t/pp.o $$t/pp.s; \
	rel=$$(../readelf/build/readelf -r $$t/pp.o | awk '$$3 ~ /^R_ARM_(REL32|GOT_PREL)$$/ {printf "%s=%s ", $$5, $$3}'); \
	case "$$rel" in ".rodata=R_ARM_REL32 glob_var=R_ARM_GOT_PREL hidden_var=R_ARM_REL32 .data=R_ARM_REL32 .rodata=R_ARM_REL32 ") \
	  echo "PASS -fPIC addressing: PC-relative for a literal/static/hidden symbol, the GOT only for an interposable one"; pass=$$((pass+1));; \
	  *) echo "FAIL -fPIC addressing: $$rel"; fail=$$((fail+1));; esac; \
	printf 'int f(void){return nope(1);}\n' > $$t/impl.c; \
	iw=$$($(BUILD)/cc $$t/impl.c -o $$t/impl.s 2>&1; echo "rc=$$?"); \
	ie=$$($(BUILD)/cc -Werror=implicit-function-declaration $$t/impl.c -o $$t/impl.s 2>&1; echo "rc=$$?"); \
	iq=$$($(BUILD)/cc -Wno-implicit-function-declaration $$t/impl.c -o $$t/impl.s 2>&1; echo "rc=$$?"); \
	if printf '%s' "$$iw" | grep -q "warning: implicit declaration of function 'nope'" && printf '%s' "$$iw" | grep -q 'rc=0' \
	   && printf '%s' "$$ie" | grep -q "implicit declaration of function 'nope'" && ! printf '%s' "$$ie" | grep -q 'rc=0' \
	   && [ "$$iq" = "rc=0" ]; then \
	  echo "PASS implicit declaration: warns by default (as gcc 13), errors under -Werror=, silent under -Wno-"; pass=$$((pass+1)); \
	else echo "FAIL implicit declaration modes: default[$$iw] werror[$$ie] wno[$$iq]"; fail=$$((fail+1)); fi; \
	for spec in "implicit-int|static x;|type defaults" "strict-prototypes|int f();|a prototype"; do \
	  nm=$${spec%%|*}; rest=$${spec#*|}; src=$${rest%%|*}; want=$${rest#*|}; \
	  printf '%s\n' "$$src" > $$t/dg.c; \
	  d=$$($(BUILD)/cc -W$$nm -o $$t/dg.s $$t/dg.c 2>&1; echo rc=$$?); \
	  e=$$($(BUILD)/cc -Werror=$$nm -o $$t/dg.s $$t/dg.c 2>&1; echo rc=$$?); \
	  q=$$($(BUILD)/cc -Wno-$$nm -o $$t/dg.s $$t/dg.c 2>&1; echo rc=$$?); \
	  w=$$($(BUILD)/cc -w -W$$nm -o $$t/dg.s $$t/dg.c 2>&1; echo rc=$$?); \
	  if printf '%s' "$$d" | grep -qF "$$want" && printf '%s' "$$d" | grep -q 'rc=0' \
	     && printf '%s' "$$e" | grep -qF "$$want" && ! printf '%s' "$$e" | grep -q 'rc=0' \
	     && [ "$$q" = "rc=0" ] && [ "$$w" = "rc=0" ]; then \
	    echo "PASS -W$$nm: warns, errors under -Werror=, silent under -Wno- and -w"; pass=$$((pass+1)); \
	  else echo "FAIL -W$$nm modes: W[$$d] Werror[$$e] Wno[$$q] w[$$w]"; fail=$$((fail+1)); fi; done; \
	printf 'struct s { int a; ; int b; } v;\nunsigned u;\nstruct t { int c; } w;\n' > $$t/ii.c; \
	if [ -z "$$($(BUILD)/cc -Werror=implicit-int -o $$t/ii.s $$t/ii.c 2>&1)" ]; then \
	  echo "PASS -Wimplicit-int: an empty declaration is not an implicit int -- the kernel generates bare semicolon members"; pass=$$((pass+1)); \
	else echo "FAIL -Wimplicit-int fired on an empty declaration: $$($(BUILD)/cc -Werror=implicit-int -o $$t/ii.s $$t/ii.c 2>&1)"; fail=$$((fail+1)); fi; \
	ad=$$($(BUILD)/cc -o $$t/ad.s tests/attr_diag.c 2>&1; echo "rc=$$?"); \
	adw=$$(printf '%s' "$$ad" | grep -c "attribute warning: emitted on purpose"); \
	ade=$$(printf '%s' "$$ad" | grep -c "attribute error"); \
	if [ "$$adw" = 1 ] && [ "$$ade" = 0 ] && printf '%s' "$$ad" | grep -q 'rc=0'; then \
	  echo "PASS attribute error/warning: raised where the call is EMITTED, so a folded-away call is silent (BUILD_BUG_ON)"; pass=$$((pass+1)); \
	else echo "FAIL attribute error/warning: want 1 warning + 0 errors + rc=0, got [$$ad]"; fail=$$((fail+1)); fi; \
	$(BUILD)/cc -mno-unaligned-access -o $$t/ua.s tests/unaligned_access.c; \
	$(OURAS) -o $$t/ua.o $$t/ua.s && $(OURLD) -Ttext 0x40000000 -o $$t/ua.elf $$t/crt.o $$t/ua.o $(RTLIB) 2>/dev/null; \
	timeout 10 $(QEMU) -M virt -cpu cortex-a7 -nographic -semihosting -net none -kernel $$t/ua.elf >/dev/null 2>&1; uag=$$?; \
	uaw=$$(awk '/^rd_check:/,0' $$t/ua.s | grep -cE '(ldr|str) r[0-9]+, \\[r[0-9]+\\]'); \
	ualdrb=$$(grep -c 'ldrb' $$t/ua.s); \
	if [ "$$uag" = 42 ] && [ "$$ualdrb" -gt 0 ]; then echo "PASS -mno-unaligned-access: packed members accessed byte-wise, values unchanged"; pass=$$((pass+1)); \
	else echo "FAIL -mno-unaligned-access: exit $$uag (want 42), ldrb count $$ualdrb (want >0)"; fail=$$((fail+1)); fi; \
	$(BUILD)/cc -o $$t/uaw.s tests/unaligned_access.c; \
	if [ "$$(grep -c ldrb $$t/uaw.s)" -lt "$$ualdrb" ]; then echo "PASS -munaligned-access default: the same code uses wide accesses, as gcc's default does"; pass=$$((pass+1)); \
	else echo "FAIL the default should use FEWER byte accesses than -mno-unaligned-access"; fail=$$((fail+1)); fi; \
	$(BUILD)/cc -o $$t/pb.s tests/packed_bitfield.c; \
	bf="$$(awk '/^q_wr:/,/\.size q_wr/' $$t/pb.s | grep -c strb) $$(awk '/^q_wr:/,/\.size q_wr/' $$t/pb.s | grep -cE 'str r[0-9a-z]+, \\[r[0-3]') $$(awk '/^q_rd:/,/\.size q_rd/' $$t/pb.s | grep -c ldrb)"; \
	case "$$bf" in "3 0 3") echo "PASS packed bit-field access: byte-wise over the field's own 3 bytes, never the unit's 4th"; pass=$$((pass+1));; \
	  *) echo "FAIL packed bit-field access (strb/word-store/ldrb counts): $$bf, want '3 0 3'"; fail=$$((fail+1));; esac; \
	$(BUILD)/cc -o $$t/vis.s tests/visibility.c; vis=$$(grep -oE '\.(hidden|protected|internal) [a-z_]+' $$t/vis.s | sort | tr '\n' ' '); \
	case "$$vis" in ".hidden hidden_fn .protected prot_var ") echo "PASS visibility: .hidden / .protected emitted (none for an unused declaration, as GCC)"; pass=$$((pass+1));; \
	  *) echo "FAIL visibility directives: '$$vis'"; fail=$$((fail+1));; esac; \
	$(BUILD)/cc -o $$t/pa.s tests/proto_attrs.c; sec=$$(awk '/^\t\.section/{s=$$2} /^in_init:/{print s}' $$t/pa.s); \
	case "$$sec" in .text.proto_init*) echo "PASS prototype attributes: a prototype's section() places the definition"; pass=$$((pass+1));; \
	  *) echo "FAIL prototype section(): in_init is in '$$sec'"; fail=$$((fail+1));; esac; \
	$(BUILD)/cc -o $$t/ca.s tests/const_array_typedef.c; sec=$$(awk '/^\t\.section|^\t\.data$$|^\t\.text$$/{s=$$1 == ".section" ? $$2 : $$1} /^table:/{print s}' $$t/ca.s); \
	case "$$sec" in .rodata*) echo "PASS const array typedef: in .rodata"; pass=$$((pass+1));; \
	  *) echo "FAIL const array typedef: table is in '$$sec'"; fail=$$((fail+1));; esac; \
	$(BUILD)/cc -o $$t/rz.s tests/returns_twice.c; rz=$$(awk 'prev ~ /add sp, sp, ip/ && /sub sp, sp, #16/ {n++} {prev = $$0} END {print n + 0}' $$t/rz.s); \
	[ "$$rz" = 0 ] && { echo "PASS call return: sp comes back in one move (never above a live word: an interrupt pushes below sp)"; pass=$$((pass+1)); } \
	  || { echo "FAIL call return: $$rz 'add sp, sp, ip; sub sp, sp, #16' windows"; fail=$$((fail+1)); }; \
	$(BUILD)/cc -o $$t/u.s tests/uart.c && $(OURAS) -o $$t/u.o $$t/u.s && $(OURLD) -Ttext 0x40000000 -o $$t/u.elf $$t/crt.o $$t/u.o $(RTLIB); \
	out=$$(timeout 10 $(QEMU) -M virt -cpu cortex-a7 -nographic -semihosting -net none -kernel $$t/u.elf 2>/dev/null); \
	case "$$out" in *"cc UART works"*) echo "PASS uart: cc program printed via PL011 (real I/O)"; pass=$$((pass+1));; \
	                *) echo "FAIL uart: expected UART output, got '$$out'"; fail=$$((fail+1));; esac; \
	gi=$$($(GNU)gcc -print-file-name=include); \
	$(GNU)gcc -E -Os -mcpu=cortex-a7 -marm -ffreestanding -nostdinc -isystem $$gi -I../../libc/include -I../../kernel/include/uapi ../../libc/src/string.c > $$t/str.pp.c 2>/dev/null; \
	printf '%s\n' 'int strlen(char*s);' 'int strcmp(char*a,char*b);' 'void*malloc(int n){return 0;}' \
	  'int main(void){int n=strlen("hello");int e=strcmp("abc","abc");int d=strcmp("abd","abc");int r=n;if(e==0)r+=10;if(d>0)r+=100;return r;}' > $$t/lm.c; \
	if $(BUILD)/cc -o $$t/str.s $$t/str.pp.c && $(OURAS) -o $$t/str.o $$t/str.s \
	   && $(BUILD)/cc -o $$t/lm.s $$t/lm.c && $(OURAS) -o $$t/lm.o $$t/lm.s \
	   && $(OURLD) -Ttext 0x40000000 -o $$t/lp.elf $$t/crt.o $$t/lm.o $$t/str.o $(RTLIB) 2>/dev/null; then \
	  timeout 10 $(QEMU) -M virt -cpu cortex-a7 -nographic -semihosting -net none -kernel $$t/lp.elf >/dev/null 2>&1; g=$$?; \
	  [ "$$g" = 115 ] && { echo "PASS libc-string: our cc+as compiled real libc/src/string.c; strlen/strcmp ran = $$g"; pass=$$((pass+1)); } \
	                  || { echo "FAIL libc-string: got $$g, want 115"; fail=$$((fail+1)); }; \
	else echo "FAIL libc-string: compile/assemble/link of real string.c failed"; fail=$$((fail+1)); fi; \
	rm -rf $$t; echo "----- $$pass passed, $$fail failed -----"; [ $$fail -eq 0 ]

# GCC 13.3 gcc.c-torture/execute (1627 self-checking programs) through our cc/as/ld on qemu, scored only on
# tests the GNU oracle passes (torture/run.py). The suite is extracted once from the os build's gcc tarball.
GCC_TAR ?= ../../projects/gameboy-v3/image/build/downloads/gcc-13.3.0.tar.xz
CTORTURE_WORK ?= /tmp/ctorture
torture: $(BUILD)/cc
	@$(MAKE) -s -C ../as >/dev/null; $(MAKE) -s -C ../ld >/dev/null
	@test -d $(CTORTURE_WORK)/gcc-13.3.0/gcc/testsuite/gcc.c-torture/execute || { \
	  test -f $(GCC_TAR) || { echo "torture: $(GCC_TAR) missing (fetch it via the os build)"; exit 1; }; \
	  mkdir -p $(CTORTURE_WORK) && tar -xJf $(GCC_TAR) -C $(CTORTURE_WORK) gcc-13.3.0/gcc/testsuite/gcc.c-torture/execute; }
	@CTORTURE_WORK=$(CTORTURE_WORK) GNU=$(abspath $(GNU)) python3 torture/run.py -j $$(nproc)
# gcc.c-torture/compile (~1900 compile-only programs): the front end's coverage, scored where GCC compiles them.
torture-compile: $(BUILD)/cc
	@$(MAKE) -s -C ../as >/dev/null
	@test -d $(CTORTURE_WORK)/gcc-13.3.0/gcc/testsuite/gcc.c-torture/compile || { \
	  test -f $(GCC_TAR) || { echo "torture: $(GCC_TAR) missing (fetch it via the os build)"; exit 1; }; \
	  mkdir -p $(CTORTURE_WORK) && tar -xJf $(GCC_TAR) -C $(CTORTURE_WORK) gcc-13.3.0/gcc/testsuite/gcc.c-torture/compile; }
	@CTORTURE_WORK=$(CTORTURE_WORK) GNU=$(abspath $(GNU)) python3 torture/run.py --compile -j $$(nproc)

clean:
	rm -rf $(BUILD)

# AAPCS / AAPCS-VFP interop with GCC, both directions (needs the torture runtime: `make torture` first).
abi-interop: $(BUILD)/cc
	@$(MAKE) -s -C ../as >/dev/null; $(MAKE) -s -C ../ld >/dev/null
	@GNU=$(abspath $(GNU)) CTORTURE_WORK=$(CTORTURE_WORK) ./torture/abi.sh

# The parser-lowered builtins (overflow arithmetic over every type combination, FP classification, abs, isdigit)
# against exact arithmetic computed in Python (torture/builtin_diff.py).
builtin-diff: $(BUILD)/cc
	@$(MAKE) -s -C ../as >/dev/null; $(MAKE) -s -C ../ld >/dev/null; $(MAKE) -s -C ../rt >/dev/null
	@python3 torture/builtin_diff.py

# Programs whose results are compared with GCC's rather than predicted (torture/diff/*.c; torture/gnu_diff.sh).
gnu-diff: $(BUILD)/cc
	@$(MAKE) -s -C ../as >/dev/null; $(MAKE) -s -C ../ld >/dev/null; $(MAKE) -s -C ../rt >/dev/null
	@GNU=$(abspath $(GNU)) CTORTURE_WORK=$(CTORTURE_WORK) ./torture/gnu_diff.sh
	@OPT="-O0 -mword-relocations" GNU=$(abspath $(GNU)) CTORTURE_WORK=$(CTORTURE_WORK) ./torture/gnu_diff.sh

# The compiler's own headers (include/: stddef.h, stdint.h, limits.h, float.h, …) against GCC's — every constant and
# type, freestanding and hosted (tests/headers-diff.sh).
headers-diff: $(BUILD)/cc
	@$(MAKE) -s -C ../cpp >/dev/null; $(MAKE) -s -C ../as >/dev/null
	@GNU=$(abspath $(GNU)) T=$(abspath ..) ./tests/headers-diff.sh

.PHONY: clean test torture torture-compile abi-interop builtin-diff gnu-diff headers-diff
