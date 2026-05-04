TC      = /home/johmagnu/zephyr-sdk-0.16.8/arm-zephyr-eabi/bin/arm-zephyr-eabi
CC      = $(TC)-gcc
CFLAGS  = -mcpu=cortex-m4 -mthumb -nostdlib -ffreestanding -Wall -Og -g -I.
LDFLAGS = -T linker.ld -nostdlib

BUILD = build

# Functional tests
FUNC_ARMV7M = test_alu test_alu2 test_arith test_call test_it test_ldm_stm \
              test_ldst_w test_mem test_misc test_shift test_wide
FUNC_STM32  = test_dma test_dma_spi test_dma_i2s_kick test_gpio_exti test_gpio_irq_chardev test_syscfg test_irq test_it_irq \
              test_uart_chardev test_w25q128
FUNC_CORE   = test_elf

# Performance tests
PERF_ARMV7M = test_mips test_irq_latency test_systick_period
PERF_STM32  = test_spi_throughput test_membus \
              test_spi_fps_full test_spi_fps_partial test_spi_fps_sustained \
              test_dma_fps test_dma_fps_full test_dma_fps_sustained \
              test_i2s_cpu test_i2s_dma
PERF_ILI    = test_display_refresh test_display_cpu test_display_dma
PERF_MAX    = test_audio

# Build paths
ELFS  = $(addprefix $(BUILD)/func/arch/armv7m/,$(addsuffix .elf,$(FUNC_ARMV7M)))
ELFS += $(addprefix $(BUILD)/func/hw/stm32/,$(addsuffix .elf,$(FUNC_STM32)))
ELFS += $(addprefix $(BUILD)/func/core/,$(addsuffix .elf,$(FUNC_CORE)))
ELFS += $(addprefix $(BUILD)/perf/arch/armv7m/,$(addsuffix .elf,$(PERF_ARMV7M)))
ELFS += $(addprefix $(BUILD)/perf/hw/stm32/,$(addsuffix .elf,$(PERF_STM32)))
ELFS += $(addprefix $(BUILD)/perf/devices/ili9341/,$(addsuffix .elf,$(PERF_ILI)))
ELFS += $(addprefix $(BUILD)/perf/devices/max98357a/,$(addsuffix .elf,$(PERF_MAX)))

all: $(ELFS)

# Pattern rules for each source directory
$(BUILD)/func/arch/armv7m/%.elf: func/arch/armv7m/%.c test.h linker.ld
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(BUILD)/func/hw/stm32/%.elf: func/hw/stm32/%.c test.h linker.ld
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(BUILD)/func/core/test_elf.elf: func/core/test_elf.c test.h linker_elf.ld
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -T linker_elf.ld -nostdlib -o $@ $<

$(BUILD)/perf/arch/armv7m/%.elf: perf/arch/armv7m/%.c test.h linker.ld
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(BUILD)/perf/hw/stm32/%.elf: perf/hw/stm32/%.c test.h linker.ld
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(BUILD)/perf/devices/ili9341/%.elf: perf/devices/ili9341/%.c test.h linker.ld
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(BUILD)/perf/devices/max98357a/%.elf: perf/devices/max98357a/%.c test.h linker.ld
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	rm -rf $(BUILD)

.PHONY: all clean
