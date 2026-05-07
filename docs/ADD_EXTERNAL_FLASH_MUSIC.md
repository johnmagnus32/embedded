Add W25Q128 external SPI NOR flash for storing multiple songs. The gameboy currently stores ~10s of audio in internal flash. With 16MB external flash, we can store ~6 minutes of uncompressed 22050Hz 16-bit mono audio, or ~25 minutes with IMA ADPCM compression.

Work spans three areas:
- `sim/mcu/` — W25Q128 device model + simulator tests
- `rtos/` — flash driver (already exists for W25Q128, verify JEDEC ID handling) + audio streaming layer + OS tests
- `projects/gameboy/` — integrate streaming playback into the game

## Hardware wiring (gameboy board example)

The W25Q128 connects to any available SPI bus. The specific bus assignment is a **board-level decision**, not a driver or device model concern.

### Bus assignment constraint

**SPI1 is used by the ILI9341 display. SPI2 is used for I2S audio output to the MAX98357A.** The flash goes on **SPI3** (dedicated, no sharing).

Gameboy board wiring (flash on SPI3):

```
STM32 PC10 (SPI3_SCK)  → W25Q128 CLK
STM32 PC12 (SPI3_MOSI) → W25Q128 DI
STM32 PC11 (SPI3_MISO) → W25Q128 DO
STM32 PB0  (GPIO CS)   → W25Q128 /CS
3.3V                    → VCC, /HOLD, /WP
GND                     → GND
```

A different board could put the flash on a different SPI bus — only the device tree / machine config changes. The driver and device model are bus-agnostic.

### Abstraction layers

```
OS driver (flash_w25q.c):
  - Knows W25Q protocol (commands, timing)
  - Does NOT know which SPI bus — gets it from device tree
  - Calls generic spi_write/spi_read API

Simulator device model (w25q128.c):
  - Implements spi_bus_transfer callback (same as ili9341)
  - Does NOT know which SPI peripheral — attached by the machine init
  - Machine config (gameboy.c, blink.c, etc.) wires it to a specific SPI bus

Board DTS (gameboy):
  &spi2 { flash0: w25q128@0 { ... }; };

Board DTS (hypothetical other board):
  &spi1 { flash0: w25q128@0 { ... }; };
```

No code changes needed to move the flash to a different bus — only config.

## Design constraints and edge cases

### 1. SPI bus isolation

The flash is on SPI3 (dedicated). SPI1 drives the display, SPI2 drives I2S audio. No bus sharing, no CS coordination, no timing conflicts between peripherals.

The simulator SoC model needs a third SPI peripheral added (`soc.spis[2]` at 0x40003C00, which is the real STM32F411 SPI3 base address).

### 2. UART assignment for upload mode

The gameboy uses **USART2 (PA2/PA3)** for both debug output and flash upload. This is the same UART — no conflict because upload mode replaces all normal operation (the game loop is frozen, no debug prints are happening). The baud rate is switched to 921600 on entry to upload mode and doesn't need to be restored (the chip reboots after upload).

If a future board uses separate UARTs for debug and upload, the upload task just references a different UART device from the device tree.

### 3. W25Q128 addressing

The W25Q128 uses 3-byte addresses (24-bit), which can address up to 16MB — exactly the chip's capacity. No special address mode configuration needed. All commands (read, write, erase) use the standard 3-byte address format.

If upgrading to W25Q256 in the future, the driver would need to enter 4-byte address mode (command 0xB7) during init for addresses above 16MB. The driver should branch based on the capacity byte from JEDEC ID (0x18 = 128Mbit/16MB, 0x19 = 256Mbit/32MB).

### 4. Page program boundary handling

W25Q page program (0x02) wraps within a 256-byte page — writing past a page boundary wraps to the beginning of the same page, corrupting data. The driver **must** split writes at page boundaries.

The existing `flash_w25q.c` driver already handles this:
```c
while (len > 0) {
    size_t page_remain = 256 - (offset & 0xFF);
    size_t chunk = len < page_remain ? len : page_remain;
    // ... program chunk ...
}
```

The OS test `test_flash_w25q` deliberately writes across a page boundary to verify this logic works correctly.

### 5. Audio buffer timing

At 22050 Hz mono 16-bit, one 512-sample buffer plays for 23ms. The flash read to fill the next buffer must complete within that window.

Reading 1024 bytes (512 samples × 2 bytes) from flash on SPI3:
- At 8 MHz SPI (16MHz HSI, /2 prescaler): 1024 × 8 / 8M = 1.02ms ✓ (22x margin)
- At 25 MHz SPI (100MHz PLL, /4 prescaler): 1024 × 8 / 25M = 0.33ms ✓ (70x margin)

Since flash is on a dedicated bus (SPI3), reads can happen at any time without waiting for display or audio transfers to complete.

## Part 1: Simulator — W25Q128 device model

### New files

```
sim/mcu/src/devices/w25q128.c    — SPI slave device model
sim/mcu/src/devices/w25q128.h    — header
```

### Behavior

The W25Q128 model attaches to an SPI bus (same pattern as ili9341_transfer). It maintains:
- A backing store: 16MB malloc'd buffer (or mmap'd file for persistence)
- Command state machine: idle → command byte → address bytes → data phase
- Supported commands:
  - `0x9F` JEDEC ID → returns `0xEF 0x40 0x18` (Winbond, 16MB)
  - `0x03` Read Data → 3-byte address, then streams data bytes
  - `0x0B` Fast Read → 3-byte address + 1 dummy byte, then streams data
  - `0x06` Write Enable → sets WEL latch
  - `0x02` Page Program → 3-byte address, then accepts up to 256 bytes
  - `0x20` Sector Erase (4KB) → 3-byte address, sets busy for N cycles
  - `0x05` Read Status Register → returns busy/WEL bits

Busy timing: sector erase takes ~45ms on real hardware. In the simulator, model this as a cycle count (e.g., 720,000 cycles at 16MHz). During busy, all commands except Read Status return 0xFF.

### Wiring into the gameboy machine

In `mcu-sim/mcu/src/machine/gameboy.c`:
- Attach W25Q128 to `soc.spis[2].bus` (SPI3) as a slave device
- Load song data from a file specified via `--device flash0=songs.bin`
- If no file specified, the flash is all 0xFF (erased state)

Requires adding SPI3 to the SoC model (`mcu-sim/mcu/src/soc/stm32f411.c`):
- Increase `STM32F411_NUM_SPIS` from 2 to 3
- Init `soc->spis[2]` and register at 0x40003C00 (real STM32F411 SPI3 base)

### CLI change

```
sim-core --machine gameboy --firmware game.elf --device flash0=songs.bin [...]
```

`--device name=file` is a generic mechanism for providing backing storage to any device that needs it. The machine init looks up the device name and passes the file path to the corresponding model. This extends naturally to future devices (SD cards, EEPROMs, etc.) without adding new flags.

### Simulator tests

Location: `mcu-sim/mcu/tests/firmware/func/hw/stm32/`

**test_w25q_jedec.c** — Functional: read JEDEC ID via SPI3
```
- Init SPI3 (PC10/PC11/PC12, flash CS on PB0)
- Send 0x9F command
- Read 3 bytes
- CHECK(id[0] == 0xEF && id[1] == 0x40 && id[2] == 0x18)
```

**test_w25q_read.c** — Functional: write known data, read it back
```
- Pre-load flash image with known pattern (0xDE 0xAD 0xBE 0xEF at offset 0x1000)
- Send Read command (0x03) with address 0x001000
- Read 4 bytes
- CHECK(data matches expected pattern)
```

**test_w25q_write.c** — Functional: page program and read back
```
- Send Write Enable (0x06)
- Send Page Program (0x02) at address 0x2000 with 16 bytes of test data
- Wait for busy to clear (poll status register)
- Send Read (0x03) at address 0x2000
- CHECK(read data matches written data)
```

**test_w25q_erase.c** — Functional: sector erase sets bytes to 0xFF
```
- Pre-load flash with non-0xFF data at offset 0x3000
- Send Write Enable
- Send Sector Erase (0x20) at address 0x3000
- Wait for busy to clear
- Read 16 bytes at 0x3000
- CHECK(all bytes == 0xFF)
```

Location: `mcu-sim/mcu/tests/firmware/perf/hw/stm32/`

**test_w25q_read_throughput.c** — Performance: measure sequential read speed
```
- Read 4096 bytes from flash using Fast Read (0x0B)
- Measure cycles taken
- Report bytes/sec
- CHECK_RANGE: expect >= 500 KB/s at SPI2 /2 prescaler
```

**test_w25q_dma_read.c** — Performance: DMA-driven flash read
```
- Configure DMA1 Stream 3 for SPI2 RX
- Trigger a 4096-byte DMA read from flash
- Measure cycles from DMA start to transfer complete interrupt
- Report throughput
- CHECK_RANGE: expect >= 800 KB/s (DMA eliminates per-byte CPU overhead)
```

## Part 2: OS — flash driver + audio streaming

### OS flash driver update

File: `rtos/drivers/flash/flash_w25q.c`

The existing driver already targets W25Q128 (JEDEC ID `0xEF 0x40 0x18`). Changes needed:
- Verify the existing `flash_nor_init` correctly identifies the W25Q128
- No address mode changes needed (W25Q128 uses 3-byte addresses for all 16MB)
- Confirm `flash_nor_config.size` is set to 16MB

### New: audio streaming layer

File: `rtos/drivers/audio/audio_stream.c` and `rtos/include/drivers/audio_stream.h`

This layer sits between the flash driver and the audio driver, providing double-buffered streaming:

```c
struct audio_stream {
    const struct device *flash;
    const struct device *audio;
    uint32_t song_offset;       /* start address in flash */
    uint32_t song_length;       /* total bytes */
    uint32_t position;          /* current playback position */
    int16_t  buf[2][STREAM_BUF_SAMPLES];  /* double buffer */
    int      active_buf;
    int      playing;
    int      loop;
};

/* API */
int  audio_stream_init(struct audio_stream *s, const struct device *flash, const struct device *audio);
int  audio_stream_play(struct audio_stream *s, uint32_t offset, uint32_t length, int loop);
void audio_stream_stop(struct audio_stream *s);
int  audio_stream_is_playing(struct audio_stream *s);
```

The `audio_fill_fn` callback (called from I2S DMA interrupt) swaps buffers. A background task (or the main loop) pre-fills the next buffer from flash:

```
ISR (DMA half-complete):
    swap active_buf
    signal "fill needed"

Background task:
    wait for "fill needed"
    flash_read(flash, song_offset + position, buf[!active_buf], BUF_SIZE)
    position += BUF_SIZE
    if position >= song_length:
        if loop: position = 0
        else: stop
```

### Song table format

A simple header at flash offset 0x000000:

```c
struct song_table_header {
    uint32_t magic;         /* 0x534F4E47 = "SONG" */
    uint32_t num_songs;
    struct song_entry songs[];
};

struct song_entry {
    uint32_t offset;        /* byte offset in flash */
    uint32_t length;        /* length in bytes */
    uint32_t sample_rate;   /* e.g., 22050 */
    uint8_t  channels;      /* 1 = mono, 2 = stereo */
    uint8_t  bits;          /* 16 */
    char     name[22];      /* null-terminated name */
};
```

### OS tests

Location: `rtos/tests/src/`

**test_flash_w25q.c** — Functional: verify flash driver API works through the OS abstraction
```
void test_flash_run(void) {
    TEST_SUITE("flash_w25q");

    const struct device *flash = DEVICE_DT_GET(flash0);

    // Test 1: read JEDEC ID (implicit in init — device should be ready)
    TEST_ASSERT(flash != NULL, "flash device should exist");

    // Test 2: read known data from pre-loaded flash image
    uint8_t buf[16];
    flash_read(flash, 0x1000, buf, 16);
    TEST_ASSERT_EQ(buf[0], 0xDE, "first byte should match");
    TEST_ASSERT_EQ(buf[1], 0xAD, "second byte should match");

    // Test 3: erase + write + read back
    flash_erase(flash, 0x10000, 4096);
    uint8_t pattern[4] = {0xCA, 0xFE, 0xBA, 0xBE};
    flash_write(flash, 0x10000, pattern, 4);
    uint8_t verify[4];
    flash_read(flash, 0x10000, verify, 4);
    TEST_ASSERT_EQ(verify[0], 0xCA, "write/read byte 0");
    TEST_ASSERT_EQ(verify[3], 0xBE, "write/read byte 3");

    // Test 4: page boundary crossing
    uint8_t big[512];
    for (int i = 0; i < 512; i++) big[i] = i & 0xFF;
    flash_erase(flash, 0x20000, 4096);
    flash_write(flash, 0x200F0, big, 512);  // crosses page boundary at 0x20100
    uint8_t readback[512];
    flash_read(flash, 0x200F0, readback, 512);
    TEST_ASSERT_EQ(readback[0], 0xF0, "page-cross byte 0");
    TEST_ASSERT_EQ(readback[16], 0x00, "page-cross byte 16 (after boundary)");
}
```

**test_audio_stream.c** — Functional: verify streaming playback logic
```
void test_audio_stream_run(void) {
    TEST_SUITE("audio_stream");

    const struct device *flash = DEVICE_DT_GET(flash0);
    const struct device *audio = DEVICE_DT_GET(i2s0);
    struct audio_stream stream;

    // Test 1: init
    int ret = audio_stream_init(&stream, flash, audio);
    TEST_ASSERT_EQ(ret, 0, "stream init should succeed");

    // Test 2: play starts playback
    audio_stream_play(&stream, 0x1000, 44100, 0);  // 1 second of audio
    TEST_ASSERT(audio_stream_is_playing(&stream), "should be playing");

    // Test 3: playback completes (non-looping)
    // Simulate enough ticks for 1 second of audio at 22050 Hz
    // (in simulator, run until stream reports done)
    sched_sleep_ms(1200);
    TEST_ASSERT(!audio_stream_is_playing(&stream), "should stop after song ends");

    // Test 4: looping mode
    audio_stream_play(&stream, 0x1000, 44100, 1);  // loop=1
    sched_sleep_ms(2500);  // > 1 second, should still be playing (looped)
    TEST_ASSERT(audio_stream_is_playing(&stream), "should still be playing (looped)");
    audio_stream_stop(&stream);
    TEST_ASSERT(!audio_stream_is_playing(&stream), "should stop after explicit stop");
}
```

**test_audio_stream_perf.c** — Performance: measure streaming overhead
```
void test_audio_stream_perf_run(void) {
    TEST_SUITE("audio_stream_perf");

    const struct device *flash = DEVICE_DT_GET(flash0);
    const struct device *audio = DEVICE_DT_GET(i2s0);
    struct audio_stream stream;
    audio_stream_init(&stream, flash, audio);

    // Test 1: buffer fill time — how long does flash_read take for one buffer?
    uint32_t t0 = cycles();
    uint8_t tmp[1024];
    flash_read(flash, 0x1000, tmp, 1024);
    uint32_t t1 = cycles();
    uint32_t fill_cycles = t1 - t0;
    // At 22050 Hz, 512 samples = 23ms of audio = 368,000 cycles at 16MHz
    // Flash read of 1024 bytes should take << 368,000 cycles
    TEST_ASSERT(fill_cycles < 100000, "buffer fill should be fast relative to playback");

    // Test 2: CPU utilization during streaming
    // Start playback, measure how many cycles the main loop gets
    audio_stream_play(&stream, 0x1000, 441000, 1);  // 10 seconds, looping
    uint32_t idle_start = cycles();
    for (int i = 0; i < 1000; i++) {
        // Simulate main loop work
        volatile int x = i * i;
        (void)x;
    }
    uint32_t idle_end = cycles();
    audio_stream_stop(&stream);
    // Main loop should still get CPU time (streaming shouldn't starve it)
    TEST_ASSERT((idle_end - idle_start) < 50000, "main loop should not be starved");
}
```

## Part 3: Gameboy integration

### Device tree update

In the gameboy board DTS, add the W25Q128 on SPI3:

```dts
&spi3 {
    status = "okay";
    flash0: w25q128@0 {
        compatible = "jedec,spi-nor";
        reg = <0>;
        jedec-id = [EF 40 18];
        size = <0x8000000>;  /* 128 Mbit = 16 MB */
        page-size = <256>;
        sector-size = <4096>;
    };
};
```

### Song management

New file: `projects/gameboy/src/music.c`

```c
static struct audio_stream music_stream;
static struct song_entry songs[MAX_SONGS];
static int num_songs;

void music_init(void) {
    const struct device *flash = DEVICE_DT_GET(flash0);
    const struct device *audio = DEVICE_DT_GET(i2s0);
    audio_stream_init(&music_stream, flash, audio);

    // Read song table from flash offset 0
    struct song_table_header hdr;
    flash_read(flash, 0, &hdr, sizeof(hdr));
    if (hdr.magic == 0x534F4E47) {
        num_songs = hdr.num_songs;
        flash_read(flash, sizeof(hdr), songs, num_songs * sizeof(struct song_entry));
    }
}

void music_play(int song_index, int loop) {
    if (song_index < num_songs)
        audio_stream_play(&music_stream, songs[song_index].offset,
                          songs[song_index].length, loop);
}

void music_stop(void) { audio_stream_stop(&music_stream); }
```

### Song upload tool

New file: `tools/flash_songs.py`

A Python script that:
1. Takes a list of WAV files as input
2. Converts each to raw 22050Hz 16-bit mono PCM (using ffmpeg or built-in)
3. Builds the song table header + concatenated PCM data
4. Writes the result to `songs.bin` (for the simulator's `--device flash0=songs.bin`)
5. Optionally programs it to real hardware via the in-firmware upload mode

```bash
# Generate songs.bin for simulator
python3 tools/flash_songs.py --output songs.bin \
    overworld.wav battle.wav dungeon.wav gameover.wav victory.wav

# Program to real hardware (triggers upload mode over UART)
python3 tools/flash_songs.py --program /dev/ttyUSB0 \
    overworld.wav battle.wav dungeon.wav gameover.wav victory.wav
```

### In-firmware flash upload mode (for real hardware)

No separate programmer firmware. The gameboy firmware itself handles flash uploads via a high-priority task that blocks all other tasks when triggered.

**How it works:**

The firmware runs a low-overhead UART listener task. When the host sends a magic string (`"FLASH\n"`), the task enters upload mode. Because it never yields to the scheduler (no `sched_sleep` or `sched_yield` calls), no other task gets CPU time — the game, audio, and display are effectively frozen.

```
Host PC (tools/flash_songs.py)
    │
    │ UART (921600 baud for speed)
    ▼
STM32 running gameboy firmware (upload mode active)
    │
    │ SPI2
    ▼
W25Q128 external flash
```

**Firmware changes:**

New file: `projects/gameboy/src/flash_upload.c`

```c
void flash_upload_mode(void)
{
    // 1. Stop audio — critical: prevents DMA reads from flash during erase/write
    audio_stop(audio_dev);

    // 2. Signal ready to host
    uart_puts("READY\n");

    // 3. Receive total size (4 bytes, little-endian)
    uint32_t total_size = uart_read_u32();

    // 4. Erase needed sectors
    uint32_t sectors = (total_size + 4095) / 4096;
    for (uint32_t i = 0; i < sectors; i++) {
        flash_erase(flash_dev, i * 4096, 4096);
        uart_putc('.');  // progress to host
    }
    uart_puts("\nWRITING\n");

    // 5. Receive and write data in 256-byte pages
    uint8_t page_buf[256];
    uint32_t offset = 0;
    while (offset < total_size) {
        uint32_t chunk = (total_size - offset) > 256 ? 256 : (total_size - offset);
        uart_read_exact(page_buf, chunk);
        flash_write(flash_dev, offset, page_buf, chunk);
        offset += chunk;
        uart_putc('#');  // per-page progress
    }

    // 6. Verify via CRC
    uart_puts("\nVERIFY\n");
    offset = 0;
    while (offset < total_size) {
        uint8_t verify_buf[256];
        uint32_t chunk = (total_size - offset) > 256 ? 256 : (total_size - offset);
        flash_read(flash_dev, offset, verify_buf, chunk);
        uint16_t crc = crc16(verify_buf, chunk);
        uart_write_u16(crc);  // host compares against its own CRC
        offset += chunk;
    }

    // 7. Reboot — game restarts with new songs
    uart_puts("\nDONE\n");
    NVIC_SystemReset();
}
```

**UART listener task (runs during normal gameplay):**

```c
// Created at highest priority so it preempts everything when upload starts
void flash_upload_task(void)
{
    while (1) {
        if (uart_has_line()) {
            char buf[16];
            uart_read_line(buf, sizeof(buf));
            if (strcmp(buf, "FLASH") == 0) {
                // This call never returns — blocks until upload done, then reboots
                flash_upload_mode();
            }
        }
        sched_sleep_ms(100);  // yield to game tasks between checks
    }
}

// In game init:
sched_create_task(flash_upload_task, "flash_upload", PRIORITY_HIGHEST);
```

**Why this works without explicitly stopping the game:**

- `flash_upload_mode()` never calls `sched_sleep` or `sched_yield`
- The scheduler only switches tasks at yield points or timer preemption
- Since the upload task is highest priority and never yields, no other task runs
- Audio is explicitly stopped because its DMA interrupt would fire regardless of task priority and could read flash during erase (the one thing that MUST be stopped)
- After upload completes, `NVIC_SystemReset()` reboots cleanly — no state to restore

**Baud rate consideration:**

At 115200 baud: ~11.5 KB/s → 13MB takes ~19 minutes (too slow).
At 921600 baud: ~92 KB/s → 13MB takes ~2.4 minutes (acceptable).

Configure USART2 for 921600 during upload mode (or always). The STM32F411 supports this at 16MHz HSI.

**Host-side script behavior:**

The `tools/flash_songs.py` script:
1. Opens serial port at 921600 baud
2. Sends `"FLASH\n"` and waits for `"READY\n"`
3. Sends 4-byte total size
4. Waits for erase progress dots, then `"WRITING\n"`
5. Sends data in 256-byte pages, waits for `'#'` ack per page
6. Reads back CRC per page and compares against local CRC
7. Waits for `"DONE\n"`
8. Prints summary: `"Uploaded 5 songs (12.8 MB) in 2m18s, verified OK"`

## Verification checklist

### Simulator tests (run with `make test` from `sim/mcu/`)

| Test | Type | What it verifies |
|------|------|-----------------|
| test_w25q_jedec | Functional | Device model responds with correct JEDEC ID |
| test_w25q_read | Functional | Pre-loaded data can be read back correctly |
| test_w25q_write | Functional | Page program stores data persistently |
| test_w25q_erase | Functional | Sector erase sets all bytes to 0xFF |
| test_w25q_read_throughput | Performance | Sequential read meets bandwidth target |
| test_w25q_dma_read | Performance | DMA read meets bandwidth target |

### OS tests (run with `make test` from `rtos/tests/`)

| Test | Type | What it verifies |
|------|------|-----------------|
| test_flash_w25q | Functional | OS flash API (read/write/erase) works through driver abstraction |
| test_audio_stream | Functional | Streaming playback starts, stops, loops correctly |
| test_audio_stream_perf | Performance | Buffer fill is fast enough, CPU isn't starved |

### Integration test (manual or scripted)

1. Build songs.bin: `python3 tools/flash_songs.py --output songs.bin overworld.wav battle.wav`
2. Run in simulator: `./sim-core --machine gameboy --firmware gameboy.elf --device flash0=songs.bin --chardev audio=9005`
3. Connect to audio chardev and verify PCM output matches expected song data
4. Verify song switching: send command to change songs mid-playback, confirm audio output changes
5. Verify looping: let a song play past its end, confirm it restarts seamlessly (no gap/click)

### Performance targets

| Metric | Target | Rationale |
|--------|--------|-----------|
| Flash read throughput | ≥ 500 KB/s | 22050 Hz × 2 bytes = 44 KB/s needed; 10x headroom |
| Buffer fill time | < 5ms | Must complete before current buffer finishes playing (23ms for 512 samples) |
| CPU overhead during streaming | < 10% | Main game loop needs 90%+ of CPU for rendering |
| Audio gap on song switch | < 10ms | Imperceptible to human ear |
| Song start latency | < 50ms | Responsive to game events |
