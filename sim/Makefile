CC = gcc
CFLAGS = -Wall -O2 -g

sim-core: main.o cpu.o mem.o vis.o elf_sym.o state.o
	$(CC) -o $@ $^

%.o: %.c cpu.h vis.h elf_sym.h state.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f sim *.o

# Run with the STM32 sample firmware
run: sim
	./sim ../sample-stm32f411re/build/hello.bin

# Run with FIFO (open another terminal: cat /tmp/uart)
run-fifo: sim
	./sim ../sample-stm32f411re/build/hello.bin --fifo /tmp/uart

# Run with visualization (UART to stdout, vis to stderr)
# Use: make run-vis 2>/dev/tty  (vis on current terminal)
# Or:  make run-vis 2>/tmp/vis.txt  (vis to file, tail -f /tmp/vis.txt)
run-vis: sim
	./sim ../sample-stm32f411re/build/hello.bin --vis

.PHONY: clean run run-fifo run-vis
