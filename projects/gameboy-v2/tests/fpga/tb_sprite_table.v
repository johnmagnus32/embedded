`timescale 1ns/1ps
module tb_sprite_table;
    reg clk = 0;
    always #10 clk = ~clk;

    reg [5:0] wr_idx;
    reg [39:0] wr_data;
    reg wr_en;
    reg swap;
    reg apply_swap;
    reg [5:0] rd_idx;
    wire [39:0] rd_data;
    reg [6:0] wr_num_sprites;
    reg wr_num_en;
    wire [6:0] num_sprites;

    sprite_table uut (
        .clk(clk), .wr_idx(wr_idx), .wr_data(wr_data), .wr_en(wr_en),
        .swap(swap), .apply_swap(apply_swap), .rd_idx(rd_idx), .rd_data(rd_data),
        .wr_num_sprites(wr_num_sprites), .wr_num_en(wr_num_en),
        .num_sprites(num_sprites)
    );

    task write_sprite(input [5:0] idx, input [39:0] data);
        begin
            wr_idx = idx; wr_data = data; wr_en = 1;
            @(posedge clk); #1; wr_en = 0;
        end
    endtask

    task write_num(input [6:0] n);
        begin
            wr_num_sprites = n; wr_num_en = 1;
            @(posedge clk); #1; wr_num_en = 0;
        end
    endtask

    /* Swap is now deferred (vsync'd): `swap` latches a request, `apply_swap`
     * (a frame boundary in hardware) actually applies it. Pulse both so the
     * bank flips within this task, as the old single-signal swap did. */
    task do_swap;
        begin
            swap = 1; @(posedge clk); #1; swap = 0;
            apply_swap = 1; @(posedge clk); #1; apply_swap = 0; @(posedge clk); #1;
        end
    endtask

    task read_sprite(input [5:0] idx);
        begin
            rd_idx = idx; @(posedge clk); #1; @(posedge clk); #1;
        end
    endtask

    initial begin
        wr_idx = 0; wr_data = 0; wr_en = 0;
        swap = 0; apply_swap = 0; rd_idx = 0;
        wr_num_sprites = 0; wr_num_en = 0;
        #100;

        // ═══════════════════════════════════════════════════════════
        // Test 1: Write multiple sprites, verify all read back after swap
        // ═══════════════════════════════════════════════════════════
        write_sprite(0, 40'h11_22_33_44_55);
        write_sprite(1, 40'hAA_BB_CC_DD_EE);
        write_sprite(2, 40'h01_02_03_04_05);
        write_num(3);
        do_swap;

        read_sprite(0);
        if (rd_data !== 40'h11_22_33_44_55) begin
            $display("FAIL: sprite[0]=0x%010X, expected 0x1122334455", rd_data); $finish; end
        read_sprite(1);
        if (rd_data !== 40'hAA_BB_CC_DD_EE) begin
            $display("FAIL: sprite[1]=0x%010X, expected 0xAABBCCDDEE", rd_data); $finish; end
        read_sprite(2);
        if (rd_data !== 40'h01_02_03_04_05) begin
            $display("FAIL: sprite[2]=0x%010X, expected 0x0102030405", rd_data); $finish; end
        if (num_sprites !== 3) begin
            $display("FAIL: num_sprites=%0d, expected 3", num_sprites); $finish; end
        $display("PASS: multiple sprites write/read");

        // ═══════════════════════════════════════════════════════════
        // Test 2: Write doesn't affect read bank
        // ═══════════════════════════════════════════════════════════
        // Write to the (now inactive) write bank — shouldn't change read
        write_sprite(0, 40'hFF_FF_FF_FF_FF);
        read_sprite(0);
        if (rd_data !== 40'h11_22_33_44_55) begin
            $display("FAIL: read bank corrupted by write, got 0x%010X", rd_data); $finish; end
        $display("PASS: write doesn't affect read bank");

        // ═══════════════════════════════════════════════════════════
        // Test 3: Double swap — new data appears in read bank
        // ═══════════════════════════════════════════════════════════
        write_sprite(0, 40'hDE_AD_BE_EF_00);
        write_num(1);
        do_swap;

        read_sprite(0);
        if (rd_data !== 40'hDE_AD_BE_EF_00) begin
            $display("FAIL: after double swap, sprite[0]=0x%010X, expected 0xDEADBEEF00", rd_data); $finish; end
        if (num_sprites !== 1) begin
            $display("FAIL: num_sprites=%0d after double swap, expected 1", num_sprites); $finish; end
        $display("PASS: double swap");

        // ═══════════════════════════════════════════════════════════
        // Test 4: num_sprites isolation — reflects correct bank
        // After double swap, read bank has num=1. Write bank (old read) has num=3.
        // ═══════════════════════════════════════════════════════════
        // num_sprites should be 1 (current read bank)
        if (num_sprites !== 1) begin
            $display("FAIL: num_sprites isolation, got %0d expected 1", num_sprites); $finish; end
        // Write new num to write bank
        write_num(7);
        // Read bank num should still be 1
        if (num_sprites !== 1) begin
            $display("FAIL: num_sprites changed before swap, got %0d", num_sprites); $finish; end
        do_swap;
        // Now should be 7
        if (num_sprites !== 7) begin
            $display("FAIL: num_sprites after swap, got %0d expected 7", num_sprites); $finish; end
        $display("PASS: num_sprites isolation");

        // ═══════════════════════════════════════════════════════════
        // Test 5: Overwrite — same index twice, latest wins
        // ═══════════════════════════════════════════════════════════
        write_sprite(0, 40'h11_11_11_11_11);
        write_sprite(0, 40'h22_22_22_22_22);
        write_num(1);
        do_swap;

        read_sprite(0);
        if (rd_data !== 40'h22_22_22_22_22) begin
            $display("FAIL: overwrite, got 0x%010X expected 0x2222222222", rd_data); $finish; end
        $display("PASS: overwrite (latest wins)");

        // ═══════════════════════════════════════════════════════════
        // Test 6: Read different indices in sequence
        // ═══════════════════════════════════════════════════════════
        write_sprite(0, 40'hA0_A0_A0_A0_A0);
        write_sprite(3, 40'hB3_B3_B3_B3_B3);
        write_sprite(63, 40'hCF_CF_CF_CF_CF);
        write_num(64);
        do_swap;

        read_sprite(0);
        if (rd_data !== 40'hA0_A0_A0_A0_A0) begin
            $display("FAIL: idx 0 = 0x%010X", rd_data); $finish; end
        read_sprite(3);
        if (rd_data !== 40'hB3_B3_B3_B3_B3) begin
            $display("FAIL: idx 3 = 0x%010X", rd_data); $finish; end
        read_sprite(63);
        if (rd_data !== 40'hCF_CF_CF_CF_CF) begin
            $display("FAIL: idx 63 = 0x%010X", rd_data); $finish; end
        $display("PASS: read different indices");

        $display("PASS: tb_sprite_table");
        $finish;
    end
endmodule
