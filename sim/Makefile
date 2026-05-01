# sim/Makefile

CC = gcc
CFLAGS = -Wall -O3 -g -flto -march=native -fomit-frame-pointer -DNDEBUG
BUILD = build

# --- sim-core: emulator + debug stub (no DWARF) ---
CORE_DIRS = src src/core src/arch/armv7m src/hw/stm32 src/devices src/soc src/machine src/debug
CORE_INCLUDES = $(addprefix -I,$(CORE_DIRS))

CORE_SRCS = src/main.c \
       src/core/membus.c src/core/spi_bus.c src/core/chardev.c src/core/event_queue.c src/core/elf_load.c \
       src/arch/armv7m/armv7m_cpu.c src/arch/armv7m/armv7m_nvic.c src/arch/armv7m/armv7m_systick.c \
       src/hw/stm32/stm32_uart.c src/hw/stm32/stm32_spi.c src/hw/stm32/stm32_gpio.c \
       src/hw/stm32/stm32_exti.c src/hw/stm32/stm32_adc.c src/hw/stm32/stm32_dma.c \
       src/devices/ili9341.c src/devices/max98357a.c src/devices/trace_dev.c \
       src/soc/stm32f411.c \
       src/machine/gameboy.c src/machine/machine.c \
       src/debug/dbg_stub.c

CORE_OBJS = $(patsubst src/%.c,$(BUILD)/%.o,$(CORE_SRCS))

# --- sim-dbg: debugger CLI (DWARF, symbols, RTOS tasks) ---
DBG_DIRS = src/debugger src/arch/armv7m
DBG_INCLUDES = $(addprefix -I,$(DBG_DIRS))

DBG_SRCS = src/debugger/dbg_main.c src/debugger/dbg_client.c \
           src/debugger/dbg_cmd.c src/debugger/elf_sym.c

DBG_OBJS = $(patsubst src/%.c,$(BUILD)/%.o,$(DBG_SRCS))
DBG_LDFLAGS =

# Check for readline
HAVE_READLINE := $(shell echo '\#include <readline/readline.h>' | $(CC) -E - >/dev/null 2>&1 && echo yes)
ifeq ($(HAVE_READLINE),yes)
  DBG_CFLAGS = -DHAVE_READLINE
  DBG_LDFLAGS = -lreadline
endif

# --- Headers (trigger rebuild on any header change) ---
HDRS = $(foreach d,$(CORE_DIRS) $(DBG_DIRS),$(wildcard $(d)/*.h))

# --- Targets ---
all: $(BUILD)/sim-core $(BUILD)/sim-dbg test

$(BUILD)/sim-core: $(CORE_OBJS)
	$(CC) -flto -o $@ $^

$(BUILD)/sim-dbg: $(DBG_OBJS)
	$(CC) -flto -o $@ $^ $(DBG_LDFLAGS)

# Core objects
$(BUILD)/%.o: src/%.c $(HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CORE_INCLUDES) -c -o $@ $<

# Debugger objects (different includes, optional readline)
$(BUILD)/debugger/%.o: src/debugger/%.c $(HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DBG_CFLAGS) $(DBG_INCLUDES) -c -o $@ $<

test: $(BUILD)/sim-core
	python3 tests/run_tests.py

clean:
	rm -rf $(BUILD)
	make -C tests/firmware clean

.PHONY: all clean test
