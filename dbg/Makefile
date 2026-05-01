# dbg/Makefile — sim-dbg debugger CLI

CC = gcc
CFLAGS = -Wall -O2 -g
BUILD = build

SRCS = src/dbg_main.c src/dbg_client.c src/dbg_cmd.c src/elf_sym.c
OBJS = $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))
HDRS = $(wildcard src/*.h)

# elf_sym.c needs armv7m_cpu.h for memory map constants
SIM_INCLUDES = -I../sim/src/arch/armv7m

# Check for readline
HAVE_READLINE := $(shell echo '\#include <readline/readline.h>' | $(CC) -E - >/dev/null 2>&1 && echo yes)
ifeq ($(HAVE_READLINE),yes)
  CFLAGS += -DHAVE_READLINE
  LDFLAGS = -lreadline
endif

all: $(BUILD)/sim-dbg

$(BUILD)/sim-dbg: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(BUILD)/%.o: src/%.c $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -Isrc $(SIM_INCLUDES) -c -o $@ $<

clean:
	rm -rf $(BUILD)

.PHONY: all clean
