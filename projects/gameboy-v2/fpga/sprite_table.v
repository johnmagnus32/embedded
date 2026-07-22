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
    input         swap,        // frame_valid: REQUEST a bank swap (may arrive mid-render)
    input         apply_swap,  // frame boundary: actually apply a pending swap here
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

    // Deferred (vsync'd) swap: `swap` (frame_valid) can arrive at ANY point,
    // including mid-frame. Swapping the active bank mid-render would draw the top
    // of the screen from the old positions and the bottom from the new ones —
    // visible tearing. So we only LATCH the request here and apply it on
    // `apply_swap` (pulsed by lcd_driver between frames), so a whole frame is
    // always drawn from one consistent bank.
    reg swap_pending = 0;

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
        if (swap) swap_pending <= 1;              // remember the request
        if (apply_swap && swap_pending) begin     // apply only at frame boundary
            active_bank  <= ~active_bank;
            swap_pending <= 0;
        end
    end

    // Read from active bank (registered — matches SB_RAM40_4K behavior)
    reg [39:0] rd_data_reg;
    always @(posedge clk)
        rd_data_reg <= active_bank ? bank1[rd_idx] : bank0[rd_idx];
    assign rd_data = rd_data_reg;
    assign num_sprites = active_bank ? num_b : num_a;
endmodule
