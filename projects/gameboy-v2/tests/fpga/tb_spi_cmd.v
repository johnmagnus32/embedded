`timescale 1ns/1ps
module tb_spi_cmd;
    reg clk = 0;
    always #10 clk = ~clk;

    reg spi_clk = 0;
    reg spi_mosi = 0;
    reg spi_cs = 1;

    wire [5:0] sprite_wr_idx;
    wire [39:0] sprite_wr_data;
    wire sprite_wr_en;
    wire [6:0] sprite_num;
    wire sprite_num_en;
    wire frame_valid;
    wire [7:0] tile_wr_idx;
    wire [31:0] tile_wr_data;
    wire tile_wr_en;
    wire [14:0] mem_addr;
    wire [7:0] mem_wdata;
    wire mem_we;
    wire [15:0] bg_color;

    spi_cmd uut (
        .clk(clk), .spi_clk(spi_clk), .spi_mosi(spi_mosi), .spi_cs(spi_cs),
        .sprite_wr_idx(sprite_wr_idx), .sprite_wr_data(sprite_wr_data),
        .sprite_wr_en(sprite_wr_en), .sprite_num(sprite_num),
        .sprite_num_en(sprite_num_en), .frame_valid(frame_valid),
        .tile_wr_idx(tile_wr_idx), .tile_wr_data(tile_wr_data),
        .tile_wr_en(tile_wr_en), .mem_addr(mem_addr),
        .mem_wdata(mem_wdata), .mem_we(mem_we), .bg_color(bg_color)
    );

    task spi_send_byte(input [7:0] byte);
        integer i;
        for (i = 7; i >= 0; i = i - 1) begin
            spi_mosi = byte[i];
            #100; spi_clk = 1;
            #100; spi_clk = 0;
        end
    endtask

    // Capture pulsed outputs
    reg saw_sprite_num_en = 0;
    reg [6:0] captured_sprite_num;
    reg saw_sprite_wr_en = 0;
    reg [5:0] captured_sprite_wr_idx;
    reg [39:0] captured_sprite_wr_data;
    reg saw_frame_valid = 0;
    reg saw_tile_wr_en = 0;
    reg [7:0] captured_tile_wr_idx;
    reg [31:0] captured_tile_wr_data;
    integer mem_we_count = 0;
    reg [7:0] captured_mem_wdata [0:15];
    reg [14:0] captured_mem_addr [0:15];

    always @(posedge clk) begin
        if (sprite_num_en) begin
            saw_sprite_num_en <= 1;
            captured_sprite_num <= sprite_num;
        end
        if (sprite_wr_en) begin
            saw_sprite_wr_en <= 1;
            captured_sprite_wr_idx <= sprite_wr_idx;
            captured_sprite_wr_data <= sprite_wr_data;
        end
        if (frame_valid)
            saw_frame_valid <= 1;
        if (tile_wr_en) begin
            saw_tile_wr_en <= 1;
            captured_tile_wr_idx <= tile_wr_idx;
            captured_tile_wr_data <= tile_wr_data;
        end
        if (mem_we && mem_we_count < 16) begin
            captured_mem_wdata[mem_we_count] <= mem_wdata;
            captured_mem_addr[mem_we_count] <= mem_addr;
            mem_we_count <= mem_we_count + 1;
        end
    end

    task reset_captures;
        begin
            saw_sprite_num_en = 0;
            saw_sprite_wr_en = 0;
            saw_frame_valid = 0;
            saw_tile_wr_en = 0;
            mem_we_count = 0;
        end
    endtask

    initial begin
        #200;

        // ═══════════════════════════════════════════════════════════
        // Test 1: CMD_BG_COLOR (0x03)
        // ═══════════════════════════════════════════════════════════
        reset_captures;
        spi_cs = 0; #200;
        spi_send_byte(8'h03);
        spi_send_byte(8'hAB);
        spi_send_byte(8'hCD);
        #200; spi_cs = 1;
        repeat (20) @(posedge clk);

        if (bg_color !== 16'hABCD) begin
            $display("FAIL: bg_color=0x%04X, expected 0xABCD", bg_color); $finish; end
        // No other outputs should have fired
        if (saw_sprite_num_en || saw_sprite_wr_en || saw_tile_wr_en || mem_we_count > 0) begin
            $display("FAIL: unexpected output during CMD_BG_COLOR"); $finish; end
        $display("PASS: CMD_BG_COLOR");

        // ═══════════════════════════════════════════════════════════
        // Test 2: CMD_SPRITE (0x01) with 2 sprites
        // ═══════════════════════════════════════════════════════════
        #500;
        reset_captures;
        spi_cs = 0; #200;
        spi_send_byte(8'h01);  // cmd
        spi_send_byte(8'h02);  // num_sprites = 2
        // Sprite 0: x_lo=0x50, x_hi=0x00, y=0x30, tile=0x02, flags=0x01
        spi_send_byte(8'h50);
        spi_send_byte(8'h00);
        spi_send_byte(8'h30);
        spi_send_byte(8'h02);
        spi_send_byte(8'h01);
        // Sprite 1: x_lo=0xA0, x_hi=0x01, y=0x60, tile=0x05, flags=0x00
        spi_send_byte(8'hA0);
        spi_send_byte(8'h01);
        spi_send_byte(8'h60);
        spi_send_byte(8'h05);
        spi_send_byte(8'h00);
        #200; spi_cs = 1;
        repeat (20) @(posedge clk);

        // Check sprite_num
        if (!saw_sprite_num_en) begin
            $display("FAIL: sprite_num_en never fired"); $finish; end
        if (captured_sprite_num !== 7'd2) begin
            $display("FAIL: sprite_num=%0d, expected 2", captured_sprite_num); $finish; end

        // Check sprite_wr_en fired (at least for last sprite)
        if (!saw_sprite_wr_en) begin
            $display("FAIL: sprite_wr_en never fired"); $finish; end

        // Check frame_valid
        if (!saw_frame_valid) begin
            $display("FAIL: frame_valid never fired"); $finish; end

        $display("PASS: CMD_SPRITE (num=%0d, wr_en=%0d, frame_valid=%0d)",
                 captured_sprite_num, saw_sprite_wr_en, saw_frame_valid);

        // ═══════════════════════════════════════════════════════════
        // Test 3: CMD_SPRITE with 0 sprites (should still frame_valid)
        // ═══════════════════════════════════════════════════════════
        #500;
        reset_captures;
        spi_cs = 0; #200;
        spi_send_byte(8'h01);
        spi_send_byte(8'h00);  // 0 sprites
        #200; spi_cs = 1;
        repeat (20) @(posedge clk);

        if (!saw_frame_valid) begin
            $display("FAIL: frame_valid not fired for 0-sprite frame"); $finish; end
        if (saw_sprite_wr_en) begin
            $display("FAIL: sprite_wr_en fired for 0-sprite frame"); $finish; end
        $display("PASS: CMD_SPRITE (0 sprites, frame_valid=%0d)", saw_frame_valid);

        // ═══════════════════════════════════════════════════════════
        // Test 4: CMD_PIXEL_UPLOAD (0x02) — 4 bytes to addr 0x0100
        // ═══════════════════════════════════════════════════════════
        #500;
        reset_captures;
        spi_cs = 0; #200;
        spi_send_byte(8'h02);
        spi_send_byte(8'h01);  // addr hi = 0x01
        spi_send_byte(8'h00);  // addr lo = 0x00 → addr = 0x0100
        spi_send_byte(8'hDE);
        spi_send_byte(8'hAD);
        spi_send_byte(8'hBE);
        spi_send_byte(8'hEF);
        #200; spi_cs = 1;
        repeat (20) @(posedge clk);

        if (mem_we_count !== 4) begin
            $display("FAIL: mem_we fired %0d times, expected 4", mem_we_count); $finish; end
        if (captured_mem_wdata[0] !== 8'hDE) begin
            $display("FAIL: mem_wdata[0]=0x%02X, expected 0xDE", captured_mem_wdata[0]); $finish; end
        if (captured_mem_wdata[1] !== 8'hAD) begin
            $display("FAIL: mem_wdata[1]=0x%02X, expected 0xAD", captured_mem_wdata[1]); $finish; end
        if (captured_mem_wdata[2] !== 8'hBE) begin
            $display("FAIL: mem_wdata[2]=0x%02X, expected 0xBE", captured_mem_wdata[2]); $finish; end
        if (captured_mem_wdata[3] !== 8'hEF) begin
            $display("FAIL: mem_wdata[3]=0x%02X, expected 0xEF", captured_mem_wdata[3]); $finish; end
        // Addresses should be 0x0100, 0x0101, 0x0102, 0x0103
        if (captured_mem_addr[0] !== 15'h0100) begin
            $display("FAIL: mem_addr[0]=0x%04X, expected 0x0100", captured_mem_addr[0]); $finish; end
        if (captured_mem_addr[3] !== 15'h0103) begin
            $display("FAIL: mem_addr[3]=0x%04X, expected 0x0103", captured_mem_addr[3]); $finish; end
        // No sprite/tile outputs should fire
        if (saw_sprite_wr_en || saw_tile_wr_en || saw_frame_valid) begin
            $display("FAIL: unexpected output during CMD_PIXEL_UPLOAD"); $finish; end
        $display("PASS: CMD_PIXEL_UPLOAD (4 bytes at 0x0100)");

        // Test 4b: CMD_PIXEL_UPLOAD — 10 bytes (exposes byte_idx wrap bug at byte 9)
        #500;
        reset_captures;
        spi_cs = 0; #200;
        spi_send_byte(8'h02);
        spi_send_byte(8'h00);  // addr hi
        spi_send_byte(8'h00);  // addr lo = 0x0000
        spi_send_byte(8'h01); spi_send_byte(8'h02); spi_send_byte(8'h03); spi_send_byte(8'h04);
        spi_send_byte(8'h05); spi_send_byte(8'h06); spi_send_byte(8'h07); spi_send_byte(8'h08);
        spi_send_byte(8'h09); spi_send_byte(8'h0A);
        #200; spi_cs = 1;
        repeat (20) @(posedge clk);

        if (mem_we_count !== 10) begin
            $display("FAIL: 10-byte upload: mem_we=%0d, expected 10", mem_we_count); $finish; end
        if (captured_mem_addr[7] !== 15'h0007) begin
            $display("FAIL: 10-byte addr[7]=0x%04X, expected 0x0007", captured_mem_addr[7]); $finish; end
        if (captured_mem_addr[8] !== 15'h0008) begin
            $display("FAIL: 10-byte addr[8]=0x%04X, expected 0x0008 (byte_idx wrap!)", captured_mem_addr[8]); $finish; end
        if (captured_mem_addr[9] !== 15'h0009) begin
            $display("FAIL: 10-byte addr[9]=0x%04X, expected 0x0009", captured_mem_addr[9]); $finish; end
        $display("PASS: CMD_PIXEL_UPLOAD (10 bytes, addr[8]=0x%04X)", captured_mem_addr[8]);

        // ═══════════════════════════════════════════════════════════
        // Test 5: CMD_TILE_TABLE (0x04) — 2 entries
        // ═══════════════════════════════════════════════════════════
        #500;
        reset_captures;
        spi_cs = 0; #200;
        spi_send_byte(8'h04);
        spi_send_byte(8'h02);  // 2 entries
        // Entry 0: base=0x0080, width_shift=4, height=16
        spi_send_byte(8'h00); spi_send_byte(8'h80);
        spi_send_byte(8'h04); spi_send_byte(8'h10);
        // Entry 1: base=0x0200, width_shift=3, height=8
        spi_send_byte(8'h02); spi_send_byte(8'h00);
        spi_send_byte(8'h03); spi_send_byte(8'h08);
        #200; spi_cs = 1;
        repeat (20) @(posedge clk);

        if (!saw_tile_wr_en) begin
            $display("FAIL: tile_wr_en never fired"); $finish; end
        // Last captured should be entry 1
        if (captured_tile_wr_data !== 32'h02000308) begin
            $display("FAIL: tile_wr_data=0x%08X, expected 0x02000308", captured_tile_wr_data); $finish; end
        if (captured_tile_wr_idx !== 8'd1) begin
            $display("FAIL: tile_wr_idx=%0d, expected 1", captured_tile_wr_idx); $finish; end
        // No sprite/mem outputs should fire
        if (saw_sprite_wr_en || saw_frame_valid || mem_we_count > 0) begin
            $display("FAIL: unexpected output during CMD_TILE_TABLE"); $finish; end
        $display("PASS: CMD_TILE_TABLE (2 entries)");

        $display("PASS: tb_spi_cmd");
        $finish;
    end
endmodule
