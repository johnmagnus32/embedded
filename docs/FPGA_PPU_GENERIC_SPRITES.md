Refactor the FPGA PPU from hardcoded dino game registers to a generic sprite table renderer. The PPU becomes game-agnostic — it renders N sprites at given positions with given tile graphics. The STM32 decides what each sprite represents. Work in `/home/johmagnu/learning/simple-stm32/projects/fpga-ppu`. Build with `make`.

## Goal

Replace the fixed `dino_y`, `obs0_x`, etc. registers with a generic sprite table. The FPGA doesn't know what a "dino" or "obstacle" is — it just renders sprites at coordinates. Any game can use the same PPU bitstream by uploading different tiles and filling the sprite table differently.

## Architecture

```
STM32 (any game)                    FPGA PPU (generic)
────────────────                    ──────────────────
Game logic decides:                 Sprite table RAM (64 entries):
  "dino at (30, 180), tile 2"        slot 0: x=30, y=180, tile=2
  "cactus at (200, 200), tile 4"     slot 1: x=200, y=200, tile=4
  "coin at (150, 100), tile 8"       slot 2: x=150, y=100, tile=8
                                      ...
Sends sprite table over SPI →→→     Pixel gen checks all 64 slots per pixel
                                    Reads tile pixels from SPRAM
                                    Outputs composited color to display
```

## SPI command protocol

### Command 0x01: Update sprite table

```
CS low
  [0x01]                    ← command byte
  [num_sprites]             ← how many entries follow (0-64)
  For each sprite (5 bytes):
    [x_low]                 ← X position bits [7:0]
    [x_high]                ← X position bit [8] (0 or 1)
    [y_low]                 ← Y position bits [7:0]
    [tile]                  ← tile index (0-255)
    [flags]                 ← bit 0: flip H, bit 1: flip V, bit 2: priority
CS high                     ← frame_valid: latch sprite table for rendering
```

Total per frame: 2 + (N × 5) bytes. For 10 sprites: 52 bytes. At 8MHz SPI: ~52μs.

Unused slots (beyond num_sprites) are marked inactive — pixel_gen skips them.

### Command 0x02: Upload tile graphics (unchanged)

```
CS low
  [0x02] [addr_h] [addr_l] [pixel_data...]
CS high
```

Streams RGB565 pixel data into SPRAM. Called once at boot to load all tile graphics.

### Command 0x03: Set background color (new, optional)

```
CS low
  [0x03] [color_high] [color_low]
CS high
```

Sets the background fill color (default: sky blue 0x867D).

## Sprite table format (in FPGA block RAM)

```
64 entries × 5 bytes = 320 bytes (fits in 2 block RAMs)

struct sprite_entry {  // FPGA-side, in block RAM
    uint9_t  x;       // 0-319 (9 bits), 511 = inactive/offscreen
    uint8_t  y;       // 0-255 (screen is 240 tall in landscape, 320 in portrait)
    uint8_t  tile;    // index into tile table (0-255)
    uint8_t  flags;   // flip_h, flip_v, priority, (reserved)
};
```

## Tile table (metadata, separate from pixel data)

A small lookup table mapping tile index → SPRAM base address + dimensions:

```
Tile table (in block RAM, 256 entries × 4 bytes = 1KB):

struct tile_info {
    uint16_t base_addr;  // word address in SPRAM where pixels start
    uint8_t  width;      // tile width in pixels (8, 16, 20, 24, 32)
    uint8_t  height;     // tile height in pixels
};
```

Uploaded via a new command:

### Command 0x04: Upload tile table

```
CS low
  [0x04] [num_tiles]
  For each tile (4 bytes):
    [base_addr_h] [base_addr_l] [width] [height]
CS high
```

This allows variable-sized sprites — the dino can be 20×40, a coin can be 12×12, a bird can be 24×16. The pixel_gen reads the tile's dimensions to know how many pixels to look up.

## SPRAM layout (sprite pixel data)

```
Address 0x0000: Tile 0 pixels (e.g., dino standing, 20×40 = 800 words)
Address 0x0320: Tile 1 pixels (dino run frame 1)
Address 0x0640: Tile 2 pixels (dino run frame 2)
...
Address 0x1000: Tile 4 pixels (small cactus, 16×40 = 640 words)
...
```

The tile table tells the pixel_gen where each tile starts. The STM32 decides the layout when it uploads sprites at boot.

## File changes

### Modified files

```
projects/fpga-ppu/src/
├── spi_cmd.v       ← rewrite: parse sprite table + tile table commands
├── game_regs.v     ← DELETE (replaced by sprite_table.v)
├── pixel_gen.v     ← rewrite: iterate sprite table instead of fixed checks
└── ppu_top.v       ← update wiring
```

### New files

```
projects/fpga-ppu/src/
├── sprite_table.v  ← NEW: block RAM holding 64 sprite entries
├── tile_table.v    ← NEW: block RAM holding 256 tile metadata entries
```

### Unchanged files

```
projects/fpga-ppu/src/
├── sprite_mem.v    ← unchanged (SPRAM for pixel data)
├── lcd_driver.v    ← REWRITE: parallel 8080 interface instead of SPI
```

## sprite_table.v

Dual-port block RAM: SPI writes entries, pixel_gen reads them.

```verilog
module sprite_table (
    input         clk,
    // Write port (from spi_cmd during frame update)
    input  [5:0]  wr_idx,       // which slot (0-63)
    input  [39:0] wr_data,      // {x[8:0], y[7:0], tile[7:0], flags[7:0], ...}
    input         wr_en,
    // Read port (from pixel_gen during rendering)
    input  [5:0]  rd_idx,
    output [39:0] rd_data
);
    reg [39:0] entries [0:63];
    // ... dual-port RAM implementation ...
endmodule
```

## pixel_gen.v (rewritten)

The pixel generator now scans the sprite table for each pixel:

```verilog
// Simplified logic per pixel:
//
// For pixel at (px, py):
//   color = background_color
//   for slot = 0 to num_active_sprites-1:
//     entry = sprite_table[slot]
//     tile = tile_table[entry.tile]
//     if px >= entry.x AND px < entry.x + tile.width AND
//        py >= entry.y AND py < entry.y + tile.height:
//       sprite_px = px - entry.x
//       sprite_py = py - entry.y
//       pixel = spram[tile.base + sprite_py * tile.width + sprite_px]
//       if pixel != TRANSPARENT:
//         color = pixel
//   output color
```

In hardware, checking all 64 sprites per pixel sequentially would be too slow (64 SPRAM reads per pixel). Instead, use a **scanline approach**:

### Scanline sprite evaluation

Before rendering each scanline, scan the sprite table once and build a list of sprites that intersect this Y coordinate (max 8-16 per scanline). Then for each pixel on that scanline, only check those few sprites:

```
Start of scanline Y=180:
  Scan 64 entries: which sprites have entry.y <= 180 < entry.y + height?
  Result: slots 0, 1, 5 are active on this scanline (3 sprites)

For each pixel X on this scanline:
  Only check slots 0, 1, 5 (not all 64)
  Much faster — max 8-16 SPRAM lookups per pixel instead of 64
```

This is exactly how the NES PPU works — it evaluates sprites per-scanline, with a max of 8 sprites per line.

```verilog
// Per-scanline sprite list (built at start of each scanline)
reg [5:0] active_sprites [0:15];  // up to 16 sprites per scanline
reg [3:0] num_active = 0;

// At start of scanline:
//   for i = 0 to num_sprites-1:
//     if sprite_table[i].y <= current_y < sprite_table[i].y + tile_height:
//       active_sprites[num_active++] = i

// Per pixel:
//   for i = 0 to num_active-1:
//     check active_sprites[i] for X overlap + read pixel
```

## STM32 firmware (game-side)

```c
// Generic sprite table API — works for any game
#define MAX_SPRITES 64

struct ppu_sprite {
    uint16_t x;      // screen X (9 bits used)
    uint8_t  y;      // screen Y
    uint8_t  tile;   // tile index
    uint8_t  flags;  // flip, priority
};

static struct ppu_sprite sprites[MAX_SPRITES];
static int num_sprites = 0;

void ppu_clear_sprites(void) { num_sprites = 0; }

void ppu_add_sprite(uint16_t x, uint8_t y, uint8_t tile, uint8_t flags) {
    if (num_sprites < MAX_SPRITES) {
        sprites[num_sprites++] = (struct ppu_sprite){x, y, tile, flags};
    }
}

void ppu_send_frame(void) {
    spi_cs_low();
    spi_write_byte(0x01);
    spi_write_byte(num_sprites);
    for (int i = 0; i < num_sprites; i++) {
        spi_write_byte(sprites[i].x & 0xFF);
        spi_write_byte(sprites[i].x >> 8);
        spi_write_byte(sprites[i].y);
        spi_write_byte(sprites[i].tile);
        spi_write_byte(sprites[i].flags);
    }
    spi_cs_high();  // triggers frame_valid in FPGA
}
```

### Dino game using the generic API:

```c
void dino_game_render(void) {
    ppu_clear_sprites();

    // Dino
    ppu_add_sprite(30, GROUND_Y - dino.height - dino.jump_y,
                   TILE_DINO_BASE + dino.anim_frame, 0);

    // Obstacles
    for (int i = 0; i < num_obstacles; i++)
        ppu_add_sprite(obstacles[i].x, GROUND_Y - OBS_HEIGHT,
                       TILE_OBS_BASE + obstacles[i].type, 0);

    // Coins
    for (int i = 0; i < num_coins; i++)
        ppu_add_sprite(coins[i].x, coins[i].y,
                       TILE_COIN_BASE + (tick / 8) % 4, 0);

    // Score digits
    for (int d = 0; d < 5; d++)
        ppu_add_sprite(200 + d * 8, 10,
                       TILE_DIGIT_BASE + score_digits[d], 0);

    ppu_send_frame();
}
```

### Space invaders using the same API:

```c
void invaders_render(void) {
    ppu_clear_sprites();

    // Player ship
    ppu_add_sprite(player.x, 280, TILE_SHIP, 0);

    // Aliens (5×11 grid)
    for (int r = 0; r < 5; r++)
        for (int c = 0; c < 11; c++)
            if (aliens[r][c].alive)
                ppu_add_sprite(aliens_base_x + c * 16, 40 + r * 20,
                               TILE_ALIEN_BASE + r + (tick / 30) % 2, 0);

    // Bullets
    for (int i = 0; i < num_bullets; i++)
        ppu_add_sprite(bullets[i].x, bullets[i].y, TILE_BULLET, 0);

    ppu_send_frame();
}
```

Same FPGA bitstream, completely different game.

## Boot sequence

```
1. STM32 configures FPGA (loads bitstream)
2. STM32 uploads tile graphics:
     spi_cmd 0x02: pixel data for all tiles → SPRAM
3. STM32 uploads tile table:
     spi_cmd 0x04: base_addr + width + height for each tile
4. STM32 starts game loop:
     Each frame: fill sprite table → spi_cmd 0x01 → FPGA renders
```

## Testing

### Simulator tests (sim/fpga)

| Test | What it verifies |
|------|-----------------|
| Sprite table write/read | Write 10 entries via SPI, read back from pixel_gen, verify positions match |
| Single sprite render | Upload a 4×4 red tile, place sprite at (100, 100), verify pixel at (101, 101) is red |
| Transparency | Sprite pixel = 0xF81F should show background, not magenta |
| Sprite priority | Two overlapping sprites — higher slot number wins (front) |
| Scanline limit | 17 sprites on same scanline — verify first 16 render, 17th is dropped |
| Offscreen | Sprite at X=511 should not appear anywhere |
| Background color | Command 0x03 changes background, verify non-sprite pixels use new color |
| Variable tile size | Two tiles with different dimensions, verify both render at correct size |

### Integration test (with MCU sim via cosim or scripted)

| Test | What it verifies |
|------|-----------------|
| Full frame | Upload dino tiles, send frame with dino at Y=20, capture LCD output, verify dino pixels at expected screen position |
| Animation | Send 3 frames with different tile indices, verify LCD output changes |
| Multiple sprites | 10 sprites at various positions, verify all appear in rendered frame |

## Resource estimate (iCE40UP5K)

| Resource | Usage | Available | Notes |
|----------|-------|-----------|-------|
| LUTs | ~1500-2000 | 5280 | Sprite comparators + address math + line buffer control |
| Block RAM | ~10 of 30 | 30 | Sprite table ×2 (4) + tile table (2) + line buffers (2) + misc (2) |
| SPRAM | 1 of 4 banks | 4 | Tile pixel data (16KB) |
| Flip-flops | ~200 | 5280 | State machines, pipeline regs |

Fits comfortably with room for future additions (background tiles, scrolling, more sprites).

## Implementation refinements

### System clock: 48MHz

Use `SB_HFOSC` with no divider (`CLKHF_DIV("0b00")` → 48MHz). The higher clock gives 4× more cycles per scanline for sprite evaluation and SPRAM reads. The LCD parallel output runs at clk/2 = 24MHz (one byte per 2 cycles). Pixel output rate is decoupled from system clock via the line buffer.

### Display interface: 8-bit parallel (8080) at 60 FPS

Use the ILI9341's 8080 parallel interface instead of SPI. This gives ~312 FPS theoretical throughput — capped to 60 FPS with a frame timing counter.

```
FPGA pins to ILI9341:
    D0-D7:  8 data pins (directly from line buffer output)
    WR:     write strobe (active low pulse)
    DC:     data/command select
    CS:     chip select (active low, tie low permanently)
    RST:    reset (active low, tie to GPIO or hold high)

Timing per pixel:
    Cycle 0: put high byte on D0-D7, WR=0
    Cycle 1: WR=1 (rising edge latches byte)
    Cycle 2: put low byte on D0-D7, WR=0
    Cycle 3: WR=1
    → 4 cycles per pixel at 48MHz = 83ns/pixel
    → 76800 pixels × 83ns = 6.4ms per frame
    → max ~156 FPS, capped to 60 FPS
```

Frame rate control: after rendering + outputting all 320 scanlines, wait until 16.67ms has elapsed since the last frame start before beginning the next frame.

Pin count: 8 (data) + 1 (WR) + 1 (DC) + 1 (CS) = 11 pins for the display. Plus 3 pins for STM32 SPI input = 14 total. The iCE40UP5K has 39 GPIO — plenty of headroom.

### Line buffer rendering (solves SPRAM bandwidth)

SPRAM is single-port — one read per cycle. With 8 sprites per scanline and 240 pixels, that's up to 1920 SPRAM reads per scanline. At 48MHz with ~1248 cycles per scanline (at 30fps), this is tight but workable with pipelining.

The approach: **pre-render the next scanline while outputting the current one.**

```
Line buffer A: being output to LCD (pixel by pixel, via lcd_driver)
Line buffer B: being rendered (sprite evaluation + SPRAM reads)

At end of scanline: swap A and B
```

Each line buffer is 240 × 16-bit = 480 bytes → 1 block RAM each.

Rendering pipeline for one scanline (runs during the previous scanline's output time):

```
Phase 1 (cycles 0-63): Scan sprite table, build active list
    Read 64 sprite entries from sprite table
    For each: does entry.y <= current_y < entry.y + tile.height?
    Store up to 8 matching slot indices in active_sprites[]

Phase 2 (cycles 64-1200+): Render 240 pixels
    For each pixel X (0-239):
        color = background
        For each active sprite (0-7):
            if X within sprite's X range:
                compute SPRAM address
                read pixel from SPRAM (1 cycle)
                if not transparent: color = sprite pixel
        Write color to line buffer B at position X
```

At 48MHz, phase 2 has ~5 cycles per pixel per sprite check (1200 cycles / 240 pixels = 5 cycles each). With 8 sprites that's tight — may need to limit to 4 sprites per scanline initially, or accept that some scanlines take longer (variable-length rendering, output stalls until buffer is ready).

### Double-buffered sprite table (solves frame tearing)

Two copies of the sprite table in block RAM:

```
Bank A: pixel_gen reads from here during rendering
Bank B: spi_cmd writes here during SPI transfer

On frame_valid (CS deassert): swap A and B (just flip a pointer bit)
```

This ensures pixel_gen never reads a partially-updated sprite table. Cost: 4 block RAMs instead of 2 for the sprite table (320 bytes × 2 banks).

### Sprite-per-scanline limit: 8

Like the NES (which had 8). If more than 8 sprites overlap on the same scanline, the lowest-priority ones (highest slot numbers) are dropped. This is a hard limit enforced during scanline evaluation — the active list is capped at 8 entries.

This is a known limitation to document for game developers: "don't put more than 8 sprites on the same horizontal line."

### Compositing priority: lower slot = higher priority

Slot 0 is drawn last (on top). Slot 63 is drawn first (behind everything). During per-pixel evaluation, sprites are checked from highest slot to lowest — the first non-transparent hit wins. This means the STM32 should put the most important sprites (player) in low slots and background sprites (particles, decorations) in high slots.

### Transparent color: 0xF81F (magenta)

Any pixel in SPRAM with value 0xF81F is treated as transparent — the background or lower-priority sprite shows through. This means actual magenta cannot be displayed. This is the standard convention used by most 2D game engines and sprite editors.

### Flip H/V implementation

When `flags` bit 0 (flip_h) is set:
```
sprite_px = tile.width - 1 - (pixel_x - entry.x)   // instead of (pixel_x - entry.x)
```

When `flags` bit 1 (flip_v) is set:
```
sprite_py = tile.height - 1 - (pixel_y - entry.y)   // instead of (pixel_y - entry.y)
```

One subtraction + mux in the address calculation path. Minimal LUT cost.

## Verification checklist

- [ ] `make sim` passes all sprite table tests
- [ ] Single sprite renders at correct position
- [ ] Transparency works (magenta pixels show background)
- [ ] Variable tile sizes render correctly
- [ ] Scanline sprite limit enforced (no glitches with >8 sprites per line)
- [ ] Frame update is atomic (no tearing mid-SPI-transfer)
- [ ] Same bitstream works for dino game and a different game (e.g., test with invaders-style layout)
- [ ] Boot sequence works: upload tiles → upload tile table → send frames
- [ ] `make synth` fits on iCE40UP5K with >50% LUT headroom
- [ ] Parallel LCD output achieves 60 FPS (frame time ≤ 16.67ms)
- [ ] Line buffer double-buffering: no visual tearing between scanlines
- [ ] Frame timing: consistent 60 FPS with no dropped frames under 8 sprites per scanline
