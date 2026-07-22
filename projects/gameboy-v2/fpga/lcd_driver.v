/*
 * lcd_driver.v — ILI9341 8-bit parallel (8080) interface
 *
 * Sends the ILI9341 wake + init sequence once, then re-issues the per-frame
 * address-window + memory-write commands and streams pixels (2 write cycles per
 * pixel: high byte, low byte).
 *
 * Pin mapping:
 *   D[7:0] — 8-bit data bus
 *   WR     — write strobe (active low, rising edge latches)
 *   DC     — data/command (1=data, 0=command)
 *   CS     — chip select (active low, active during frame)
 *
 * ILI9341 power-on note: after reset the panel is in Sleep-In / Display-Off and
 * ignores all RAM writes. It MUST be woken: Sleep-Out (0x11) then wait ~120 ms
 * for the internal supplies/clocks to stabilize, then Display-On (0x29). The
 * earlier version of this ROM jumped straight to MADCTL/CASET/RASET/RAMWR and so
 * left a real panel blank (it "worked" only against a simulated framebuffer that
 * has no sleep state). COLMOD (0x3A=0x55) is also set explicitly for 16bpp.
 */
module lcd_driver #(
    // 24 MHz system clock -> 120 ms Sleep-Out settle = 2,880,000 cycles.
    // Overridable so the testbench can shrink the wait (real hw uses the default).
    parameter [22:0] SLEEP_OUT_DELAY = 23'd2880000
) (
    input         clk,
    // Pulses high for 1 cycle at each frame boundary, BEFORE the next frame
    // starts reading pixels. The sprite table applies any pending bank swap on
    // this pulse so a swap never lands mid-frame (vsync — prevents tearing).
    output reg    frame_start,
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
    localparam W = 320;
    localparam H = 240;

    localparam S_INIT = 0, S_CMD = 1, S_CMD_WR = 2, S_DELAY = 3,
               S_REQ = 4, S_WAIT = 5, S_HI = 6, S_LO = 7, S_DONE = 8;
    reg [3:0] state = S_INIT;
    reg [15:0] cur_color;
    reg [1:0] wr_phase;
    reg [22:0] delay_cnt = 0;
    reg        woke = 0;          // set once the wake sequence has run

    // Init command sequence ROM. Format: {dc, data} — dc=0 command, dc=1 param.
    //   [0..5]  WAKE (one-time):  Sleep Out(+120ms) + COLMOD 16bpp + MADCTL + Display On
    //   [6..16] PER-FRAME:        CASET(0..319) + RASET(0..239) + RAMWR
    reg [8:0] init_rom [0:16];
    reg [4:0] init_idx;
    localparam [4:0] FRAME_START = 5'd6;   // first per-frame command (CASET)
    localparam [4:0] INIT_LAST   = 5'd16;  // RAMWR — pixels stream after this

    initial begin
        // --- WAKE (run once at power-up) ---
        init_rom[0]  = {1'b0, 8'h11};  // Sleep Out   (then wait ~120 ms)
        init_rom[1]  = {1'b0, 8'h3A};  // COLMOD
        init_rom[2]  = {1'b1, 8'h55};  //   16 bits/pixel (RGB565)
        init_rom[3]  = {1'b0, 8'h36};  // MADCTL
        init_rom[4]  = {1'b1, 8'h28};  //   MV=1 landscape + BGR
        init_rom[5]  = {1'b0, 8'h29};  // Display ON
        // --- PER-FRAME (re-issued every frame) ---
        init_rom[6]  = {1'b0, 8'h2A};  // CASET (columns)
        init_rom[7]  = {1'b1, 8'h00};  //   col_start hi
        init_rom[8]  = {1'b1, 8'h00};  //   col_start lo
        init_rom[9]  = {1'b1, 8'h01};  //   col_end hi
        init_rom[10] = {1'b1, 8'h3F};  //   col_end lo (319)
        init_rom[11] = {1'b0, 8'h2B};  // RASET (rows)
        init_rom[12] = {1'b1, 8'h00};  //   row_start hi
        init_rom[13] = {1'b1, 8'h00};  //   row_start lo
        init_rom[14] = {1'b1, 8'h00};  //   row_end hi
        init_rom[15] = {1'b1, 8'hEF};  //   row_end lo (239)
        init_rom[16] = {1'b0, 8'h2C};  // RAMWR -> pixel data follows
    end

    // No initial block for outputs — all set explicitly by the state machine.
    // This avoids Yosys inserting inverters for non-zero initial values.

    always @(posedge clk) begin
        pixel_req <= 0;
        frame_start <= 0;

        case (state)
        S_INIT: begin
            lcd_cs <= 0;
            lcd_dc <= 0;
            // First boot: run the full ROM from Sleep-Out. Later frames: skip the
            // one-time wake and re-issue only the per-frame window + RAMWR.
            init_idx <= woke ? FRAME_START : 5'd0;
            state <= S_CMD;
        end

        S_CMD: begin
            // Set DC and data, WR stays high (setup phase)
            lcd_dc <= init_rom[init_idx][8];
            lcd_data <= init_rom[init_idx][7:0];
            state <= S_CMD_WR;
        end

        S_CMD_WR: begin
            // Strobe WR low then high
            if (!wr_phase[0]) begin
                lcd_wr <= 0;
                wr_phase <= 1;
            end else begin
                lcd_wr <= 1;
                wr_phase <= 0;
                if (init_idx == 0) begin
                    // Sleep-Out just issued: wait ~120 ms before any more commands.
                    delay_cnt <= 0;
                    state <= S_DELAY;
                end else if (init_idx == INIT_LAST) begin
                    woke <= 1;
                    pixel_x <= 0;
                    pixel_y <= 0;
                    state <= S_REQ;
                end else begin
                    init_idx <= init_idx + 1;
                    state <= S_CMD;
                end
            end
        end

        S_DELAY: begin
            // One-time 120 ms Sleep-Out settle, then resume the wake sequence.
            if (delay_cnt >= SLEEP_OUT_DELAY) begin
                init_idx <= 1;
                state <= S_CMD;
            end else begin
                delay_cnt <= delay_cnt + 1;
            end
        end

        S_REQ: begin
            lcd_dc <= 1;
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
            case (wr_phase)
                0: begin lcd_data <= cur_color[15:8]; lcd_wr <= 0; wr_phase <= 1; end
                1: begin lcd_wr <= 1; state <= S_LO; wr_phase <= 0; end
            endcase
        end

        S_LO: begin
            case (wr_phase)
                0: begin lcd_data <= cur_color[7:0]; lcd_wr <= 0; wr_phase <= 1; end
                1: begin
                    lcd_wr <= 1;
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
            // End of frame: deassert CS and immediately start the next frame.
            // (Previously idled here for 800000 clocks = 66.7 ms of dead time,
            // which more than doubled the frame period for no benefit. The panel
            // accepts back-to-back frames fine, so re-render at full speed.)
            // Pulse frame_start here (between frames, before S_INIT re-reads any
            // sprites) so the sprite table applies a pending bank swap now.
            lcd_cs <= 1;
            frame_start <= 1;
            state <= S_INIT;
        end
        endcase
    end
endmodule
