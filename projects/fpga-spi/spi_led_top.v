/*
 * spi_led_top.v — SPI slave + 3 LEDs on GPIO pins
 *
 * Receives a byte over SPI. Lower 3 bits drive external LEDs:
 *   bit 0 = LED_B, bit 1 = LED_G, bit 2 = LED_R
 *
 * Wire: FPGA pin → 330Ω resistor → LED → GND
 */
module spi_led_top (
    input  SPI_CLK,
    input  SPI_MOSI,
    input  SPI_CS,
    output LED_R,
    output LED_G,
    output LED_B
);
    // 12MHz system clock from internal oscillator
    wire clk;
    SB_HFOSC #(.CLKHF_DIV("0b10")) osc (
        .CLKHFEN(1'b1), .CLKHFPU(1'b1), .CLKHF(clk)
    );

    // SPI slave receives bytes
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

    // Latch received byte
    reg [7:0] led_reg = 0;
    always @(posedge clk)
        if (rx_valid) led_reg <= rx_data;

    // Drive LEDs directly from GPIO
    assign LED_R = led_reg[2];
    assign LED_G = led_reg[1];
    assign LED_B = led_reg[0];
endmodule
