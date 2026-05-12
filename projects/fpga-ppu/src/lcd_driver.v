/*
 * lcd_driver.v — ILI9341 8-bit parallel (8080) interface
 *
 * Outputs pixels via 8-bit parallel bus at clk/4 rate.
 * Two write cycles per pixel (high byte, low byte).
 * Drives pixel_req to pixel_gen, receives pixel_color back.
 *
 * Pin mapping:
 *   D[7:0] — 8-bit data bus
 *   WR     — write strobe (active low, rising edge latches)
 *   DC     — data/command (1=data, 0=command)
 *   CS     — chip select (active low, active during frame)
 */
module lcd_driver (
    input         clk,
    // Pixel interface (to pixel_gen)
    output reg [8:0] pixel_x,
    output reg [8:0] pixel_y,
    output reg       pixel_req,
    input  [15:0]    pixel_color,
    input            pixel_valid,
    // Parallel LCD pins
    output reg [7:0] lcd_data,
    output reg       lcd_wr,
    output reg       lcd_dc,
    output reg       lcd_cs
);
    localparam W = 240;  // landscape: 240 columns
    localparam H = 320;  // landscape: 320 rows (rotated)

    localparam S_INIT = 0, S_REQ = 1, S_WAIT = 2, S_HI = 3, S_LO = 4, S_DONE = 5;
    reg [2:0] state = S_INIT;
    reg [15:0] cur_color;
    reg [1:0] wr_phase;

    // Frame timing: 60 FPS = 16.67ms. At 48MHz = 800,000 cycles/frame.
    // Pixel output: 240*320 = 76800 pixels, ~10 cycles each = 768,000 cycles.
    reg [19:0] frame_timer = 0;
    localparam FRAME_CYCLES = 800000;

    initial begin
        lcd_wr = 1;
        lcd_dc = 1;
        lcd_cs = 1;
        pixel_req = 0;
    end

    always @(posedge clk) begin
        pixel_req <= 0;

        case (state)
        S_INIT: begin
            // Start of frame
            lcd_cs <= 0;
            lcd_dc <= 1;  // data mode
            pixel_x <= 0;
            pixel_y <= 0;
            state <= S_REQ;
        end

        S_REQ: begin
            pixel_req <= 1;
            state <= S_WAIT;
        end

        S_WAIT: begin
            if (pixel_valid) begin
                cur_color <= pixel_color;
                state <= S_HI;
                wr_phase <= 0;
            end
        end

        S_HI: begin
            // Write high byte
            case (wr_phase)
                0: begin lcd_data <= cur_color[15:8]; lcd_wr <= 0; wr_phase <= 1; end
                1: begin lcd_wr <= 1; state <= S_LO; wr_phase <= 0; end
            endcase
        end

        S_LO: begin
            // Write low byte
            case (wr_phase)
                0: begin lcd_data <= cur_color[7:0]; lcd_wr <= 0; wr_phase <= 1; end
                1: begin
                    lcd_wr <= 1;
                    // Advance to next pixel
                    if (pixel_x == W - 1) begin
                        pixel_x <= 0;
                        if (pixel_y == H - 1) begin
                            state <= S_DONE;
                        end else begin
                            pixel_y <= pixel_y + 1;
                            state <= S_REQ;
                        end
                    end else begin
                        pixel_x <= pixel_x + 1;
                        state <= S_REQ;
                    end
                end
            endcase
        end

        S_DONE: begin
            lcd_cs <= 1;
            // Wait for frame timing
            frame_timer <= frame_timer + 1;
            if (frame_timer >= FRAME_CYCLES) begin
                frame_timer <= 0;
                state <= S_INIT;
            end
        end
        endcase
    end
endmodule
