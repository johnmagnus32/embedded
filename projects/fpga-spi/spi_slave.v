/*
 * spi_slave.v — Receives one byte via SPI, outputs it when complete
 *
 * Pure logic, no hard IP. Testable in any simulator.
 */
module spi_slave (
    input  clk,         // system clock (faster than SPI clock)
    input  spi_clk,     // SPI clock from master
    input  spi_mosi,    // data from master
    input  spi_cs,      // chip select (active low)
    output [7:0] data,  // received byte
    output valid        // pulses high for 1 cycle when byte is ready
);
    // Synchronize SPI clock to system clock domain (avoid metastability)
    // 3 stages: [0] might be metastable, [1] is clean, [2] is previous clean
    reg [2:0] sclk_sync = 0;
    always @(posedge clk)
        sclk_sync <= {sclk_sync[1:0], spi_clk};
    wire sclk_rise = (sclk_sync[2:1] == 2'b01);  // edge detect on clean bits only

    // Shift register — shifts in one bit per SPI clock rising edge
    reg [7:0] shift = 0;
    reg [2:0] bit_count = 0;
    reg       byte_valid = 0;

    always @(posedge clk) begin
        byte_valid <= 0;
        if (spi_cs) begin
            // CS high = deselected, reset
            bit_count <= 0;
        end else if (sclk_rise) begin
            // Shift in one bit (MSB first)
            shift <= {shift[6:0], spi_mosi};
            bit_count <= bit_count + 1;
            if (bit_count == 7) begin
                byte_valid <= 1;  // full byte received
            end
        end
    end

    assign data = shift;
    assign valid = byte_valid;
endmodule
