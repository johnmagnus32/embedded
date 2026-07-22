# PPU Integration Test

End-to-end test of the PPU FPGA design using the `fpga-test` machine. Firmware sends SPI commands, FPGA renders pixels, firmware asserts on LCD output.

## Problem

The existing `fpga-test` machine wires FPGA output pins to MCU GPIOB inputs via callbacks. But the PPU's LCD driver produces WR strobes that are 1-2 FPGA clock cycles wide. At the 48MHz/16MHz (3:1) ratio, WR goes low and back high between MCU ticks — firmware polling can never observe the falling edge.

This was confirmed: `test_fpga_ppu.c` passes the `ppu_lcd_active` check (LCD_CS goes low) but fails to capture any WR strobes.

## Approach: Memory-mapped capture buffer

Add a small hardware capture buffer to the `fpga-test` machine that records LCD writes automatically. The WR falling-edge callback (already fired by the FPGA sim on every output pin change) pushes `{dc, data}` pairs into a ring buffer. Firmware reads captured bytes from a memory-mapped peripheral.

### Capture peripheral (sim-side)

Memory-mapped at a fixed address (e.g., `0x50000000`):

| Offset | R/W | Description |
|--------|-----|-------------|
| 0x00 | R | Count of captured bytes available |
| 0x04 | R | Pop next captured byte (DC in bit 8, data in bits 7:0) |
| 0x08 | W | Write 1 to reset/clear buffer |

Implementation: ~30 lines in `fpga_test.c`. A ring buffer of 4096 entries. The existing `fpga_to_mcu_handler` already fires on every FPGA output change — add a check: if the changed pin is LCD_WR and the new level is 0 (falling edge), read LCD_D[0:7] and LCD_DC from the current pin levels and push into the buffer.

### Pin identification

The capture logic needs to know which FPGA output pins are LCD_WR, LCD_DC, and LCD_D[0..7]. Two options:

**Option A: By name.** After `wire_fpga()`, search output pins for names matching `LCD_WR`, `LCD_DC`, `LCD_D[0]`..`LCD_D[7]`. Only activate capture if all are found (so it's a no-op for simpler FPGA tests).

**Option B: By command-line flag.** Pass `--capture lcd` or similar. Overkill for now.

Go with Option A — auto-detect by pin name.

### Test firmware

Same structure as the existing test (`test_fpga_ppu.c`), but reads from the capture peripheral instead of polling GPIOB:

```c
#define CAP_COUNT (*(volatile unsigned int *)0x50000000)
#define CAP_POP   (*(volatile unsigned int *)0x50000004)
#define CAP_RESET (*(volatile unsigned int *)0x50000008)

void test_main(void)
{
    /* Init: CS high, CLK low */
    odr = PIN_SPI_CS;
    GPIOA_ODR = odr;
    delay(200);

    CAP_RESET = 1;  /* clear any boot-time LCD init traffic */

    /* Send CMD_BG_COLOR(0x1234) */
    cs_low();
    spi_send_byte(0x03);
    spi_send_byte(0x12);
    spi_send_byte(0x34);
    cs_high();

    /* Wait for LCD to render some pixels */
    delay(100000);

    /* Read captured LCD writes */
    int count = CAP_COUNT;
    CHECK(count >= 4);  /* at least 2 pixels (4 bytes: hi,lo,hi,lo) */

    /* First pixel should be bg color 0x1234 */
    unsigned int hi = CAP_POP & 0xFF;
    unsigned int lo = CAP_POP & 0xFF;
    CHECK(hi == 0x12);
    CHECK(lo == 0x34);

    /* Subsequent pixels should also be 0x1234 (no sprites) */
    unsigned int hi2 = CAP_POP & 0xFF;
    unsigned int lo2 = CAP_POP & 0xFF;
    CHECK(hi2 == 0x12);
    CHECK(lo2 == 0x34);

    TEST_DONE("fpga_ppu");
}
```

### What this tests

1. SPI command decoder receives and parses CMD_BG_COLOR correctly
2. LCD driver starts rendering after FPGA init
3. Pixel generator outputs the configured background color
4. The full pipeline (SPI → cmd decode → pixel gen → LCD driver → output pins) works end-to-end

### Future extensions

- Send CMD_PIXEL_UPLOAD + CMD_SPRITE_UPDATE, verify non-bg pixels appear at expected positions
- Send CMD_TILE_TABLE, verify tile lookup works
- Verify frame timing (count pixels between LCD_CS deassert/reassert = 240×320×2 bytes)

## Implementation steps

1. Add capture ring buffer struct to `fpga_test.h`
2. In `wire_fpga()`, detect LCD_WR/LCD_DC/LCD_D pins by name; store indices
3. Modify `fpga_to_mcu_handler()`: on LCD_WR falling edge, push `{dc, data}` to buffer
4. Register membus region at `0x50000000` with read/write handlers for count/pop/reset
5. Update `test_fpga_ppu.c` to use capture peripheral instead of GPIOB polling
6. Verify: `make test` passes for all existing FPGA tests (capture is no-op when LCD pins not found)
7. Verify: `test_fpga_ppu` passes

## Alternatives considered

**Poll faster (lower FPGA:MCU ratio):** Would work but makes the test unrealistic — real hardware has the same problem that you can't poll fast enough. The capture buffer is analogous to DMA or a hardware FIFO.

**Use EXTI interrupts on WR pin:** The STM32 EXTI model could trigger an ISR on WR falling edge. But ISR latency (~12 cycles) means the FPGA has already moved on by the time the ISR reads LCD_D. Same fundamental problem.

**Wire into ILI9341 model and check framebuffer:** Would work (gameboy_v2 already does this) but requires adding the ILI9341 model to fpga-test, plus a way for firmware to read the framebuffer. Heavier than needed for a unit test of the FPGA logic.

**Dedicated ppu-test machine:** Overkill. The capture buffer is a small addition to the existing fpga-test machine and benefits any future FPGA test that has fast output signals.
