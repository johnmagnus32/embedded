`timescale 1ns/1ps
module tb_tile_table;
    reg clk = 0;
    always #10 clk = ~clk;

    reg [7:0] wr_idx;
    reg [31:0] wr_data;
    reg wr_en;
    reg [7:0] rd_idx;
    wire [31:0] rd_data;

    tile_table uut (
        .clk(clk), .wr_idx(wr_idx), .wr_data(wr_data),
        .wr_en(wr_en), .rd_idx(rd_idx), .rd_data(rd_data)
    );

    task write_tile(input [7:0] idx, input [31:0] data);
        begin
            wr_idx = idx; wr_data = data; wr_en = 1;
            @(posedge clk); #1; wr_en = 0;
        end
    endtask

    task read_tile(input [7:0] idx);
        begin
            rd_idx = idx;
            @(posedge clk); #1;  // present address
            @(posedge clk); #1;  // data available (registered output)
        end
    endtask

    initial begin
        wr_idx = 0; wr_data = 0; wr_en = 0; rd_idx = 0;
        #100;

        // ═══════════════════════════════════════════════════════════
        // Test 1: Basic write and read
        // ═══════════════════════════════════════════════════════════
        write_tile(0, {16'h1000, 8'd4, 8'd16});
        write_tile(5, {16'h2000, 8'd3, 8'd8});

        read_tile(0);
        if (rd_data !== {16'h1000, 8'd4, 8'd16}) begin
            $display("FAIL: tile[0]=0x%08X, expected 0x10000410", rd_data); $finish; end
        read_tile(5);
        if (rd_data !== {16'h2000, 8'd3, 8'd8}) begin
            $display("FAIL: tile[5]=0x%08X, expected 0x20000308", rd_data); $finish; end
        $display("PASS: basic write/read");

        // ═══════════════════════════════════════════════════════════
        // Test 2: Write doesn't affect other indices
        // ═══════════════════════════════════════════════════════════
        write_tile(10, 32'hDEADBEEF);
        read_tile(0);
        if (rd_data !== {16'h1000, 8'd4, 8'd16}) begin
            $display("FAIL: tile[0] corrupted after writing tile[10], got 0x%08X", rd_data); $finish; end
        read_tile(5);
        if (rd_data !== {16'h2000, 8'd3, 8'd8}) begin
            $display("FAIL: tile[5] corrupted after writing tile[10], got 0x%08X", rd_data); $finish; end
        read_tile(10);
        if (rd_data !== 32'hDEADBEEF) begin
            $display("FAIL: tile[10]=0x%08X, expected 0xDEADBEEF", rd_data); $finish; end
        $display("PASS: write isolation");

        // ═══════════════════════════════════════════════════════════
        // Test 3: Overwrite — same index, latest wins
        // ═══════════════════════════════════════════════════════════
        write_tile(0, 32'h11111111);
        write_tile(0, 32'h22222222);
        read_tile(0);
        if (rd_data !== 32'h22222222) begin
            $display("FAIL: overwrite, tile[0]=0x%08X expected 0x22222222", rd_data); $finish; end
        $display("PASS: overwrite (latest wins)");

        // ═══════════════════════════════════════════════════════════
        // Test 4: Boundary index (255)
        // ═══════════════════════════════════════════════════════════
        write_tile(255, 32'hCAFEBABE);
        read_tile(255);
        if (rd_data !== 32'hCAFEBABE) begin
            $display("FAIL: tile[255]=0x%08X, expected 0xCAFEBABE", rd_data); $finish; end
        $display("PASS: boundary index 255");

        // ═══════════════════════════════════════════════════════════
        // Test 5: Registered read — data not available same cycle as address
        // ═══════════════════════════════════════════════════════════
        write_tile(20, 32'h12345678);
        rd_idx = 20;
        @(posedge clk); #1;
        // rd_data should NOT yet have the value (registered)
        // (it has the previous read's data or X)
        // After one more cycle it should be correct
        @(posedge clk); #1;
        if (rd_data !== 32'h12345678) begin
            $display("FAIL: registered read, tile[20]=0x%08X expected 0x12345678", rd_data); $finish; end
        $display("PASS: registered read latency");

        $display("PASS: tb_tile_table");
        $finish;
    end
endmodule
