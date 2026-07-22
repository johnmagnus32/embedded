/*
 * pixel_gen.v — Scanline-based sprite renderer
 *
 * Uses explicit wait states for all registered memory reads.
 */
module pixel_gen (
    input         clk,
    input  [8:0]  pixel_x,
    input  [8:0]  pixel_y,
    input         pixel_req,
    output reg [15:0] pixel_color,
    output reg    pixel_valid,
    output reg [5:0] sprite_rd_idx,
    input  [39:0]    sprite_rd_data,
    input  [6:0]     num_sprites,
    output reg [7:0] tile_rd_idx,
    input  [31:0]    tile_rd_data,
    output reg [13:0] sprite_addr,
    input  [15:0]     sprite_data,
    input  [15:0] bg_color
);
    localparam TRANSPARENT = 16'hF81F;
    // Max sprites composited per scanline. A full row of ground-fill tiles is
    // SCR_W/16 = 20; on the ground scanlines the player + up to 3 obstacles also
    // count (the scan window is a fixed sp_y+40, so short sprites above the
    // ground are still "active" there), giving 24 — the exact old cap, which had
    // zero headroom and dropped a ground tile on any transient (seen as a chunk
    // of ground flashing to the sky-blue background). 32 gives margin. Measured
    // cost: +2% logic, Fmax ~25 MHz (still well above the 20 MHz clock); the
    // per-pixel compose loop runs over num_active, not this cap, so no slowdown.
    // Counters below are 6-bit to index 0..32.
    localparam MAX_PER_LINE = 32;

    reg [8:0] active_x     [0:MAX_PER_LINE-1];
    reg [7:0] active_y     [0:MAX_PER_LINE-1];
    reg [7:0] active_tile  [0:MAX_PER_LINE-1];
    reg [5:0] num_active;
    reg [15:0] active_base  [0:MAX_PER_LINE-1];
    reg [7:0]  active_width [0:MAX_PER_LINE-1];
    reg [7:0]  active_height[0:MAX_PER_LINE-1];
    reg [13:0] active_row_addr [0:MAX_PER_LINE-1];  // pre-computed per scanline

    localparam S_IDLE = 0, S_SCAN_SET = 1, S_SCAN_WAIT = 2, S_SCAN_READ = 3,
               S_TILE_SET = 4, S_TILE_WAIT = 5, S_TILE_READ = 6,
               S_COMPOSE = 7, S_SPRAM_SET = 8, S_SPRAM_WAIT = 9, S_OUTPUT = 10;
    reg [3:0] state = S_IDLE;
    reg [5:0] scan_idx;
    reg [5:0] tile_lookup_idx;
    reg [5:0] compose_idx;
    reg [8:0] last_y = 9'h1FF;
    reg found_pixel = 0;

    wire [8:0] sp_x = {sprite_rd_data[24], sprite_rd_data[39:32]};
    wire [7:0] sp_y = sprite_rd_data[23:16];
    wire [7:0] sp_tile = sprite_rd_data[15:8];

    always @(posedge clk) begin
        pixel_valid <= 0;

        case (state)
        S_IDLE: begin
            if (pixel_req && pixel_y != last_y) begin
                last_y <= pixel_y;
                scan_idx <= 0;
                num_active <= 0;
                sprite_rd_idx <= 0;
                state <= S_SCAN_SET;
            end else if (pixel_req) begin
                compose_idx <= 0;
                found_pixel <= 0;
                state <= (num_active > 0) ? S_COMPOSE : S_OUTPUT;
            end
        end

        // --- Sprite scan: three-phase (set addr, wait for BRAM, then read) ---
        S_SCAN_SET: begin
            // sprite_rd_idx was set last cycle, BRAM needs 1 cycle to latch
            state <= S_SCAN_WAIT;
        end

        S_SCAN_WAIT: begin
            // BRAM RDATA now valid, rd_data_reg captures this edge
            state <= S_SCAN_READ;
        end

        S_SCAN_READ: begin
            // Data is now valid for current sprite_rd_idx
            if (scan_idx < num_sprites) begin
                // Bounds guard: never write past active_*[MAX_PER_LINE-1].
                // (Without it, num_active could overflow the arrays/counter.)
                if (sp_x != 9'h1FF && num_active < MAX_PER_LINE &&
                    pixel_y >= sp_y && pixel_y < sp_y + 40) begin
                    active_x[num_active] <= sp_x;
                    active_y[num_active] <= sp_y;
                    active_tile[num_active] <= sp_tile;
                    num_active <= num_active + 1;
                end
                scan_idx <= scan_idx + 1;
                sprite_rd_idx <= scan_idx + 1;
                state <= S_SCAN_SET;
            end else begin
                // Done scanning
                if (num_active > 0) begin
                    tile_rd_idx <= active_tile[0];
                    tile_lookup_idx <= 0;
                    state <= S_TILE_SET;
                end else begin
                    state <= S_OUTPUT;
                end
            end
        end

        // --- Tile lookup: three-phase ---
        S_TILE_SET: begin
            state <= S_TILE_WAIT;
        end

        S_TILE_WAIT: begin
            state <= S_TILE_READ;
        end

        S_TILE_READ: begin
            active_base[tile_lookup_idx] <= tile_rd_data[31:16];
            active_width[tile_lookup_idx] <= tile_rd_data[15:8];
            active_height[tile_lookup_idx] <= tile_rd_data[7:0];
            // Pre-compute row start address: base + (y_offset << width_shift)
            // tile_rd_data[15:8] is width_shift (log2 of pixel width)
            active_row_addr[tile_lookup_idx] <= tile_rd_data[31:16] +
                ((pixel_y - active_y[tile_lookup_idx]) << tile_rd_data[11:8]);
            if (tile_lookup_idx + 1 >= num_active) begin
                compose_idx <= 0;
                found_pixel <= 0;
                state <= S_COMPOSE;
            end else begin
                tile_lookup_idx <= tile_lookup_idx + 1;
                tile_rd_idx <= active_tile[tile_lookup_idx + 1];
                state <= S_TILE_SET;
            end
        end

        // --- Pixel compositing ---
        S_COMPOSE: begin
            if (compose_idx < num_active && !found_pixel) begin
                if (pixel_x >= active_x[compose_idx] &&
                    pixel_x < active_x[compose_idx] + (8'd1 << active_width[compose_idx][3:0]) &&
                    pixel_y >= active_y[compose_idx] &&
                    pixel_y < active_y[compose_idx] + active_height[compose_idx]) begin
                    // addr = row_start + x_offset
                    sprite_addr <= active_row_addr[compose_idx] +
                                   (pixel_x - active_x[compose_idx]);
                    state <= S_SPRAM_SET;
                end else begin
                    compose_idx <= compose_idx + 1;
                end
            end else begin
                state <= S_OUTPUT;
            end
        end

        S_SPRAM_SET: begin
            // Wait one cycle for SPRAM address to propagate
            state <= S_SPRAM_WAIT;
        end

        S_SPRAM_WAIT: begin
            if (sprite_data != TRANSPARENT) begin
                pixel_color <= sprite_data;
                found_pixel <= 1;
            end else begin
                compose_idx <= compose_idx + 1;
            end
            state <= (sprite_data != TRANSPARENT) ? S_OUTPUT : S_COMPOSE;
        end

        S_OUTPUT: begin
            if (!found_pixel)
                pixel_color <= bg_color;
            pixel_valid <= 1;
            state <= S_IDLE;
        end
        endcase
    end
endmodule
