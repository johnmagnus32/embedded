# sim/Makefile — unified simulator (sim-core)

CC = gcc
CFLAGS = -Wall -O3 -g -flto -march=native -fomit-frame-pointer -DNDEBUG
BUILD = build

SRC_DIRS = src src/core src/arch/armv7m src/hw/stm32 src/devices src/devices/ice40up5k src/soc src/machine src/debug
INCLUDES = $(addprefix -I,$(SRC_DIRS))

SRCS = src/main.c \
       src/core/membus.c src/core/spi_bus.c src/core/chardev.c \
       src/core/event_queue.c src/core/elf_load.c src/core/signal_trace.c \
       src/arch/armv7m/armv7m_cpu.c src/arch/armv7m/armv7m_nvic.c \
       src/arch/armv7m/armv7m_systick.c \
       src/hw/stm32/stm32_uart.c src/hw/stm32/stm32_spi.c \
       src/hw/stm32/stm32_gpio.c src/hw/stm32/stm32_exti.c \
       src/hw/stm32/stm32_adc.c src/hw/stm32/stm32_dma.c \
       src/hw/stm32/stm32_syscfg.c \
       src/devices/ili9341.c src/devices/max98357a.c \
       src/devices/trace_dev.c src/devices/w25q128.c \
       src/devices/ice40up5k/ice40up5k.c \
       src/devices/ice40up5k/netlist.c \
       src/devices/ice40up5k/eval.c \
       src/devices/ice40up5k/cJSON.c \
       src/soc/stm32f411.c \
       src/machine/gameboy.c src/machine/gameboy_v2.c src/machine/fpga_test.c src/machine/machine.c \
       src/debug/dbg_stub.c

OBJS = $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))
HDRS = $(foreach d,$(SRC_DIRS),$(wildcard $(d)/*.h))

all: $(BUILD)/sim-core

$(BUILD)/sim-core: $(OBJS)
	$(CC) -flto -o $@ $^

$(BUILD)/%.o: src/%.c $(HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

test: $(BUILD)/sim-core
	python3 tests/run_tests.py

# Standalone regression test for ice40 gate-level evaluator SPI integrity
ICE40_SRCS = src/devices/ice40up5k/ice40up5k.c src/devices/ice40up5k/netlist.c \
             src/devices/ice40up5k/eval.c src/devices/ice40up5k/cJSON.c
$(BUILD)/test_ice40_spi_integrity: tests/test_ice40_spi_integrity.c $(ICE40_SRCS) $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ tests/test_ice40_spi_integrity.c $(ICE40_SRCS)

test-ice40-spi: $(BUILD)/test_ice40_spi_integrity
	$(BUILD)/test_ice40_spi_integrity ../projects/gameboy-v2/build/ppu_top.json

# Full end-to-end display test (MCU + FPGA + ILI9341 framebuffer check)
DISPLAY_TEST_SRCS = $(filter-out src/main.c,$(SRCS))
$(BUILD)/test_gameboy_v2_display: tests/test_gameboy_v2_display.c $(DISPLAY_TEST_SRCS) $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ tests/test_gameboy_v2_display.c $(DISPLAY_TEST_SRCS)

test-display: $(BUILD)/test_gameboy_v2_display
	$(BUILD)/test_gameboy_v2_display ../projects/gameboy-v2/build/gameboy-v2.elf ../projects/gameboy-v2/build/ppu_top.json

# Fast end-to-end test using Verilator-compiled PPU (seconds, not minutes)
VERILATOR_DIR = src/devices/verilator_ppu
VERILATOR_OBJ = $(VERILATOR_DIR)/obj_dir
VERILATOR_INC = -I$(VERILATOR_OBJ) -I$(shell verilator --getenv VERILATOR_ROOT)/include

$(BUILD)/test_gameboy_v2_e2e: tests/test_gameboy_v2_e2e.cpp $(VERILATOR_OBJ)/Vppu_wrapper__ALL.a
	@mkdir -p $(BUILD)
	g++ -O2 $(VERILATOR_INC) -o $@ $< \
		$(VERILATOR_OBJ)/Vppu_wrapper__ALL.a \
		$(shell verilator --getenv VERILATOR_ROOT)/include/verilated.cpp \
		$(shell verilator --getenv VERILATOR_ROOT)/include/verilated_threads.cpp \
		-lpthread

test-e2e: $(BUILD)/test_gameboy_v2_e2e
	$(BUILD)/test_gameboy_v2_e2e

clean:
	rm -rf $(BUILD)

.PHONY: all clean test test-ice40-spi test-display test-e2e
