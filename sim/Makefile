# sim/Makefile

CC = gcc
CFLAGS = -Wall -O2 -g
BUILD = build

SRC_DIRS = src src/core src/arch/armv7m src/hw/stm32 src/devices src/soc src/machine src/debug
INCLUDES = $(addprefix -I,$(SRC_DIRS))

SRCS = src/main.c \
       src/core/membus.c src/core/spi_bus.c src/core/chardev.c \
       src/arch/armv7m/armv7m_cpu.c src/arch/armv7m/armv7m_nvic.c src/arch/armv7m/armv7m_systick.c \
       src/hw/stm32/stm32_uart.c src/hw/stm32/stm32_spi.c src/hw/stm32/stm32_gpio.c \
       src/hw/stm32/stm32_exti.c src/hw/stm32/stm32_adc.c src/hw/stm32/stm32_dma.c \
       src/devices/ili9341.c src/devices/max98357a.c src/devices/trace_dev.c \
       src/soc/stm32f411.c \
       src/machine/gameboy.c src/machine/machine.c \
       src/debug/dbg_server.c src/debug/dbg_cmd.c src/debug/dbg_eval.c src/debug/dbg_tasks.c src/core/elf_sym.c

OBJS = $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))
HDRS = $(foreach d,$(SRC_DIRS),$(wildcard $(d)/*.h))

all: $(BUILD)/sim-core

$(BUILD)/sim-core: $(OBJS)
	$(CC) -o $@ $^

$(BUILD)/%.o: src/%.c $(HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	rm -rf $(BUILD)

.PHONY: all clean
