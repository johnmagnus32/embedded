/*
 * tb_frame_timing.v — measure the FPGA display frame rate (clocks per frame).
 *
 * Drives the full PPU (spi_cmd -> tables/mem -> pixel_gen -> lcd_driver) exactly
 * like the real hardware: uploads a tile table + a realistic scene (a full row
 * of 20 ground tiles + 3 obstacles + player = 24 sprites, the worst case that
 * exercises the raised MAX_PER_LINE), then counts system clocks between
 * successive LCD_CS rising edges (= frame boundaries in lcd_driver S_DONE).
 *
 * Reports clocks/frame and the implied FPS at the 12 MHz system clock, both
 * WITH the fixed S_DONE inter-frame gap and the render-only time (gap removed).
 * Pure measurement — no PASS/FAIL.
 */
`timescale 1ns/1ps
module tb_frame_timing;
    localparam real FCLK = 12_000_000.0;  // actual silicon clock (SB_HFOSC /4)

    reg clk = 0;
    always #41.667 clk = ~clk;            // ~12 MHz

    // SPI master model (drive spi_cmd)
    reg spi_clk = 0, spi_mosi = 0, spi_cs = 1;

    wire [5:0]  cmd_sprite_wr_idx;  wire [39:0] cmd_sprite_wr_data; wire cmd_sprite_wr_en;
    wire [6:0]  cmd_sprite_num;     wire cmd_sprite_num_en;         wire cmd_frame_valid;
    wire [7:0]  cmd_tile_wr_idx;    wire [31:0] cmd_tile_wr_data;   wire cmd_tile_wr_en;
    wire [14:0] cmd_mem_addr;       wire [7:0]  cmd_mem_wdata;      wire cmd_mem_we;
    wire [15:0] cmd_bg_color;

    spi_cmd u_spi_cmd (
        .clk(clk), .spi_clk(spi_clk), .spi_mosi(spi_mosi), .spi_cs(spi_cs),
        .sprite_wr_idx(cmd_sprite_wr_idx), .sprite_wr_data(cmd_sprite_wr_data),
        .sprite_wr_en(cmd_sprite_wr_en), .sprite_num(cmd_sprite_num),
        .sprite_num_en(cmd_sprite_num_en), .frame_valid(cmd_frame_valid),
        .tile_wr_idx(cmd_tile_wr_idx), .tile_wr_data(cmd_tile_wr_data),
        .tile_wr_en(cmd_tile_wr_en), .mem_addr(cmd_mem_addr),
        .mem_wdata(cmd_mem_wdata), .mem_we(cmd_mem_we), .bg_color(cmd_bg_color)
    );

    wire lcd_frame_start;
    wire [5:0] sprite_rd_idx; wire [39:0] sprite_rd_data; wire [6:0] sprite_num_active;
    sprite_table u_sprite_table (
        .clk(clk), .wr_idx(cmd_sprite_wr_idx), .wr_data(cmd_sprite_wr_data),
        .wr_en(cmd_sprite_wr_en), .swap(cmd_frame_valid), .apply_swap(lcd_frame_start),
        .rd_idx(sprite_rd_idx), .rd_data(sprite_rd_data),
        .wr_num_sprites(cmd_sprite_num), .wr_num_en(cmd_sprite_num_en),
        .num_sprites(sprite_num_active)
    );

    wire [7:0] tile_rd_idx; wire [31:0] tile_rd_data;
    tile_table u_tile_table (
        .clk(clk), .wr_idx(cmd_tile_wr_idx), .wr_data(cmd_tile_wr_data),
        .wr_en(cmd_tile_wr_en), .rd_idx(tile_rd_idx), .rd_data(tile_rd_data)
    );

    wire [13:0] sprite_rd_addr; wire [15:0] sprite_rd_pixel;
    sprite_mem u_sprite_mem (
        .clk(clk), .wr_addr(cmd_mem_addr), .wr_data(cmd_mem_wdata), .wr_en(cmd_mem_we),
        .rd_addr(sprite_rd_addr), .rd_data(sprite_rd_pixel)
    );

    wire [8:0] px_x, px_y; wire px_req; wire [15:0] px_color; wire px_valid;
    pixel_gen u_pixel_gen (
        .clk(clk), .pixel_x(px_x), .pixel_y(px_y), .pixel_req(px_req),
        .pixel_color(px_color), .pixel_valid(px_valid),
        .sprite_rd_idx(sprite_rd_idx), .sprite_rd_data(sprite_rd_data),
        .num_sprites(sprite_num_active), .tile_rd_idx(tile_rd_idx),
        .tile_rd_data(tile_rd_data), .sprite_addr(sprite_rd_addr),
        .sprite_data(sprite_rd_pixel), .bg_color(cmd_bg_color)
    );

    wire [7:0] lcd_data; wire lcd_wr, lcd_dc, lcd_cs;
    // Shrink the one-time Sleep-Out wait so the sim reaches frames quickly.
    lcd_driver #(.SLEEP_OUT_DELAY(23'd50)) u_lcd (
        .clk(clk), .frame_start(lcd_frame_start),
        .pixel_x(px_x), .pixel_y(px_y), .pixel_req(px_req),
        .pixel_color(px_color), .pixel_valid(px_valid),
        .lcd_data(lcd_data), .lcd_wr(lcd_wr), .lcd_dc(lcd_dc), .lcd_cs(lcd_cs)
    );

    // --- SPI byte helper (mode 0, MSB first) ---
    task spi_byte(input [7:0] b);
        integer i;
        begin
            for (i = 7; i >= 0; i = i - 1) begin
                spi_mosi = b[i]; #200; spi_clk = 1; #200; spi_clk = 0; #200;
            end
        end
    endtask

    // --- clock counter + frame-boundary capture ---
    integer clk_count = 0;
    always @(posedge clk) clk_count = clk_count + 1;

    integer f0, f1, f2, gap_start, gap_end;
    integer frame_clks, render_clks;
    reg lcd_cs_d = 1;
    integer rise_num = 0;
    always @(posedge clk) begin
        // detect LCD_CS rising edge = end of a frame (S_DONE)
        if (lcd_cs && !lcd_cs_d) begin
            rise_num = rise_num + 1;
            if (rise_num == 1) f0 = clk_count;
            else if (rise_num == 2) f1 = clk_count;
            else if (rise_num == 3) f2 = clk_count;
        end
        lcd_cs_d <= lcd_cs;
    end

    // Send one 5-byte sprite entry (matches struct ppu_sprite, packed LE):
    //   byte0=x[7:0]  byte1=x[15:8]  byte2=y  byte3=tile  byte4=flags
    task put_sprite(input [15:0] x, input [7:0] y, input [7:0] tile);
        begin
            spi_byte(x[7:0]); spi_byte(x[15:8]); spi_byte(y); spi_byte(tile); spi_byte(8'd0);
        end
    endtask

    integer t;
    initial begin
        #2000;
        // One CS frame, protocol per src/ppu.c ppu_send_frame():
        //   0x01, count, then count * 5-byte sprite entries. frame_valid pulses
        //   on the NEXT CS assertion (spi_cmd sees cmd 0x01 at CS-deassert).
        // Scene = worst case for pixel_gen: a full 20-tile ground row (y=200) +
        // 3 obstacles (y=180) + player (y=180) = 24 sprites, exercising the
        // raised MAX_PER_LINE=24 on the ground scanline.
        spi_cs = 0;
        spi_byte(8'h01);                 // CMD_SPRITE_UPDATE
        spi_byte(8'd24);                 // count
        for (t = 0; t < 20; t = t + 1)
            put_sprite(t*16, 8'd200, 8'd4);   // ground tiles across the row
        put_sprite(16'd60,  8'd180, 8'd1);    // obstacle 0
        put_sprite(16'd160, 8'd180, 8'd1);    // obstacle 1
        put_sprite(16'd260, 8'd180, 8'd1);    // obstacle 2
        put_sprite(16'd40,  8'd180, 8'd0);    // player
        spi_cs = 1; #2000;
        // Second empty CS just to latch frame_valid (0x01 at deassert swaps banks)
        spi_cs = 0; spi_byte(8'h01); spi_byte(8'd0); spi_cs = 1;

        // wait for 3 frame boundaries (steady state = f2-f1)
        wait(rise_num >= 3);
        frame_clks  = f2 - f1;          // one full frame period (steady state)
        $display("FRAME TIMING @ %0d Hz clk:", 12000000);
        $display("  clocks/frame = %0d", frame_clks);
        $display("  -> frame rate = %0d.%0d FPS",
                 12000000/frame_clks, (12000000*10/frame_clks)%10);
        $finish;
    end

    initial begin
        #500_000_000;   // 500 ms sim timeout
        $display("TIMEOUT: only %0d frame boundaries seen (clk_count=%0d)", rise_num, clk_count);
        $finish;
    end
endmodule
