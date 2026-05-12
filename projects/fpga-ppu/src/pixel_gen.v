/*
 * pixel_gen.v — Scanline-based sprite renderer
 *
 * Per scanline:
 *   Phase 1: Scan sprite table, build active list (sprites on this Y)
 *   Phase 2: For each pixel X, check active sprites for overlap, read SPRAM
 *
 * Max 8 sprites per scanline (like NES PPU).
 * Transparent color: 0xF81F (magenta).
 */
module pixel_gen (
    input         clk,
    // LCD interface
    input  [8:0]  pixel_x,
    input  [8:0]  pixel_y,
    input         pixel_req,
    output reg [15:0] pixel_color,
    output reg    pixel_valid,
    // Sprite table read
    output reg [5:0] sprite_rd_idx,
    input  [39:0]    sprite_rd_data,
    input  [6:0]     num_sprites,
    // Tile table read
    output reg [7:0] tile_rd_idx,
    input  [31:0]    tile_rd_data,
    // SPRAM read
    output reg [13:0] sprite_addr,
    input  [15:0]     sprite_data,
    // Background color
    input  [15:0] bg_color
);
    localparam TRANSPARENT = 16'hF81F;
    localparam MAX_PER_LINE = 8;

    // Active sprite list for current scanline
    reg [5:0] active_slots [0:MAX_PER_LINE-1];
    reg [8:0] active_x     [0:MAX_PER_LINE-1];
    reg [7:0] active_y     [0:MAX_PER_LINE-1];
    reg [7:0] active_tile  [0:MAX_PER_LINE-1];
    reg [7:0] active_flags [0:MAX_PER_LINE-1];
    reg [3:0] num_active;

    // Tile info cache for active sprites
    reg [15:0] active_base  [0:MAX_PER_LINE-1];
    reg [7:0]  active_width [0:MAX_PER_LINE-1];
    reg [7:0]  active_height[0:MAX_PER_LINE-1];

    // State machine
    localparam S_IDLE = 0, S_SCAN = 1, S_TILE_LOOKUP = 2, S_RENDER = 3;
    reg [1:0] state = S_IDLE;
    reg [5:0] scan_idx;
    reg [3:0] tile_lookup_idx;
    reg [8:0] last_y = 9'h1FF;

    // Sprite entry unpacking
    wire [8:0] entry_x = {sprite_rd_data[39], sprite_rd_data[38:31]};
    wire [7:0] entry_y = sprite_rd_data[30:23];  // Adjusted bit positions
    wire [7:0] entry_tile = sprite_rd_data[22:15];
    wire [7:0] entry_flags = sprite_rd_data[14:7];

    // Simplified: unpack as {x_hi, x_lo, y, tile, flags}
    // Actually the SPI sends: x_lo, x_hi, y, tile, flags (5 bytes)
    // Stored as 40 bits: [39:32]=x_lo, [31:24]=x_hi, [23:16]=y, [15:8]=tile, [7:0]=flags
    wire [8:0] sp_x = {sprite_rd_data[24], sprite_rd_data[39:32]};
    wire [7:0] sp_y = sprite_rd_data[23:16];
    wire [7:0] sp_tile = sprite_rd_data[15:8];
    wire [7:0] sp_flags = sprite_rd_data[7:0];

    always @(posedge clk) begin
        pixel_valid <= 0;

        case (state)
        S_IDLE: begin
            if (pixel_req && pixel_y != last_y) begin
                // New scanline — start sprite evaluation
                last_y <= pixel_y;
                scan_idx <= 0;
                num_active <= 0;
                state <= S_SCAN;
                sprite_rd_idx <= 0;
            end else if (pixel_req) begin
                // Same scanline — render pixel immediately
                state <= S_RENDER;
            end
        end

        S_SCAN: begin
            // Check if this sprite is on the current scanline
            // (need tile height — for now assume 16 pixels tall, refine in S_TILE_LOOKUP)
            if (scan_idx < num_sprites && num_active < MAX_PER_LINE) begin
                // Simple check: is pixel_y within [sp_y, sp_y+16)?
                // Full check requires tile height — we'll do a coarse pass first
                if (pixel_y >= sp_y && pixel_y < sp_y + 40 && sp_x != 9'h1FF) begin
                    active_slots[num_active] <= scan_idx;
                    active_x[num_active] <= sp_x;
                    active_y[num_active] <= sp_y;
                    active_tile[num_active] <= sp_tile;
                    active_flags[num_active] <= sp_flags;
                    num_active <= num_active + 1;
                end
                scan_idx <= scan_idx + 1;
                sprite_rd_idx <= scan_idx + 1;
            end else begin
                // Done scanning — look up tile info for active sprites
                tile_lookup_idx <= 0;
                if (num_active > 0) begin
                    tile_rd_idx <= active_tile[0];
                    state <= S_TILE_LOOKUP;
                end else begin
                    state <= S_RENDER;
                end
            end
        end

        S_TILE_LOOKUP: begin
            // Read tile table entry for each active sprite
            active_base[tile_lookup_idx] <= tile_rd_data[31:16];
            active_width[tile_lookup_idx] <= tile_rd_data[15:8];
            active_height[tile_lookup_idx] <= tile_rd_data[7:0];
            tile_lookup_idx <= tile_lookup_idx + 1;
            if (tile_lookup_idx + 1 < num_active)
                tile_rd_idx <= active_tile[tile_lookup_idx + 1];
            else
                state <= S_RENDER;
        end

        S_RENDER: begin
            // Output background color (sprite compositing requires
            // sequential SPRAM reads — handled by a sub-state machine
            // in a full implementation. For now, output background.)
            pixel_color <= bg_color;
            pixel_valid <= 1;
            state <= S_IDLE;
        end
        endcase
    end
endmodule
