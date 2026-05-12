# Gameboy — STM32F411RE Jump Game

A side-scrolling jump game running on the STM32F411RE with an ILI9341 display, I2S audio, and button input.

## Pin Assignments (Nucleo-F411RE)

| Pin | Function | Peripheral | Notes |
|-----|----------|-----------|-------|
| PA4 | CS | SPI1 / ILI9341 Display | |
| PA5 | SCK | SPI1 / ILI9341 Display | |
| PA6 | MISO | SPI1 / ILI9341 Display | |
| PA7 | MOSI | SPI1 / ILI9341 Display | |
| PA9 | TX | USART1 (Debug UART) | |
| PA10 | RX | USART1 (Flash upload) | |
| PB0 | CS | W25Q128 Flash | |
| PB5 | DC | ILI9341 Display | |
| PC0 | Button A | Jump | |
| PC1 | Button B | SFX | |
| PC2 | Button Left | — | |
| PC3 | Button Right | — | |
| PC4 | Button Up | — | |
| PC5 | Button Down | — | |
| PC6 | Button Start | — | |
| PC7 | Button Select | — | |
| PB12 | WS (LRCK) | I2S2 (Audio) | |
| PB13 | SCK | I2S2 (Audio) | |
| PB15 | SD | I2S2 (Audio) | |
| PC10 | SCK | SPI3 (Flash) | Morpho header only |
| PC11 | MISO | SPI3 (Flash) | Morpho header only |
| PC12 | MOSI | SPI3 (Flash) | Morpho header only |

**Avoided pins:** PB8/PB9 have I2C pull-ups, PB12 is I2S2_WS on the Nucleo board — not suitable for pull-down buttons.

## ILI9341 Display Wiring

| Display Pin | Connect To |
|-------------|-----------|
| VIN | 5V |
| GND | GND |
| CS | PA4 |
| RESET | 3.3V (tied high) |
| DC | PB5 |
| MOSI (SDA) | PA7 |
| SCK | PA5 |
| LED (LITE) | 3.3V |
| MISO | PA6 |

**Important:** Solder bridge IM1, IM2, IM3 on the back of the display breakout for SPI mode. IM0 stays open.

## Buttons

Buttons connect between the GPIO pin and **3.3V**. Internal pull-down is enabled; rising edge (press down) triggers the interrupt.

| Button | Pin | Function |
|--------|-----|----------|
| A | PC0 | Jump |
| B | PC1 | SFX |
| Left | PC2 | — |
| Right | PC3 | — |
| Up | PC4 | — |
| Down | PC5 | — |
| Start | PC6 | — |
| Select | PC7 | — |

## UART Debug

115200 baud on PA9 (USART1 TX). Connect to a USB-UART adapter's RX pin.

## SPI Clock

SPI1 runs at 8 MHz (APB2 16 MHz / 2 prescaler). ILI9341 max is 10 MHz.

## Building

```bash
make
```

## Flashing (via ST-Link V2 + OpenOCD)

```bash
openocd -f interface/stlink-v2.cfg -f target/stm32f4x.cfg \
  -c "program build/gameboy.elf verify reset exit"
```

## Running in Simulator

```bash
cd ../../sim/mcu
./sim ../../projects/gameboy/build/gameboy.elf ../../projects/gameboy/build/songs.bin
```

Open http://localhost:3000 in a browser.

## Audio

Background music streams from the W25Q128 external flash over SPI3. The jump sound effect is synthesized in firmware.

### Converting audio

Place a WAV file at `assets/music.wav` and run:

```bash
make audio    # → build/songs.bin (8-bit unsigned PCM, 22050 Hz mono)
```

### Programming audio to flash

The firmware includes an upload mode triggered over UART. Requires `pyserial`:

```bash
pip install pyserial
python3 tools/flash_upload.py /dev/ttyUSB0 build/songs.bin
```

This takes ~5 minutes at 115200 baud for a 2.6MB file. The tool:
1. Sends `FLASH\n` to trigger upload mode
2. Firmware stops audio and responds `READY\n`
3. Tool sends 4-byte size + data in 256-byte pages
4. Firmware erases sectors and programs pages
5. Firmware reboots when done

### W25Q128 Flash Wiring

| Flash Pin | Connect To |
|-----------|-----------|
| CLK | PC10 (SPI3 SCK) |
| DI (MOSI) | PC12 (SPI3 MOSI) |
| DO (MISO) | PC11 (SPI3 MISO) |
| /CS | PB0 |
| VCC | 3.3V |
| /HOLD | 3.3V |
| /WP | 3.3V |
| GND | GND |
