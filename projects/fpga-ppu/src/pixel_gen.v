/*
 * pixel_gen.v — Pixel compositor for dino run game
 *
 * Given the current pixel coordinate and game state, outputs the
 * RGB565 color for that pixel. Reads sprite data from sprite memory.
 *
 * Screen layout (240×320, landscape):
 *   Sky:    rows 0-239 (light blue)
 *   Ground: rows 240-259 (brown line at row 240, dirt below)
 *   Dino:   20×40 sprite, X fixed at 30, Y from game_regs
 *   Obstacles: 16×40 sprites at variable X positions
 *
 * Sprite memory layout (set by STM32 during upload):
 *   0x0000: dino frame 0 (20×40 = 800 pixels × 2 bytes = 1600 bytes)
 *   0x0640: dino frame 1
 *   0x0C80: dino frame 2
 *   0x12C0: obstacle type 0 (16×40 = 640 pixels = 1280 bytes)
 *   0x17C0: obstacle type 1
 */
module pixel_gen (
    input         clk,
    input  [8:0]  pixel_x,     // 0-239
    input  [8:0]  pixel_y,     // 0-319
    input         pixel_req,   // request pixel color

    // Game state (from game_regs)
    input  [7:0]  dino_y,      // pixels above ground (0 = on ground)
    input  [1:0]  dino_frame,
    input  [8:0]  obs0_x,
    input  [1:0]  obs0_type,
    input  [8:0]  obs1_x,
    input  [1:0]  obs1_type,

    // Sprite memory read port
    output reg [13:0] sprite_addr,
    input      [15:0] sprite_data,

    // Output
    output reg [15:0] pixel_color,
    output reg        pixel_valid
);
    // Constants
    localparam GROUND_Y    = 240;
    localparam DINO_X      = 30;
    localparam DINO_W      = 20;
    localparam DINO_H      = 40;
    localparam OBS_W       = 16;
    localparam OBS_H       = 40;
    localparam SKY_COLOR   = 16'h867D;   // light blue
    localparam GND_COLOR   = 16'h79E0;   // brown-green
    localparam TRANSPARENT = 16'hF81F;   // magenta = transparent

    // Sprite base addresses (byte address / 2 = word address)
    localparam DINO_BASE   = 14'd0;       // frame 0 at word 0
    localparam DINO_STRIDE = 14'd800;     // 20*40 pixels per frame
    localparam OBS_BASE    = 14'd2400;    // after 3 dino frames
    localparam OBS_STRIDE  = 14'd640;     // 16*40 pixels per type

    // Compute dino screen position
    wire [8:0] dino_top = GROUND_Y - dino_y - DINO_H;

    // Check if current pixel is inside dino
    wire in_dino = (pixel_x >= DINO_X) && (pixel_x < DINO_X + DINO_W) &&
                   (pixel_y >= dino_top) && (pixel_y < dino_top + DINO_H);

    // Check obstacles
    wire in_obs0 = (obs0_x < 320) &&
                   (pixel_x >= obs0_x[7:0]) && (pixel_x < obs0_x[7:0] + OBS_W) &&
                   (pixel_y >= GROUND_Y - OBS_H) && (pixel_y < GROUND_Y);

    wire in_obs1 = (obs1_x < 320) &&
                   (pixel_x >= obs1_x[7:0]) && (pixel_x < obs1_x[7:0] + OBS_W) &&
                   (pixel_y >= GROUND_Y - OBS_H) && (pixel_y < GROUND_Y);

    // Pipeline: request sprite data, then output color next cycle
    reg       req_d = 0;
    reg       is_dino_d = 0;
    reg       is_obs_d = 0;
    reg       is_ground_d = 0;

    always @(posedge clk) begin
        pixel_valid <= 0;
        req_d <= pixel_req;

        if (pixel_req) begin
            // Determine what to read from sprite memory
            if (in_dino) begin
                sprite_addr <= DINO_BASE + (dino_frame * DINO_STRIDE) +
                               (pixel_y - dino_top) * DINO_W +
                               (pixel_x - DINO_X);
                is_dino_d <= 1;
                is_obs_d <= 0;
            end else if (in_obs0) begin
                sprite_addr <= OBS_BASE + (obs0_type * OBS_STRIDE) +
                               (pixel_y - (GROUND_Y - OBS_H)) * OBS_W +
                               (pixel_x - obs0_x[7:0]);
                is_dino_d <= 0;
                is_obs_d <= 1;
            end else if (in_obs1) begin
                sprite_addr <= OBS_BASE + (obs1_type * OBS_STRIDE) +
                               (pixel_y - (GROUND_Y - OBS_H)) * OBS_W +
                               (pixel_x - obs1_x[7:0]);
                is_dino_d <= 0;
                is_obs_d <= 1;
            end else begin
                is_dino_d <= 0;
                is_obs_d <= 0;
            end
            is_ground_d <= (pixel_y >= GROUND_Y);
        end

        // One cycle later: sprite data is available
        if (req_d) begin
            pixel_valid <= 1;
            if ((is_dino_d || is_obs_d) && sprite_data != TRANSPARENT)
                pixel_color <= sprite_data;
            else if (is_ground_d)
                pixel_color <= GND_COLOR;
            else
                pixel_color <= SKY_COLOR;
        end
    end
endmodule
