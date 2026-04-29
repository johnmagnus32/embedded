# sim/Makefile

CC = gcc
CFLAGS = -Wall -O2 -g
SRC = src/sim-core
BUILD = build

SRCS = $(SRC)/main.c $(SRC)/dbg_server.c $(SRC)/dbg_cmd.c $(SRC)/dbg_eval.c $(SRC)/dbg_tasks.c \
       $(SRC)/armv7m_cpu.c $(SRC)/membus.c $(SRC)/elf_sym.c \
       $(SRC)/gameboy.c $(SRC)/machine.c $(SRC)/armv7m_nvic.c $(SRC)/armv7m_systick.c $(SRC)/stm32_uart.c $(SRC)/stm32f411.c $(SRC)/chardev.c $(SRC)/trace_dev.c \
       $(SRC)/spi_bus.c $(SRC)/stm32_spi.c $(SRC)/ili9341.c $(SRC)/stm32_gpio.c $(SRC)/stm32_exti.c $(SRC)/max98357a.c $(SRC)/stm32_adc.c $(SRC)/stm32_dma.c
OBJS = $(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(SRCS))
HDRS = $(wildcard $(SRC)/*.h)

all: $(BUILD)/sim-core

$(BUILD)/sim-core: $(OBJS)
	$(CC) -o $@ $^

$(BUILD)/%.o: $(SRC)/%.c $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(SRC) -c -o $@ $<

clean:
	rm -f $(BUILD)/*

.PHONY: all clean
