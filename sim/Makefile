CC = gcc
CFLAGS = -Wall -O2 -g

sim: main.o cpu.o mem.o
	$(CC) -o $@ $^

%.o: %.c cpu.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f sim *.o

# Run with the STM32 sample firmware
run: sim
	./sim ../sample-stm32f411re/build/hello.bin

# Run with FIFO (open another terminal: cat /tmp/uart)
run-fifo: sim
	./sim ../sample-stm32f411re/build/hello.bin --fifo /tmp/uart

.PHONY: clean run run-fifo
