# sim/Makefile

CC = gcc
CFLAGS = -Wall -O2 -g
SRC = src/sim-core
BUILD = build

SRCS = $(SRC)/main.c $(SRC)/cpu.c $(SRC)/membus.c $(SRC)/elf_sym.c $(SRC)/state.c \
       $(SRC)/board.c $(SRC)/nvic.c $(SRC)/systick.c $(SRC)/uart.c $(SRC)/dts.c $(SRC)/chardev.c $(SRC)/trace_dev.c \
       $(SRC)/spi.c $(SRC)/ili9341.c $(SRC)/gpio.c
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
