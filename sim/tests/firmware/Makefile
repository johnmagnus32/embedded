TC      = /home/johmagnu/zephyr-sdk-0.16.8/arm-zephyr-eabi/bin/arm-zephyr-eabi
CC      = $(TC)-gcc
CFLAGS  = -mcpu=cortex-m4 -mthumb -nostdlib -ffreestanding -Wall -Og -g -I.
LDFLAGS = -T linker.ld -nostdlib

BUILD = build

CPU_TESTS = test_alu test_alu2 test_it test_call test_ldm_stm test_mem \
            test_shift test_wide test_arith test_misc test_ldst_w
HW_TESTS  = test_irq test_it_irq test_uart_chardev test_gpio_exti test_dma \
            test_perf_mips test_perf_systick test_perf_irq_latency \
            test_perf_spi test_perf_membus
ELF_TESTS = test_elf

CPU_ELFS = $(addprefix $(BUILD)/cpu/,$(addsuffix .elf,$(CPU_TESTS)))
HW_ELFS  = $(addprefix $(BUILD)/hw/,$(addsuffix .elf,$(HW_TESTS)))
ELF_ELFS = $(addprefix $(BUILD)/cpu/,$(addsuffix .elf,$(ELF_TESTS)))

all: $(CPU_ELFS) $(HW_ELFS) $(ELF_ELFS)

$(BUILD)/cpu/%.elf: cpu/%.c test.h linker.ld
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(BUILD)/hw/%.elf: hw/%.c test.h linker.ld
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

# test_elf needs .data/.bss init in its linker script
$(BUILD)/cpu/test_elf.elf: cpu/test_elf.c test.h linker_elf.ld
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -T linker_elf.ld -nostdlib -o $@ $<

clean:
	rm -rf $(BUILD)

.PHONY: all clean
