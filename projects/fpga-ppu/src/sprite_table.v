/*
 * sprite_table.v — Double-buffered sprite table (64 entries)
 *
 * Each entry: {x[8:0], y[7:0], tile[7:0], flags[7:0]} = 33 bits (stored as 40)
 * SPI writes to bank B. On frame_valid, banks swap.
 * Pixel gen reads from bank A (stable during rendering).
 */
module sprite_table (
    input         clk,
    // Write port (from spi_cmd)
    input  [5:0]  wr_idx,
    input  [39:0] wr_data,  // {x_hi[0], x_lo[7:0], y[7:0], tile[7:0], flags[7:0]}
    input         wr_en,
    input         swap,     // pulse on frame_valid to swap banks
    // Read port (from pixel_gen)
    input  [5:0]  rd_idx,
    output [39:0] rd_data,
    // Number of active sprites (written by spi_cmd)
    input  [6:0]  wr_num_sprites,
    input         wr_num_en,
    output [6:0]  num_sprites
);
    reg [39:0] bank0 [0:63];
    reg [39:0] bank1 [0:63];
    reg active_bank = 0;  // pixel_gen reads from this bank
    reg [6:0] num_a = 0, num_b = 0;

    // Write always goes to the inactive bank
    always @(posedge clk) begin
        if (wr_en) begin
            if (active_bank == 0) bank1[wr_idx] <= wr_data;
            else                  bank0[wr_idx] <= wr_data;
        end
        if (wr_num_en) begin
            if (active_bank == 0) num_b <= wr_num_sprites;
            else                  num_a <= wr_num_sprites;
        end
        if (swap) active_bank <= ~active_bank;
    end

    // Read from active bank
    assign rd_data = active_bank ? bank1[rd_idx] : bank0[rd_idx];
    assign num_sprites = active_bank ? num_b : num_a;
endmodule
