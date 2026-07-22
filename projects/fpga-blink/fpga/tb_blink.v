/*
 * tb_blink.v — Testbench for the blink counter module
 *
 * Instantiates the REAL blink module and verifies:
 *   1. Counter starts at 0
 *   2. Counter increments each clock
 *   3. led output reflects counter bit 22
 */
`timescale 1ns/1ps

module tb_blink;
    reg clk = 0;
    always #41.67 clk = ~clk;  // ~12MHz

    wire led;

    blink uut (
        .clk(clk),
        .led(led)
    );

    integer failures = 0;

    initial begin
        $dumpfile("blink.vcd");
        $dumpvars(0, tb_blink);

        // Test 1: counter starts at 0
        #1;
        if (uut.cnt !== 0) begin
            $display("FAIL: initial value = %d, expected 0", uut.cnt);
            failures = failures + 1;
        end

        // Test 2: after 1 clock, counter = 1
        @(posedge clk); #1;
        if (uut.cnt !== 1) begin
            $display("FAIL: after 1 clk, cnt = %d, expected 1", uut.cnt);
            failures = failures + 1;
        end

        // Test 3: after 8 total clocks, counter = 8
        repeat(7) @(posedge clk);
        #1;
        if (uut.cnt !== 8) begin
            $display("FAIL: after 8 clks, cnt = %d, expected 8", uut.cnt);
            failures = failures + 1;
        end

        // Test 4: led reflects bit 22 — high when bit 22 set
        force uut.cnt = 24'h400000;   // bit 22 = 1
        #1;
        if (led !== 1) begin
            $display("FAIL: cnt=0x400000, led should be 1, got %b", led);
            failures = failures + 1;
        end

        force uut.cnt = 24'h000000;   // bit 22 = 0
        #1;
        if (led !== 0) begin
            $display("FAIL: cnt=0x000000, led should be 0, got %b", led);
            failures = failures + 1;
        end

        release uut.cnt;

        // Summary
        if (failures == 0)
            $display("PASS: all tests passed");
        else
            $display("FAILED: %0d test(s) failed", failures);

        $finish;
    end
endmodule
