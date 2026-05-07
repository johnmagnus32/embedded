/*
 * lcd_driver.v — SPI master that streams pixels to ILI9341
 *
 * Continuously renders frames:
 *   1. Send column/row address set commands
 *   2. Send memory write command (0x2C)
 *   3. Stream 240×320 pixels (requests from pixel_gen)
 *   4. Repeat
 *
 * SPI output at clk/2 (6MHz with 12MHz system clock).
 */
module lcd_driver (
    input         clk,

    // Pixel interface (to pixel_gen)
    output reg [8:0] pixel_x,
    output reg [8:0] pixel_y,
    output reg       pixel_req,
    input      [15:0] pixel_color,
    input             pixel_valid,

    // SPI output (to ILI9341)
    output reg       lcd_sclk,
    output reg       lcd_mosi,
    output reg       lcd_cs,
    output reg       lcd_dc     // 0=command, 1=data
);
    // States
    localparam S_INIT     = 0;
    localparam S_CMD      = 1;  // sending command/init bytes
    localparam S_PIXELS   = 2;  // streaming pixel data
    localparam S_WAIT     = 3;  // waiting for pixel_valid

    reg [2:0] state = S_INIT;
    reg [4:0] init_idx = 0;
    reg [7:0] tx_byte = 0;
    reg [2:0] tx_bit = 0;
    reg       tx_active = 0;
    reg       sclk_phase = 0;
    reg       pixel_high = 1;  // sending high byte or low byte of pixel

    // Init sequence: minimal ILI9341 setup
    // [dc, byte] pairs
    reg [8:0] init_seq [0:11];
    initial begin
        init_seq[0]  = {1'b0, 8'h01};  // software reset
        init_seq[1]  = {1'b0, 8'h11};  // sleep out
        init_seq[2]  = {1'b0, 8'h3A};  // pixel format
        init_seq[3]  = {1'b1, 8'h55};  //   16-bit RGB565
        init_seq[4]  = {1'b0, 8'h2A};  // column address set
        init_seq[5]  = {1'b1, 8'h00};  //   start high
        init_seq[6]  = {1'b1, 8'h00};  //   start low (0)
        init_seq[7]  = {1'b1, 8'h00};  //   end high
        init_seq[8]  = {1'b1, 8'hEF};  //   end low (239)
        init_seq[9]  = {1'b0, 8'h2B};  // row address set
        init_seq[10] = {1'b1, 8'h00};  //   0 to 319 (4 bytes)
        init_seq[11] = {1'b0, 8'h29};  // display on
    end

    // Delay counter for init
    reg [15:0] delay_cnt = 0;
    reg        delaying = 0;

    // Frame pixel counter
    reg [16:0] pixel_count = 0;  // 240*320 = 76800 pixels

    always @(posedge clk) begin
        // SPI clock generation (clk/2)
        if (tx_active) begin
            sclk_phase <= ~sclk_phase;
            if (!sclk_phase) begin
                // Rising edge: shift out bit
                lcd_sclk <= 1;
                lcd_mosi <= tx_byte[7];
            end else begin
                // Falling edge: advance to next bit
                lcd_sclk <= 0;
                tx_byte <= {tx_byte[6:0], 1'b0};
                tx_bit <= tx_bit + 1;
                if (tx_bit == 7) begin
                    tx_active <= 0;
                end
            end
        end

        pixel_req <= 0;

        case (state)
        S_INIT: begin
            lcd_cs <= 0;
            if (delaying) begin
                delay_cnt <= delay_cnt - 1;
                if (delay_cnt == 0) delaying <= 0;
            end else if (!tx_active) begin
                if (init_idx < 12) begin
                    lcd_dc <= init_seq[init_idx][8];
                    tx_byte <= init_seq[init_idx][7:0];
                    tx_active <= 1;
                    tx_bit <= 0;
                    sclk_phase <= 0;
                    init_idx <= init_idx + 1;
                    // Add delay after reset and sleep out
                    if (init_idx == 0 || init_idx == 1) begin
                        delaying <= 1;
                        delay_cnt <= 16'hFFFF;
                    end
                end else begin
                    // Init done, start pixel streaming
                    state <= S_CMD;
                end
            end
        end

        S_CMD: begin
            // Send 0x2C (memory write) command before each frame
            if (!tx_active) begin
                lcd_dc <= 0;
                tx_byte <= 8'h2C;
                tx_active <= 1;
                tx_bit <= 0;
                sclk_phase <= 0;
                pixel_x <= 0;
                pixel_y <= 0;
                pixel_count <= 0;
                state <= S_PIXELS;
            end
        end

        S_PIXELS: begin
            if (!tx_active && !pixel_req) begin
                if (pixel_count >= 76800) begin
                    // Frame done, start next
                    state <= S_CMD;
                end else begin
                    // Request next pixel
                    pixel_req <= 1;
                    state <= S_WAIT;
                end
            end
        end

        S_WAIT: begin
            if (pixel_valid) begin
                // Send high byte first
                lcd_dc <= 1;
                tx_byte <= pixel_color[15:8];
                tx_active <= 1;
                tx_bit <= 0;
                sclk_phase <= 0;
                pixel_high <= 0;
                state <= S_PIXELS;
            end
            // After high byte sent, send low byte
            if (!tx_active && !pixel_high) begin
                tx_byte <= pixel_color[7:0];
                tx_active <= 1;
                tx_bit <= 0;
                sclk_phase <= 0;
                pixel_high <= 1;
                // Advance pixel position
                pixel_count <= pixel_count + 1;
                if (pixel_x == 239) begin
                    pixel_x <= 0;
                    pixel_y <= pixel_y + 1;
                end else begin
                    pixel_x <= pixel_x + 1;
                end
            end
        end
        endcase
    end
endmodule
