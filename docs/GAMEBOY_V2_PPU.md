Create a `gameboy-v2` project that runs the same dino game but offloads rendering to the iCE40UP5K PPU over SPI. The MCU sends sprite positions each frame; the FPGA renders pixels and drives the ILI9341 display via 8-bit parallel. The MCU no longer talks to the display directly. A single `make` produces one unified binary containing firmware + FPGA bitstream + tile assets. Work in `/home/johmagnu/learning/embedded/projects/gameboy-v2`.

## Goal

Same game as `gameboy`, same RTOS, same STM32F411 — but rendering is handled by the iCE40UP5K PPU from `fpga-ppu`. The MCU's job shrinks to: run game logic, send sprite table over SPI once per frame (30 FPS). The FPGA continuously renders scanlines and pushes pixels to the ILI9341 at 60 FPS via 8-bit parallel.

## Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│  STM32F411 (100MHz)                                                 │
│                                                                     │
│  RTOS tasks:                                                        │
│    game_task:  physics + collision → fill sprite table → SPI send   │
│    audio_task: stream music from flash                              │
│    idle_task:  WFI                                                  │
│                                                                     │
│  SPI1 (8MHz) ─────────────────────────────────────────────┐         │
│  GPIO PA4 (CS) ───────────────────────────────────────────┤         │
└───────────────────────────────────────────────────────────┼─────────┘
                                                            │
                                                            ▼
┌─────────────────────────────────────────────────────────────────────┐
│  iCE40UP5K (48MHz)                                                  │
│                                                                     │
│  SPI slave → spi_cmd → sprite_table (double-buffered)               │
│                       → tile_table                                  │
│                       → sprite_mem (SPRAM)                          │
│                                                                     │
│  pixel_gen (scanline renderer) → line buffer → lcd_driver           │
│                                                                     │
│  LCD_D[7:0], LCD_WR, LCD_DC, LCD_CS ─────────────────────┐         │
└───────────────────────────────────────────────────────────┼─────────┘
                                                            │
                                                            ▼
                                                    ┌──────────────┐
                                                    │  ILI9341     │
                                                    │  240×320     │
                                                    │  8-bit 8080  │
                                                    └──────────────┘
```

## Project structure

```
projects/gameboy-v2/
├── Makefile            ← unified build: firmware + bitstream + assets → one .bin
├── .config             ← kernel config (no CONFIG_DISPLAY_ILI9341)
├── board.dts           ← device tree (SPI1 for PPU, SPI2 for FPGA config, no display node)
├── linker.ld
├── src/
│   ├── main.c          ← RTOS setup, task creation
│   ├── game.c          ← game logic (unchanged from gameboy)
│   ├── game.h          ← game state (unchanged)
│   ├── render.c        ← NEW: sends sprite table to PPU over SPI
│   ├── render.h        ← same interface as gameboy
│   ├── ppu.c           ← NEW: PPU SPI protocol driver
│   ├── ppu.h           ← NEW: PPU API (upload tiles, send frame)
│   ├── fpga_loader.c   ← NEW: loads bitstream into iCE40 over SPI at boot
│   ├── fpga_loader.h   ← NEW
│   ├── buttons.c/h     ← unchanged
│   ├── audio.c/h       ← unchanged
│   └── flash_audio.c/h ← unchanged
├── assets/
│   ├── tiles.bin       ← sprite pixel data (RGB565, generated from PNGs)
│   ├── music.wav       ← same as gameboy
│   └── convert_tiles.py ← tool to convert PNGs → tiles.bin + tile_table
└── fpga/
    ├── Makefile         ← FPGA synthesis (reuses fpga-ppu sources)
    ├── src/ → symlink to ../../fpga-ppu/src/
    └── icebreaker.pcf   ← pin constraints
```

## What changes from gameboy

| Component | gameboy (v1) | gameboy-v2 |
|-----------|-------------|------------|
| Display driver | MCU drives ILI9341 over SPI | MCU doesn't touch display |
| Rendering | `render.c` draws rects via display driver | `render.c` fills sprite table, sends over SPI |
| Display connection | SPI1 → ILI9341 | SPI1 → iCE40UP5K → (parallel) → ILI9341 |
| Frame rate | ~30 FPS (limited by SPI pixel push) | 30 FPS game updates, 60 FPS display refresh |
| CPU usage for rendering | ~70% (pushing 153KB/frame) | ~2% (pushing ~50 bytes/frame) |
| game.c | — | Unchanged |
| audio.c | — | Unchanged |
| buttons.c | — | Unchanged |

## PPU driver (ppu.c / ppu.h)

```c
#ifndef PPU_H
#define PPU_H

#include <stdint.h>

#define PPU_MAX_SPRITES   64
#define PPU_MAX_TILES     256
#define PPU_TRANSPARENT   0xF81F

struct ppu_sprite {
    uint16_t x;
    uint8_t  y;
    uint8_t  tile;
    uint8_t  flags;
};

/* Initialize PPU (assert reset, wait, release) */
void ppu_init(void);

/* Upload tile pixel data to SPRAM (called once at boot) */
void ppu_upload_pixels(uint16_t addr, const uint8_t *data, int len);

/* Upload tile table metadata (called once at boot) */
void ppu_upload_tile_table(const uint8_t *entries, int num_tiles);

/* Set background color */
void ppu_set_bg_color(uint16_t rgb565);

/* Send sprite table for this frame (triggers render on CS deassert) */
void ppu_send_frame(const struct ppu_sprite *sprites, int count);

#endif
```

```c
/* ppu.c — SPI protocol driver for iCE40UP5K PPU */

#include "ppu.h"
#include "board.h"
#include "drivers/spi.h"
#include "drivers/gpio.h"

#define CMD_SPRITE_UPDATE  0x01
#define CMD_PIXEL_UPLOAD   0x02
#define CMD_BG_COLOR       0x03
#define CMD_TILE_TABLE     0x04

static void cs_low(void)  { gpio_pin_set(dev_gpioa, 4, 0); }
static void cs_high(void) { gpio_pin_set(dev_gpioa, 4, 1); }

void ppu_init(void)
{
    /* CS starts high (deselected) */
    cs_high();
}

void ppu_upload_pixels(uint16_t addr, const uint8_t *data, int len)
{
    uint8_t hdr[3] = { CMD_PIXEL_UPLOAD, addr >> 8, addr & 0xFF };
    cs_low();
    spi_write(spi_dev, hdr, 3);
    spi_write(spi_dev, data, len);
    cs_high();
}

void ppu_upload_tile_table(const uint8_t *entries, int num_tiles)
{
    uint8_t hdr[2] = { CMD_TILE_TABLE, num_tiles };
    cs_low();
    spi_write(spi_dev, hdr, 2);
    spi_write(spi_dev, entries, num_tiles * 4);
    cs_high();
}

void ppu_set_bg_color(uint16_t rgb565)
{
    uint8_t cmd[3] = { CMD_BG_COLOR, rgb565 >> 8, rgb565 & 0xFF };
    cs_low();
    spi_write(spi_dev, cmd, 3);
    cs_high();
}

void ppu_send_frame(const struct ppu_sprite *sprites, int count)
{
    uint8_t hdr[2] = { CMD_SPRITE_UPDATE, count };
    cs_low();
    spi_write(spi_dev, hdr, 2);
    spi_write(spi_dev, (const uint8_t *)sprites, count * sizeof(struct ppu_sprite));
    cs_high();  /* CS deassert triggers frame_valid in FPGA */
}
```

## Render layer (render.c — rewritten)

Same `render.h` interface as gameboy, but implementation sends sprites instead of drawing rects:

```c
#include "render.h"
#include "ppu.h"

/* Tile indices (assigned during asset conversion) */
enum {
    TILE_PLAYER = 0,
    TILE_OBS_0  = 1,
    TILE_OBS_1  = 2,
    TILE_OBS_2  = 3,
    TILE_DIGIT_0 = 10,  /* 10-19: digits 0-9 */
    TILE_TITLE  = 20,
    TILE_GAMEOVER = 21,
};

static struct ppu_sprite sprites[PPU_MAX_SPRITES];
static int num_sprites;

static void sprite_add(uint16_t x, uint8_t y, uint8_t tile, uint8_t flags)
{
    if (num_sprites < PPU_MAX_SPRITES)
        sprites[num_sprites++] = (struct ppu_sprite){x, y, tile, flags};
}

void render_init(void)
{
    ppu_init();

    /* Upload tile graphics from flash (done once) */
    extern const uint8_t tiles_data[];
    extern const int tiles_data_len;
    ppu_upload_pixels(0, tiles_data, tiles_data_len);

    /* Upload tile table (base_addr, width, height per tile) */
    extern const uint8_t tile_table[];
    extern const int tile_table_num;
    ppu_upload_tile_table(tile_table, tile_table_num);

    ppu_set_bg_color(0x867D);  /* sky blue */
}

void render_title(void)
{
    num_sprites = 0;
    sprite_add(80, 80, TILE_TITLE, 0);
    ppu_send_frame(sprites, num_sprites);
}

void render_game_start(void)
{
    /* Send an empty sprite table to clear any title screen sprites.
     * The FPGA shows bg_color for all non-sprite pixels, so the
     * display is clean immediately. */
    num_sprites = 0;
    ppu_send_frame(sprites, num_sprites);
}

void render_game_over(int score)
{
    num_sprites = 0;
    sprite_add(80, 60, TILE_GAMEOVER, 0);
    /* Score digits */
    int x = 160;
    if (score >= 100) { sprite_add(x, 100, TILE_DIGIT_0 + (score/100)%10, 0); x += 12; }
    if (score >= 10)  { sprite_add(x, 100, TILE_DIGIT_0 + (score/10)%10, 0); x += 12; }
    sprite_add(x, 100, TILE_DIGIT_0 + score%10, 0);
    ppu_send_frame(sprites, num_sprites);
}

void render_frame(const struct game_state *cur, const struct game_state *prev)
{
    (void)prev;  /* no dirty-rect tracking needed — PPU redraws everything */
    num_sprites = 0;

    /* Player */
    sprite_add(PLAYER_X, cur->player_y, TILE_PLAYER, 0);

    /* Obstacles */
    for (int i = 0; i < MAX_OBS; i++) {
        if (cur->obs_x[i] >= 0 && cur->obs_x[i] < SCR_W)
            sprite_add(cur->obs_x[i], GROUND_Y - cur->obs_gap[i], TILE_OBS_0, 0);
    }

    /* Score digits */
    int score = cur->score;
    int x = SCR_W - 48;
    if (score >= 100) { sprite_add(x, 4, TILE_DIGIT_0 + (score/100)%10, 0); x += 12; }
    if (score >= 10)  { sprite_add(x, 4, TILE_DIGIT_0 + (score/10)%10, 0); x += 12; }
    sprite_add(x, 4, TILE_DIGIT_0 + score%10, 0);

    /* Ground line (could be a wide sprite or background layer) */
    sprite_add(0, GROUND_Y, TILE_GROUND, 0);

    ppu_send_frame(sprites, num_sprites);
}
```

## SPI bandwidth

Per frame at 30 FPS with ~10 sprites:
- Header: 2 bytes
- Sprites: 10 × 5 = 50 bytes
- Total: 52 bytes per frame

At 8MHz SPI: 52 bytes × 8 bits / 8,000,000 = **52µs per frame**

That's 0.16% of the 33ms frame budget. The MCU is essentially idle after game logic.

## Boot sequence

```
1. MCU boots, inits RTOS
2. game_task starts:
   a. fpga_load_bitstream() — load PPU bitstream into iCE40 over SPI (~4ms)
      (bitstream is embedded in firmware flash as const array)
   b. ppu_init() — CS high, FPGA now running PPU logic
   c. ppu_upload_pixels() — stream tile graphics → FPGA SPRAM (~8ms)
      (tile data is also embedded in firmware flash)
   d. ppu_upload_tile_table() — send tile metadata
   e. ppu_set_bg_color(SKY)
   f. Enter game loop: game_update() → render_frame() → sleep
3. FPGA continuously renders at 60 FPS from its sprite table
   (first few frames before upload completes will show background only)
```

Total boot time from power-on to first game frame: ~15ms (bitstream + tile upload).

## Hardware connections

```
STM32F411          iCE40UP5K          ILI9341
──────────         ─────────          ───────
PA5 (SPI1_SCK)  → SPI_CLK (game data, mode 0, MSB first)
PA6 (SPI1_MISO) ← SPI_MISO
PA7 (SPI1_MOSI) → SPI_MOSI
PA4 (GPIO CS)   → SPI_CS
PC10 (SPI3_SCK) → SPI_CONFIG_CLK (bitstream load, mode 0, MSB first)
PC12 (SPI3_MOSI)→ SPI_CONFIG_DATA
PB1 (GPIO)      → CRESET_B (active-low reset)
PB2 (GPIO in)   → CDONE (config done indicator)
PB6 (GPIO)      → SPI_SS_B (config chip select)
                   LCD_D[7:0]      → D0-D7
                   LCD_WR          → WR
                   LCD_DC          → DC
                   LCD_CS          → CS (active low)
```

Total FPGA pins used: 4 (SPI game data) + 3 (SPI config) + 11 (LCD out) = 18 of 39 available.

Notes:
- Both SPI buses use mode 0 (CPOL=0, CPHA=0), MSB first.
- CRESET_B, CDONE, and SPI_SS_B are plain GPIOs, not SPI alternate function pins. The iCE40 config protocol requires dedicated GPIO control of these signals.
- SPI3 is used for bitstream loading (not SPI2, which is used for I2S audio). Config only happens once at boot, before audio starts.
- MISO is connected for future use (e.g., reading PPU status, verifying tile uploads, frame sync handshake). The current PPU Verilog doesn't drive it — it would need a `spi_miso` output added to `spi_cmd.v` when readback is implemented.

## Unified binary image

A single `make` produces one flashable image containing everything:

```
┌─────────────────────────────────────────────────────┐
│  gameboy-v2.bin                                     │
│                                                     │
│  ┌───────────────────────────────────────────────┐  │
│  │ STM32 firmware (.text + .data + .rodata)      │  │
│  │ Includes:                                     │  │
│  │   - RTOS + drivers + game logic               │  │
│  │   - fpga_bitstream[] (const array, ~32KB)     │  │
│  │   - tiles_data[] (const array, ~8KB)          │  │
│  │   - tile_table[] (const array, ~1KB)          │  │
│  └───────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────┘
```

The FPGA bitstream and tile assets are embedded directly in the firmware as const arrays in flash. No separate files to manage at runtime.

### Build process

```makefile
# Top-level Makefile (simplified)

all: build/gameboy-v2.bin

# Step 1: Synthesize FPGA bitstream
build/ppu.bin: fpga/src/*.v fpga/icebreaker.pcf
	$(MAKE) -C fpga
	cp fpga/build/ppu_top.bin $@

# Step 2: Convert bitstream to C array
build/fpga_bitstream.c: build/ppu.bin
	xxd -i $< | sed 's/build_ppu_bin/fpga_bitstream/g' > $@

# Step 3: Convert tile assets to C arrays
build/tiles_data.c build/tile_table.c: assets/tiles.bin assets/tile_table.bin
	xxd -i assets/tiles.bin | sed 's/assets_tiles_bin/tiles_data/g' > build/tiles_data.c
	xxd -i assets/tile_table.bin | sed 's/assets_tile_table_bin/tile_table/g' > build/tile_table.c

# Step 4: Compile everything together
SRCS += build/fpga_bitstream.c build/tiles_data.c build/tile_table.c
# ... normal firmware compile + link ...

build/gameboy-v2.bin: build/gameboy-v2.elf
	arm-none-eabi-objcopy -O binary $< $@
```

One `make` invocation: synthesizes the bitstream, converts assets, compiles firmware, links everything into a single ELF/BIN. Flash one file to the STM32 and the system is complete.

## FPGA bitstream loading (fpga_loader.c)

The STM32 loads the bitstream into the iCE40UP5K at boot using the iCE40 SPI slave configuration protocol:

```c
#ifndef FPGA_LOADER_H
#define FPGA_LOADER_H

/* Load bitstream into iCE40UP5K. Returns 0 on success, -1 on failure.
 * Must be called before ppu_init(). */
int fpga_load_bitstream(void);

#endif
```

```c
/* fpga_loader.c — iCE40 SPI slave configuration */

#include "fpga_loader.h"
#include "board.h"
#include "drivers/spi.h"
#include "drivers/gpio.h"

/* Bitstream embedded in flash by the build system */
extern const uint8_t fpga_bitstream[];
extern const unsigned int fpga_bitstream_len;

#define CRESET_PIN  1   /* PB1 — active low reset */
#define CDONE_PIN   2   /* PB2 — goes high when config done */
#define SS_PIN      6   /* PB6 — SPI slave select for config */

static void delay_us(int us)
{
    /* Busy-wait ~us microseconds at 100MHz */
    volatile int n = us * 25;
    while (n--) {}
}

int fpga_load_bitstream(void)
{
    /* 1. Assert CRESET (low) and SS (low) */
    gpio_pin_set(dev_gpiob, CRESET_PIN, 0);
    gpio_pin_set(dev_gpiob, SS_PIN, 0);
    delay_us(1);

    /* 2. Release CRESET (high), keep SS low */
    gpio_pin_set(dev_gpiob, CRESET_PIN, 1);
    delay_us(1200);  /* iCE40 needs >800µs to clear internal config */

    /* 3. Send bitstream over SPI */
    spi_write(spi_config_dev, fpga_bitstream, fpga_bitstream_len);

    /* 4. Send 49 dummy clocks (7 bytes) to complete startup */
    uint8_t dummy[7] = {0};
    spi_write(spi_config_dev, dummy, sizeof(dummy));

    /* 5. Release SS */
    gpio_pin_set(dev_gpiob, SS_PIN, 1);

    /* 6. Check CDONE went high */
    delay_us(100);
    if (!gpio_pin_get(dev_gpiob, CDONE_PIN)) {
        uart_print("FPGA: config FAILED (CDONE low)\n");
        return -1;
    }

    uart_print("FPGA: config OK\n");
    return 0;
}
```

### iCE40 SPI slave configuration protocol

The iCE40UP5K supports loading its bitstream over SPI (documented in Lattice TN1248):

1. Pull CRESET_B low → clears FPGA configuration memory
2. Pull SPI_SS_B low → FPGA enters SPI slave config mode
3. Release CRESET_B → FPGA waits for bitstream on SPI
4. Clock bitstream in on MOSI (MSB first, CPOL=0, CPHA=0)
5. Send 49 extra clocks after bitstream completes
6. Release SPI_SS_B → FPGA starts running
7. CDONE goes high when configuration is successful

The iCE40UP5K bitstream is ~32KB (104,090 bits for UP5K). At 8MHz SPI, loading takes ~4ms.

## What's reused from fpga-ppu

All of it:
- `spi_cmd.v` — SPI command decoder
- `sprite_table.v` — double-buffered sprite RAM
- `tile_table.v` — tile metadata
- `sprite_mem.v` — SPRAM pixel storage
- `pixel_gen.v` — scanline renderer
- `lcd_driver.v` — 8-bit parallel output
- `ppu_top.v` — top-level wiring + SB_HFOSC

The FPGA bitstream is identical to `fpga-ppu`. No Verilog changes needed.

## What's reused from gameboy

- `game.c` / `game.h` — game logic (unchanged, no display dependency)
- `buttons.c` / `buttons.h` — input handling
- `audio.c` / `audio.h` — audio mixing
- `flash_audio.c` / `flash_audio.h` — music streaming from W25Q128
- `main.c` — RTOS setup (minor changes: no display device, add SPI device for PPU)
- RTOS kernel, drivers (SPI, GPIO, UART, DMA, ADC) — all unchanged
- `linker.ld` — unchanged

## .config changes from gameboy

```diff
-CONFIG_DISPLAY_ILI9341=y
+CONFIG_DISPLAY_ILI9341=n
+CONFIG_ICE40_PPU=y
```

The MCU no longer has a display driver. It talks to the iCE40 over SPI, and the FPGA handles the display.

## Device tree (board.dts)

The iCE40UP5K appears as a child of SPI1 (game data) with SPI3 for bitstream configuration:

```dts
spi1: spi@40013000 {
    compatible = "st,stm32-spi";
    reg = <0x40013000>;
    clocks = <&rcc 2 12>;
    sck-port = <&gpioa>;
    sck-pin = <5>;
    sck-af = <5>;
    miso-port = <&gpioa>;
    miso-pin = <6>;
    miso-af = <5>;
    mosi-port = <&gpioa>;
    mosi-pin = <7>;
    mosi-af = <5>;
    cs-port = <&gpioa>;
    cs-pin = <4>;
    status = "okay";

    ice40: ppu@0 {
        compatible = "lattice,ice40up5k";
        status = "okay";
    };
};

spi3: spi@40003C00 {
    compatible = "st,stm32-spi";
    reg = <0x40003C00>;
    clocks = <&rcc 1 15>;
    sck-port = <&gpioc>;
    sck-pin = <10>;
    sck-af = <6>;
    mosi-port = <&gpioc>;
    mosi-pin = <12>;
    mosi-af = <6>;
    status = "okay";

    ice40-cfg {
        compatible = "lattice,ice40up5k-cfg";
        creset-port = <&gpiob>;
        creset-pin = <1>;
        cdone-port = <&gpiob>;
        cdone-pin = <2>;
        ss-port = <&gpiob>;
        ss-pin = <6>;
        status = "okay";
    };
};
```

The firmware uses the device tree to:
- Find the SPI bus for game data (`ice40` node on SPI1)
- Find the SPI bus + GPIO pins for bitstream loading (`ice40-cfg` node on SPI3)

SPI2/I2S2 remains dedicated to audio (MAX98357A), unchanged from gameboy v1.

The `ili9341` display node is removed entirely — the MCU doesn't know or care about the display. That's the FPGA's problem.

## Tile asset pipeline

```bash
# Convert PNG sprites to RGB565 binary + tile table
python3 assets/convert_tiles.py \
    --input assets/player.png assets/obstacle.png assets/digits.png \
    --output-pixels assets/tiles.bin \
    --output-table assets/tile_table.bin
```

The converter:
1. Reads PNGs, converts to RGB565
2. Replaces transparent pixels with 0xF81F (magenta)
3. Packs all tiles sequentially into `tiles.bin`
4. Generates `tile_table.bin` with {base_addr, width, height} per tile
5. Generates a C header with tile index enums

## Implementation order

### Phase 1: FPGA bitstream

1. Symlink `fpga-ppu/src/` into `gameboy-v2/fpga/src`
2. Create FPGA Makefile (synthesis + place & route → `ppu_top.bin`)
3. Build bitstream — verify it synthesizes

### Phase 2: Project skeleton + unified build

1. Create `projects/gameboy-v2/` directory
2. Create top-level Makefile with bitstream → C array → compile → link pipeline
3. Write `fpga_loader.c/h` (iCE40 SPI slave config protocol)
4. Copy `game.c`, `game.h`, `buttons.c/h`, `audio.c/h`, `flash_audio.c/h` from gameboy
5. Create `ppu.h` and `ppu.c` (SPI protocol driver)
6. Create new `render.c` (sprite-based)
7. Create `main.c` (adds `fpga_load_bitstream()` call before `ppu_init()`)
8. Create .config, board.dts, linker.ld
9. `make` produces single `gameboy-v2.bin` — verify it compiles and links

### Phase 3: Tile assets

1. Create placeholder tile PNGs (colored rectangles)
2. Write `convert_tiles.py`
3. Generate `tiles.bin` and `tile_table.bin`
4. Verify they get embedded in the unified binary

### Phase 4: Integration test in sim

1. Use `--machine gameboy-v2` with the new firmware
2. Verify: game boots → loads bitstream → uploads tiles → sends frames → display shows game
3. Compare visual output to gameboy v1

### Phase 5: Real hardware

1. Wire STM32 → iCE40 (config SPI + game SPI + LCD)
2. Flash unified `gameboy-v2.bin` to STM32
3. Power on — MCU loads bitstream into FPGA, then runs game
4. Verify game runs on real hardware

## Testing

### FPGA-only tests (--machine icebreaker)

These run just the PPU netlist in isolation, driving SPI input pins with test vectors and checking output behavior. No MCU firmware involved.

| Test | Stimulus | Verify |
|------|----------|--------|
| SPI byte reception | Bit-bang 0xA5 on SPI_CLK/MOSI pins | Internal spi_cmd shift register = 0xA5 |
| Sprite table write | Send cmd 0x01 + 1 sprite entry | Sprite table slot 0 has correct x/y/tile |
| Tile upload | Send cmd 0x02 + 16 bytes of pixel data | SPRAM contents match at expected address |
| Background color | Send cmd 0x03 + 0x867D | Non-sprite pixels output 0x867D on LCD_D |
| LCD output timing | Tick 10,000 cycles after sprite upload | LCD_WR strobes observed in VCD, correct data on LCD_D |
| Double buffer swap | Send frame, then send second frame | pixel_gen reads from first frame while second is being written |
| Scanline rendering | Place sprite at (100, 50) | LCD_D outputs sprite pixel data when scanline Y=50, X=100 |
| Transparency | Upload tile with 0xF81F pixels | Those pixel positions show background color, not magenta |
| Sprite priority | Two sprites overlapping at same position | Lower slot number (higher priority) wins |

Run with:
```bash
./build/sim-core \
    --machine icebreaker \
    --device fpga0=../projects/gameboy-v2/fpga/build/ppu_top_netlist.json \
    --stimulus tests/fpga/spi_byte.csv \
    --cycles 50000 \
    --vcd build/ppu_debug.vcd \
    --assert LCD_WR=0
```

These tests catch PPU bugs before involving the MCU firmware — faster iteration, easier to debug with VCD waveforms in GTKWave.

### End-to-end tests (--machine gameboy-v2)

| Test | What it verifies |
|------|-----------------|
| `make` produces single .bin | Unified build works end-to-end |
| Bitstream embedded | `fpga_bitstream_len` matches expected ~32KB |
| FPGA loads | CDONE goes high after `fpga_load_bitstream()` |
| Tile upload | PPU receives correct pixel data over SPI |
| Frame send | Sprite table arrives at FPGA correctly |
| Game logic | Same physics/collision as gameboy (game.c unchanged) |
| Display output | sim-web shows dino game with sprites |
| 30 FPS timing | Frame interval is 33ms ± 1ms |
| CPU headroom | MCU idle >90% of the time (measured via idle task counter) |

## Verification checklist

- [ ] Single `make` builds everything (bitstream + assets + firmware → one .bin)
- [ ] No separate flash steps for FPGA and MCU
- [ ] `fpga_load_bitstream()` succeeds (CDONE high)
- [ ] game.c is byte-for-byte identical to gameboy/src/game.c
- [ ] Firmware has no `#include "drivers/display.h"` anywhere
- [ ] SPI transfer to PPU completes in <100µs per frame
- [ ] Game runs at 30 FPS in sim
- [ ] Display shows correct sprites at correct positions
- [ ] Audio still works (unchanged path)
- [ ] Buttons still work (unchanged path)
- [ ] FPGA bitstream is identical to fpga-ppu output (no Verilog changes)
- [ ] STM32 internal flash usage: firmware + bitstream (~32KB) + tiles (~8KB) < 512KB
- [ ] W25Q128 usage: music (~2.6MB) < 16MB
