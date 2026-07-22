/*
 * spi_led_top.v — SPI slave that drives two external LEDs on GPIO pins.
 *
 * This is the FPGA half of the fpga-spi exercise: the STM32 (SPI master)
 * sends one command byte per CS-framed transaction over the *runtime* SPI
 * bus; the FPGA latches it and drives two user I/O from its low two bits:
 *
 *   command bit 0 -> LED0   (FPGA pin 48, IOB_4A, header H3-8)
 *   command bit 1 -> LED1   (FPGA pin 2,  IOB_6A, header H3-9)
 *
 * Each LED pin drives both an external LED (pin -> ~330R -> LED -> GND,
 * active-high: 1 = lit) AND an STM32 input pin, so the firmware can read the
 * FPGA's outputs back and self-verify the SPI link. See spi.pcf for the pin
 * constraints and ../README.md for the full wiring + protocol.
 *
 * Target: iCE40UP5K-SG48 on the ice40-breakout board, configured over the
 * *config* SPI bus by the STM32 at boot (see projects/fpga-blink). This module
 * uses the separate *runtime* SPI bus for the LED command channel.
 */
module spi_led_top (
    input  SPI_CLK,    // runtime SPI clock  (from STM32 SPI master, mode 0)
    input  SPI_MOSI,   // runtime SPI data   (from STM32, MSB first)
    input  SPI_CS,     // runtime SPI select (active low; frames each byte)
    output LED0,       // command bit 0 -> external LED0 + STM32 feedback in
    output LED1        // command bit 1 -> external LED1 + STM32 feedback in
);
    /* Internal high-frequency oscillator: 48 MHz HFOSC / 4 = 12 MHz system
     * clock (same as the blink exercise). Used to oversample the SPI bus. */
    wire clk;
    SB_HFOSC #(.CLKHF_DIV("0b10")) osc (
        .CLKHFEN(1'b1), .CLKHFPU(1'b1), .CLKHF(clk)
    );

    /* SPI slave: shifts in one byte per CS frame, pulses valid when complete.
     * Pure logic lives in spi_slave.v (simulation-testable). */
    wire [7:0] rx_data;
    wire       rx_valid;
    spi_slave spi (
        .clk(clk),
        .spi_clk(SPI_CLK),
        .spi_mosi(SPI_MOSI),
        .spi_cs(SPI_CS),
        .data(rx_data),
        .valid(rx_valid)
    );

    /* Latch the received command byte. Powers up 0 -> both LEDs off, which is
     * the state the firmware assumes right after configuration. */
    reg [7:0] led_reg = 0;
    always @(posedge clk)
        if (rx_valid) led_reg <= rx_data;

    /* Drive the two LED pins directly from the latched command bits. */
    assign LED0 = led_reg[0];
    assign LED1 = led_reg[1];
endmodule
