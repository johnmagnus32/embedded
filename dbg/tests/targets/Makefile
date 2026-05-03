# dbg/tests/targets/Makefile
TC = /home/johmagnu/zephyr-sdk-0.16.8/arm-zephyr-eabi/bin/arm-zephyr-eabi
CC = $(TC)-gcc
CFLAGS = -mcpu=cortex-m3 -mthumb -nostdlib -ffreestanding -Wall -Og -g
LDFLAGS = -T linker.ld -nostdlib

TARGETS = test_hello test_breakpoint test_stepping test_variables test_stack
ELFS = $(addprefix build/,$(addsuffix .elf,$(TARGETS)))

all: $(ELFS)

build/%.elf: %.c startup.h linker.ld
	@mkdir -p build
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	rm -rf build

.PHONY: all clean
