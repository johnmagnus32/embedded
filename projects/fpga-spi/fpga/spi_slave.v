/*
 * spi_slave.v — Receives one byte via SPI, outputs it when complete.
 *
 * Pure logic, no hard IP. Testable in any simulator.
 *
 * Glitch-filtered edge detection. The ~250 kHz runtime SPI clock is oversampled
 * by the 12 MHz system clock (~24x), so a real SCK half-period spans many system
 * clocks. We first 2-FF-synchronize the async inputs, then DEBOUNCE spi_clk:
 * its recognized level only flips after N consecutive agreeing samples, and the
 * bit is shifted in on the rising edge of that debounced level.
 *
 * Why: a naive one-sample edge detector (sclk_sync[2:1]==2'b01) treats ANY
 * single-cycle low->high blip as a clock edge. On breadboard jumper wiring a
 * sub-microsecond glitch on SCK (ringing/crosstalk) then gets counted as an
 * extra edge, so byte_valid fires after only 7 real data bits — the shift
 * register holds (byte >> 1). On real STM32<->iCE40 silicon this showed up as
 * ~1.5% of runtime commands reading back exactly command>>1 (reproduced in sim
 * by injecting a 100 ns SCK glitch; see fpga/ tests). Debouncing rejects any
 * glitch up to N-1 system clocks wide while staying far shorter than a real
 * ~2 us SCK phase, so genuine edges are untouched.
 */
module spi_slave (
    input  clk,         // system clock (faster than SPI clock)
    input  spi_clk,     // SPI clock from master
    input  spi_mosi,    // data from master
    input  spi_cs,      // chip select (active low)
    output [7:0] data,  // received byte
    output valid        // pulses high for 1 cycle when byte is ready
);
    // 2-FF metastability guards on all async inputs.
    reg [1:0] sclk_ff = 0, mosi_ff = 0, scs_ff = 2'b11;
    always @(posedge clk) begin
        sclk_ff <= {sclk_ff[0], spi_clk};
        mosi_ff <= {mosi_ff[0], spi_mosi};
        scs_ff  <= {scs_ff[0],  spi_cs};
    end
    wire sclk_m  = sclk_ff[1];   // synchronized SCK sample
    wire mosi_m  = mosi_ff[1];   // synchronized MOSI sample
    wire cs_high = scs_ff[1];    // synchronized CS (idles high = deselected)

    // Debounce spi_clk: only accept a new level after N consecutive agreeing
    // samples. N=3 rejects glitches up to 2 system clocks (~167 ns) wide, well
    // under a real ~2 us SCK half-period (~24 samples), so real edges pass.
    localparam N = 3;
    reg       sclk_level   = 0;  // debounced SCK level
    reg       sclk_level_d = 0;  // previous debounced level (for edge detect)
    reg [1:0] cnt = 0;
    always @(posedge clk) begin
        if (sclk_m != sclk_level) begin
            if (cnt == N-1) begin
                sclk_level <= sclk_m;
                cnt <= 0;
            end else
                cnt <= cnt + 1;
        end else
            cnt <= 0;
        sclk_level_d <= sclk_level;
    end
    wire sclk_rise = (sclk_level && !sclk_level_d);  // rising edge of debounced SCK

    // Shift register — shifts in one bit per (debounced) SPI clock rising edge.
    reg [7:0] shift = 0;
    reg [2:0] bit_count = 0;
    reg       byte_valid = 0;

    always @(posedge clk) begin
        byte_valid <= 0;
        if (cs_high) begin
            // CS high = deselected, reset
            bit_count <= 0;
        end else if (sclk_rise) begin
            // Shift in one bit (MSB first)
            shift <= {shift[6:0], mosi_m};
            bit_count <= bit_count + 1;
            if (bit_count == 7) begin
                byte_valid <= 1;  // full byte received
            end
        end
    end

    assign data = shift;
    assign valid = byte_valid;
endmodule
