/*
 * game_regs.v — Double-buffered game state
 *
 * SPI writes to the "back" buffer. On frame_valid pulse, back swaps
 * to front. Pixel generator reads from front (stable during rendering).
 */
module game_regs (
    input        clk,
    input        frame_valid,    // pulse: new frame data ready

    // Write side (from spi_cmd)
    input  [7:0] in_dino_y,
    input  [1:0] in_dino_frame,
    input  [8:0] in_obs0_x,
    input  [1:0] in_obs0_type,
    input  [8:0] in_obs1_x,
    input  [1:0] in_obs1_type,

    // Read side (to pixel_gen, stable during frame)
    output reg [7:0] dino_y,
    output reg [1:0] dino_frame,
    output reg [8:0] obs0_x,
    output reg [1:0] obs0_type,
    output reg [8:0] obs1_x,
    output reg [1:0] obs1_type
);
    always @(posedge clk) begin
        if (frame_valid) begin
            dino_y     <= in_dino_y;
            dino_frame <= in_dino_frame;
            obs0_x     <= in_obs0_x;
            obs0_type  <= in_obs0_type;
            obs1_x     <= in_obs1_x;
            obs1_type  <= in_obs1_type;
        end
    end
endmodule
