# toolchain/rt/Makefile — OUR compiler runtime library (the libgcc / compiler-rt analogue): the helper routines
# compiled code calls for what ARMv7 has no instruction for (64-bit division, int64<->FP conversion, ...), in
# both the ARM RTABI names (__aeabi_*) and the libgcc names, so GCC-built objects link against it too. Its
# surface is libgcc's: the RTABI memory helpers (__aeabi_memcpy & co.) belong to the C library, as in glibc/musl.
# Built by our own cpp/cc/as/ar; os-cc links it after the libc, as gcc links libgcc.
#   make        -> build/libosrt.a
#   make test   -> differential test vs GCC's libgcc (test/rttest.c: GCC+libgcc, GCC+ours, ours+ours must agree)
BUILD ?= build
TC    := ..
CPP   := $(TC)/cpp/build/cpp
CC1   := $(TC)/cc/build/cc
AS    := $(TC)/as/build/as
AR    := $(TC)/ar/build/ar
LD    := $(TC)/ld/build/ld
GNU   ?= ../../projects/gameboy-v3/image/build/qemu/toolchain-gcc/bin/arm-forge-linux-gnueabihf-
QEMU  ?= qemu-system-arm

CSRCS := divide.c int64.c bits.c fpconv.c half.c complex.c
SSRCS := aeabi.s
OBJS  := $(CSRCS:%.c=$(BUILD)/%.o) $(SSRCS:%.s=$(BUILD)/%.o)

# Two phases: bring the tools up to date first, THEN build with them as ordinary prerequisites — so a changed
# compiler or assembler rebuilds every object (their timestamps are only trustworthy once they're rebuilt).
all: tools
	@$(MAKE) --no-print-directory lib
lib: $(BUILD)/libosrt.a

$(BUILD)/libosrt.a: $(OBJS) $(AR)
	rm -f $@; $(AR) rc $@ $(OBJS)

$(BUILD)/%.o: %.c $(CPP) $(CC1) $(AS) | $(BUILD)
	$(CPP) $< > $(BUILD)/$*.i && $(CC1) -fPIC -o $(BUILD)/$*.s $(BUILD)/$*.i && $(AS) -o $@ $(BUILD)/$*.s   # PIC: it links into libc.so / ld.so too
$(BUILD)/%.o: %.s $(AS) | $(BUILD)
	$(AS) -o $@ $<

$(BUILD):
	mkdir -p $(BUILD)
tools:
	@for t in cpp cc as ar ld; do $(MAKE) -s -C $(TC)/$$t >/dev/null || exit 1; done

test: all
	@GNU=$(abspath $(GNU)) QEMU=$(QEMU) ./test/rttest.sh

clean:
	rm -rf $(BUILD)
.PHONY: all lib tools test clean
