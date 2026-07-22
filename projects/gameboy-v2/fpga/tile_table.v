/*
 * tile_table.v — Tile metadata table (256 entries)
 *
 * Each entry: {base_addr[15:0], width[7:0], height[7:0]} = 32 bits
 * Written by SPI command 0x04. Read by pixel_gen to look up tile dimensions.
 */
module tile_table (
    input         clk,
    // Write port (from spi_cmd)
    input  [7:0]  wr_idx,
    input  [31:0] wr_data,  // {base_addr[15:0], width[7:0], height[7:0]}
    input         wr_en,
    // Read port (from pixel_gen)
    input  [7:0]  rd_idx,
    output reg [31:0] rd_data
);
    reg [31:0] entries [0:255];

    always @(posedge clk) begin
        if (wr_en) entries[wr_idx] <= wr_data;
        rd_data <= entries[rd_idx];
    end
endmodule
