/*
 * sprite_table.v — Double-buffered sprite table (64 entries)
 *
 * Each entry: {x[8:0], y[7:0], tile[7:0], flags[7:0]} = 33 bits (stored as 40)
 * The STM32 writes the next frame into the INACTIVE bank; pixel_gen reads the
 * ACTIVE bank (stable during rendering). The banks swap at a display frame
 * boundary (apply_swap) — but only if the inactive bank holds a COMPLETE frame,
 * never mid-write. See the swap-logic comment below for the tearing this guards.
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

    // Deferred (vsync'd) swap with write-in-progress protection.
    //
    // `swap` (frame_valid) marks "the inactive bank now holds a COMPLETE frame".
    // We latch that as swap_pending and only apply it on `apply_swap` (a display
    // frame boundary from lcd_driver) so a swap never lands mid-render.
    //
    // BUT the STM32 can start writing the NEXT frame into that same inactive bank
    // before the display has applied the pending swap (it occasionally gets a
    // frame ahead — 30 Hz game loop vs 32 Hz display, plus jitter). If apply_swap
    // then fired, it would flip to a HALF-OVERWRITTEN bank → the display shows a
    // mix of two frames (half-old/half-new): the "ground flashes" tearing,
    // reproduced in sim and confirmed on silicon (slower SPI made it worse).
    //
    // Fix: a write to the inactive bank invalidates its "complete" status
    // (clears swap_pending) — while a burst is in progress the bank is NOT a
    // coherent frame, so it must not be swapped in. And never apply a swap on a
    // cycle a write is occurring. The completed frame's frame_valid re-arms
    // swap_pending after the burst. Net effect: the display only ever swaps to a
    // fully-written, coherent bank; a frame that gets lapped is simply skipped
    // (an invisible dropped frame), never torn.
    reg swap_pending = 0;
    wire writing = wr_en | wr_num_en;

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

        if (apply_swap && swap_pending && !writing) begin
            // Apply only at a frame boundary, only if the inactive bank still
            // holds the complete frame (no write in progress this cycle).
            active_bank  <= ~active_bank;
            swap_pending <= 0;
        end else if (writing) begin
            // A write means the inactive bank is being (re)filled -> not a
            // complete frame until the next frame_valid re-arms it.
            swap_pending <= 0;
        end else if (swap) begin
            swap_pending <= 1;                    // frame complete -> arm the swap
        end
    end

    // Read from active bank (registered — matches SB_RAM40_4K behavior)
    reg [39:0] rd_data_reg;
    always @(posedge clk)
        rd_data_reg <= active_bank ? bank1[rd_idx] : bank0[rd_idx];
    assign rd_data = rd_data_reg;
    assign num_sprites = active_bank ? num_b : num_a;
endmodule
