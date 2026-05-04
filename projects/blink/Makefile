CC      = arm-none-eabi-gcc
CFLAGS  = -mcpu=cortex-m4 -mthumb -nostdlib -ffreestanding -Wall -Og -g
LDFLAGS = -T linker.ld -nostdlib

BUILD   = build
TARGET  = $(BUILD)/blink.elf

all: $(TARGET)

$(TARGET): main.c linker.ld
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ main.c

flash: $(TARGET)
	openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
		-c "program $(TARGET) verify reset exit"

clean:
	rm -rf $(BUILD)

.PHONY: all flash clean
