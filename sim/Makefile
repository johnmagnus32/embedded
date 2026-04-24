# sim/Makefile — builds sim-core into build/

CC = gcc
CFLAGS = -Wall -O2 -g
SRC = src/sim-core
BUILD = build

SRCS = $(SRC)/main.c $(SRC)/cpu.c $(SRC)/mem.c $(SRC)/vis.c $(SRC)/elf_sym.c $(SRC)/state.c
OBJS = $(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(SRCS))
HDRS = $(SRC)/cpu.h $(SRC)/vis.h $(SRC)/elf_sym.h $(SRC)/state.h

all: $(BUILD)/sim-core

$(BUILD)/sim-core: $(OBJS)
	$(CC) -o $@ $^

$(BUILD)/%.o: $(SRC)/%.c $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(SRC) -c -o $@ $<

clean:
	rm -f $(BUILD)/*

.PHONY: all clean
