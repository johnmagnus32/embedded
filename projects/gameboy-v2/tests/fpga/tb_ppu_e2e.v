`timescale 1ns/1ps
/*
 * tb_ppu_e2e.v — End-to-end PPU integration test.
 * Wires all modules together and tests the full SPI → LCD pipeline.
 */
module tb_ppu_e2e;
    reg clk = 0;
    always #10 clk = ~clk;

    reg spi_clk = 0, spi_mosi = 0, spi_cs = 1;

    // Internal wiring (all modules connected)
    wire [5:0] cmd_sprite_wr_idx;
    wire [39:0] cmd_sprite_wr_data;
    wire cmd_sprite_wr_en;
    wire [6:0] cmd_sprite_num;
    wire cmd_sprite_num_en;
    wire cmd_frame_valid;
    wire [7:0] cmd_tile_wr_idx;
    wire [31:0] cmd_tile_wr_data;
    wire cmd_tile_wr_en;
    wire [14:0] cmd_mem_addr;
    wire [7:0] cmd_mem_wdata;
    wire cmd_mem_we;
    wire [15:0] cmd_bg_color;

    spi_cmd u_spi (
        .clk(clk), .spi_clk(spi_clk), .spi_mosi(spi_mosi), .spi_cs(spi_cs),
        .sprite_wr_idx(cmd_sprite_wr_idx), .sprite_wr_data(cmd_sprite_wr_data),
        .sprite_wr_en(cmd_sprite_wr_en), .sprite_num(cmd_sprite_num),
        .sprite_num_en(cmd_sprite_num_en), .frame_valid(cmd_frame_valid),
        .tile_wr_idx(cmd_tile_wr_idx), .tile_wr_data(cmd_tile_wr_data),
        .tile_wr_en(cmd_tile_wr_en), .mem_addr(cmd_mem_addr),
        .mem_wdata(cmd_mem_wdata), .mem_we(cmd_mem_we), .bg_color(cmd_bg_color)
    );

    wire [5:0] sprite_rd_idx;
    wire [39:0] sprite_rd_data;
    wire [6:0] num_sprites;

    wire lcd_frame_start;
    sprite_table u_sprites (
        .clk(clk), .wr_idx(cmd_sprite_wr_idx), .wr_data(cmd_sprite_wr_data),
        .wr_en(cmd_sprite_wr_en), .swap(cmd_frame_valid), .apply_swap(lcd_frame_start),
        .rd_idx(sprite_rd_idx), .rd_data(sprite_rd_data),
        .wr_num_sprites(cmd_sprite_num), .wr_num_en(cmd_sprite_num_en),
        .num_sprites(num_sprites)
    );

    wire [7:0] tile_rd_idx;
    wire [31:0] tile_rd_data;

    tile_table u_tiles (
        .clk(clk), .wr_idx(cmd_tile_wr_idx), .wr_data(cmd_tile_wr_data),
        .wr_en(cmd_tile_wr_en), .rd_idx(tile_rd_idx), .rd_data(tile_rd_data)
    );

    wire [13:0] pixel_sprite_addr;
    wire [15:0] pixel_sprite_data;

    sprite_mem u_mem (
        .clk(clk), .wr_addr(cmd_mem_addr), .wr_data(cmd_mem_wdata),
        .wr_en(cmd_mem_we), .rd_addr(pixel_sprite_addr), .rd_data(pixel_sprite_data)
    );

    wire [8:0] pixel_x, pixel_y;
    wire pixel_req;
    wire [15:0] pixel_color;
    wire pixel_valid;

    pixel_gen u_pixel (
        .clk(clk), .pixel_x(pixel_x), .pixel_y(pixel_y),
        .pixel_req(pixel_req), .pixel_color(pixel_color), .pixel_valid(pixel_valid),
        .sprite_rd_idx(sprite_rd_idx), .sprite_rd_data(sprite_rd_data),
        .num_sprites(num_sprites), .tile_rd_idx(tile_rd_idx),
        .tile_rd_data(tile_rd_data), .sprite_addr(pixel_sprite_addr),
        .sprite_data(pixel_sprite_data), .bg_color(cmd_bg_color)
    );

    wire [7:0] lcd_d;
    wire lcd_wr, lcd_dc, lcd_cs;

    lcd_driver u_lcd (
        .clk(clk), .frame_start(lcd_frame_start),
        .pixel_x(pixel_x), .pixel_y(pixel_y),
        .pixel_req(pixel_req), .pixel_color(pixel_color), .pixel_valid(pixel_valid),
        .lcd_data(lcd_d), .lcd_wr(lcd_wr), .lcd_dc(lcd_dc), .lcd_cs(lcd_cs)
    );

    // SPI helper
    task spi_send_byte(input [7:0] byte);
        integer i;
        for (i = 7; i >= 0; i = i - 1) begin
            spi_mosi = byte[i];
            #100; spi_clk = 1;
            #100; spi_clk = 0;
        end
    endtask

    // Monitor pixel_valid output
    integer pixel_count = 0;
    integer non_bg_count = 0;
    reg [15:0] last_non_bg_color = 0;

    always @(posedge clk) begin
        if (pixel_valid) begin
            pixel_count = pixel_count + 1;
            if (pixel_color != cmd_bg_color && pixel_color != 16'h0000) begin
                non_bg_count = non_bg_count + 1;
                last_non_bg_color = pixel_color;
            end
        end
    end

    initial begin
        #500;

        // ═══════════════════════════════════════════════════════════
        // Test 1: Background color only (no sprites)
        // ═══════════════════════════════════════════════════════════
        spi_cs = 0; #200;
        spi_send_byte(8'h03);  // CMD_BG_COLOR
        spi_send_byte(8'hF8);
        spi_send_byte(8'h00);  // bg = 0xF800 (red)
        #200; spi_cs = 1; #1000;

        // Send 0-sprite frame to trigger rendering
        spi_cs = 0; #200;
        spi_send_byte(8'h01);
        spi_send_byte(8'h00);
        #200; spi_cs = 1;

        // Wait for some pixels
        wait(pixel_count > 100);
        #100;

        if (non_bg_count > 0) begin
            $display("FAIL test1: got %0d non-bg pixels with 0 sprites", non_bg_count); $finish; end
        $display("PASS: bg only (0 sprites, %0d pixels rendered)", pixel_count);

        // ═══════════════════════════════════════════════════════════
        // Test 2: Upload pixels + tile + sprite → sprite pixels appear
        // ═══════════════════════════════════════════════════════════

        // Reset counters (wait for current frame to finish)
        wait(lcd_cs == 1);
        pixel_count = 0;
        non_bg_count = 0;

        // Change bg to something that won't match sprite pixels
        spi_cs = 0; #200;
        spi_send_byte(8'h03);
        spi_send_byte(8'h12);
        spi_send_byte(8'h34);  // bg = 0x1234
        #200; spi_cs = 1; #1000;

        // Upload 4 pixels (2x2 tile) to SPRAM at addr 0
        spi_cs = 0; #200;
        spi_send_byte(8'h02);  // CMD_PIXEL_UPLOAD
        spi_send_byte(8'h00); spi_send_byte(8'h00);  // addr = 0
        spi_send_byte(8'h00); spi_send_byte(8'hF8);  // pixel 0 = 0xF800
        spi_send_byte(8'hE0); spi_send_byte(8'h07);  // pixel 1 = 0x07E0
        spi_send_byte(8'h1F); spi_send_byte(8'h00);  // pixel 2 = 0x001F
        spi_send_byte(8'hFF); spi_send_byte(8'hFF);  // pixel 3 = 0xFFFF
        #200; spi_cs = 1; #1000;

        // Upload tile 0: base=0, width_shift=1 (2px), height=2
        spi_cs = 0; #200;
        spi_send_byte(8'h04);
        spi_send_byte(8'h01);  // 1 tile
        spi_send_byte(8'h00); spi_send_byte(8'h00);  // base = 0
        spi_send_byte(8'h01);  // width_shift = 1 (2^1 = 2px)
        spi_send_byte(8'h02);  // height = 2
        #200; spi_cs = 1; #1000;

        // Send sprite frame: 1 sprite at (5, 5), tile 0
        spi_cs = 0; #200;
        spi_send_byte(8'h01);  // CMD_SPRITE
        spi_send_byte(8'h01);  // 1 sprite
        spi_send_byte(8'd5);   // x_lo
        spi_send_byte(8'h00);  // x_hi
        spi_send_byte(8'd5);   // y
        spi_send_byte(8'h00);  // tile = 0
        spi_send_byte(8'h00);  // flags
        #200; spi_cs = 1;

        // Wait for rendering past sprite area
        wait(pixel_count > 240 * 8);
        #1000;

        if (non_bg_count < 4) begin
            $display("FAIL test2: only %0d non-bg pixels, expected >= 4", non_bg_count); $finish; end
        $display("PASS: sprite rendering (%0d non-bg pixels, last=0x%04X)", non_bg_count, last_non_bg_color);

        // ═══════════════════════════════════════════════════════════
        // Test 3: Change background color mid-stream
        // ═══════════════════════════════════════════════════════════
        wait(lcd_cs == 1);
        pixel_count = 0;
        non_bg_count = 0;

        // Change bg to green
        spi_cs = 0; #200;
        spi_send_byte(8'h03);
        spi_send_byte(8'h07);
        spi_send_byte(8'hE0);  // bg = 0x07E0
        #200; spi_cs = 1; #1000;

        // Send another sprite frame (same sprite)
        spi_cs = 0; #200;
        spi_send_byte(8'h01);
        spi_send_byte(8'h01);
        spi_send_byte(8'd5); spi_send_byte(8'h00);
        spi_send_byte(8'd5); spi_send_byte(8'h00); spi_send_byte(8'h00);
        #200; spi_cs = 1;

        wait(pixel_count > 240 * 8);
        #1000;

        // Sprite pixels should still be non-bg (0x07E0 is now bg, but sprite has 0xF800 etc.)
        // Note: pixel 1 of the sprite IS 0x07E0 (green) which now matches bg — so it won't count
        if (non_bg_count < 2) begin
            $display("FAIL test3: only %0d non-bg after bg change", non_bg_count); $finish; end
        $display("PASS: bg color change (%0d non-bg pixels)", non_bg_count);

        // ═══════════════════════════════════════════════════════════
        // Test 4: Multiple sprites on same scanline
        // ═══════════════════════════════════════════════════════════
        wait(lcd_cs == 1);
        pixel_count = 0;
        non_bg_count = 0;

        // bg = 0x1234 (already set)
        // Send 2 sprites: one at (5,5) and one at (20,5), both tile 0
        spi_cs = 0; #200;
        spi_send_byte(8'h01);  // CMD_SPRITE
        spi_send_byte(8'h02);  // 2 sprites
        // Sprite 0: (5,5)
        spi_send_byte(8'd5); spi_send_byte(8'h00);
        spi_send_byte(8'd5); spi_send_byte(8'h00); spi_send_byte(8'h00);
        // Sprite 1: (20,5)
        spi_send_byte(8'd20); spi_send_byte(8'h00);
        spi_send_byte(8'd5); spi_send_byte(8'h00); spi_send_byte(8'h00);
        #200; spi_cs = 1;

        wait(pixel_count > 240 * 8);
        #1000;

        // Each sprite is 2x2 = 4 pixels, 2 sprites = 8 non-bg pixels
        // (some may read as 0x0000 from uninitialized SPRAM areas if tile base shifted)
        if (non_bg_count < 6) begin
            $display("FAIL test4: only %0d non-bg, expected >= 6 (2 sprites)", non_bg_count); $finish; end
        $display("PASS: multiple sprites (%0d non-bg pixels)", non_bg_count);

        // ═══════════════════════════════════════════════════════════
        // Test 5: Frame update — new positions replace old
        // ═══════════════════════════════════════════════════════════
        wait(lcd_cs == 1);
        pixel_count = 0;
        non_bg_count = 0;

        // Move sprite to (100, 100) — different position
        spi_cs = 0; #200;
        spi_send_byte(8'h01);
        spi_send_byte(8'h01);  // 1 sprite
        spi_send_byte(8'd100); spi_send_byte(8'h00);
        spi_send_byte(8'd100); spi_send_byte(8'h00); spi_send_byte(8'h00);
        #200; spi_cs = 1;

        // Wait past row 102 (sprite at y=100, height=2)
        wait(pixel_count > 240 * 103);
        #1000;

        // Should have 3 non-bg pixels (one sprite pixel matches bg 0x07E0)
        if (non_bg_count !== 3) begin
            $display("FAIL test5: %0d non-bg, expected 3 (frame update)", non_bg_count); $finish; end
        $display("PASS: frame update (sprite moved, %0d non-bg)", non_bg_count);

        // ═══════════════════════════════════════════════════════════
        // Test 6: Fully transparent sprite — all pixels 0xF81F
        // ═══════════════════════════════════════════════════════════
        wait(lcd_cs == 1);
        pixel_count = 0;
        non_bg_count = 0;

        // Upload transparent pixels to SPRAM at addr 0x0010 (tile 1)
        spi_cs = 0; #200;
        spi_send_byte(8'h02);
        spi_send_byte(8'h00); spi_send_byte(8'h10);  // addr = 0x0010
        spi_send_byte(8'h1F); spi_send_byte(8'hF8);  // 0xF81F (transparent)
        spi_send_byte(8'h1F); spi_send_byte(8'hF8);  // 0xF81F
        spi_send_byte(8'h1F); spi_send_byte(8'hF8);  // 0xF81F
        spi_send_byte(8'h1F); spi_send_byte(8'hF8);  // 0xF81F
        #200; spi_cs = 1; #1000;

        // Upload tile 1: base=0x0008 (word addr 8 = byte addr 0x10), width_shift=1, height=2
        spi_cs = 0; #200;
        spi_send_byte(8'h04);
        spi_send_byte(8'h02);  // 2 tiles (overwrite both)
        spi_send_byte(8'h00); spi_send_byte(8'h00); spi_send_byte(8'h01); spi_send_byte(8'h02);  // tile 0 (unchanged)
        spi_send_byte(8'h00); spi_send_byte(8'h08); spi_send_byte(8'h01); spi_send_byte(8'h02);  // tile 1: base=8
        #200; spi_cs = 1; #1000;

        // Send sprite using tile 1 (transparent)
        spi_cs = 0; #200;
        spi_send_byte(8'h01);
        spi_send_byte(8'h01);
        spi_send_byte(8'd50); spi_send_byte(8'h00);
        spi_send_byte(8'd50); spi_send_byte(8'h01); spi_send_byte(8'h00);  // tile 1
        #200; spi_cs = 1;

        wait(pixel_count > 240 * 53);
        #1000;

        if (non_bg_count !== 0) begin
            $display("FAIL test6: %0d non-bg, expected 0 (transparent sprite)", non_bg_count); $finish; end
        $display("PASS: transparent sprite (0 non-bg pixels)");

        // ═══════════════════════════════════════════════════════════
        // Test 7: Two sprites with different tiles
        // ═══════════════════════════════════════════════════════════
        wait(lcd_cs == 1);
        pixel_count = 0;
        non_bg_count = 0;

        // Sprite 0 uses tile 0 (has colored pixels), sprite 1 uses tile 1 (transparent)
        spi_cs = 0; #200;
        spi_send_byte(8'h01);
        spi_send_byte(8'h02);  // 2 sprites
        spi_send_byte(8'd10); spi_send_byte(8'h00);
        spi_send_byte(8'd10); spi_send_byte(8'h00); spi_send_byte(8'h00);  // tile 0
        spi_send_byte(8'd30); spi_send_byte(8'h00);
        spi_send_byte(8'd10); spi_send_byte(8'h01); spi_send_byte(8'h00);  // tile 1
        #200; spi_cs = 1;

        wait(pixel_count > 240 * 13);
        #1000;

        // Only sprite 0 (tile 0) should produce non-bg pixels
        // Sprite 1 (tile 1) is transparent
        // One sprite pixel (0x07E0) matches bg, so 3 non-bg
        if (non_bg_count !== 3) begin
            $display("FAIL test7: %0d non-bg, expected 3 (tile0=visible, tile1=transparent)", non_bg_count); $finish; end
        $display("PASS: different tiles (%0d non-bg from tile 0 only)", non_bg_count);

        $display("PASS: tb_ppu_e2e");
        $finish;
    end

    initial begin
        #500000000;
        $display("TIMEOUT: pixels=%0d non_bg=%0d", pixel_count, non_bg_count);
        $finish;
    end
endmodule
