# Makefile — build ld-gv3.so.1, the from-scratch dynamic linker.
#
# A PIC shared object with an entry point (_start); the kernel loads it when it
# sees a program's PT_INTERP = /lib/ld-gv3.so.1, jumps to our _start, and we
# resolve everything and hand off to the real program. No libc dependency —
# dl_main.c uses raw syscalls.
#
#   make              # build ld-gv3.so.1 for the default board (t113)
#   make BOARD=virt   # VFP-free for QEMU
#   make test         # run the L2+S1 host unit tests

CROSS_COMPILE ?= arm-buildroot-linux-musleabihf-
CC      := $(CROSS_COMPILE)gcc

BUILD := ../build
SRC   := src

BOARD ?= t113
ifeq ($(BOARD),virt)
ARCH_FLAGS := -mcpu=cortex-a7 -marm -mgeneral-regs-only
else
ARCH_FLAGS := -mcpu=cortex-a7 -marm
endif

CFLAGS := $(ARCH_FLAGS) -fPIC -ffreestanding -nostdlib -nostartfiles \
          -fno-builtin -fno-stack-protector -Os -Wall -Wextra -I$(SRC)

# The linker is just two source files: the asm entry + the C core.
OBJS := $(BUILD)/ld/dl_entry.o $(BUILD)/ld/dl_main.o

.PHONY: all clean test
all: $(BUILD)/ld-gv3.so.1

$(BUILD)/ld:
	mkdir -p $(BUILD)/ld

$(BUILD)/ld/%.o: $(SRC)/%.S | $(BUILD)/ld
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/ld/%.o: $(SRC)/%.c | $(BUILD)/ld
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/ld-gv3.so.1: $(OBJS)
	$(CC) $(CFLAGS) -shared -nostdlib -Wl,-e,_start \
	  -Wl,--build-id=none -Wl,-soname,ld-gv3.so.1 \
	  $(OBJS) -o $@
	@echo "  LD  $@ ($$($(CROSS_COMPILE)size $@ | tail -1 | awk '{print $$4}') bytes)"

test:
	./test/run.sh

clean:
	rm -rf $(BUILD)/ld $(BUILD)/ld-gv3.so.1
