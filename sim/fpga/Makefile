# fpga-sim — Gate-level simulator for iCE40 netlists
CC = gcc
CFLAGS = -Wall -O2 -g
BUILD = build
NETLIST = ../../projects/fpga-blink/build/counter_netlist.json

SRCS = src/main.c src/netlist.c src/eval.c src/vcd.c
OBJS = $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))
HDRS = $(wildcard src/*.h)

all: $(BUILD)/fpga-sim

$(BUILD)/fpga-sim: $(OBJS)
	$(CC) -o $@ $^

$(BUILD)/%.o: src/%.c $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -Isrc -c -o $@ $<

test: $(BUILD)/fpga-sim
	@echo "=== Test 1: counter increments (led_b=1 after 2.2M cycles) ==="
	$(BUILD)/fpga-sim $(NETLIST) --cycles 2200000 --vcd $(BUILD)/counter.vcd \
		--assert led_b=1 --assert led_r=0 --assert led_g=0
	@echo ""
	@echo "=== Test 2: counter too early (led_b=0 after 100 cycles) ==="
	$(BUILD)/fpga-sim $(NETLIST) --cycles 100 --vcd /dev/null \
		--assert led_b=0 --assert led_r=0 --assert led_g=0
	@echo ""
	@echo "=== Test 3: auto-detect module (no --module flag) ==="
	$(BUILD)/fpga-sim $(NETLIST) --cycles 10 --vcd /dev/null
	@echo ""
	@echo "All tests passed."

clean:
	rm -rf $(BUILD)

.PHONY: all test clean
