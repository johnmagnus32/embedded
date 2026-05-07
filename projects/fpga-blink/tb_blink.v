/*
 * tb_blink.v — Testbench for the counter module
 *
 * Instantiates the REAL counter module and verifies:
 *   1. Counter starts at 0
 *   2. Counter increments each clock
 *   3. LED outputs reflect the correct counter bits
 */
`timescale 1ns/1ps

module tb_blink;
    reg clk = 0;
    always #41.67 clk = ~clk;  // ~12MHz

    wire led_r, led_g, led_b;

    counter uut (
        .clk(clk),
        .led_r(led_r),
        .led_g(led_g),
        .led_b(led_b)
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

        // Test 4: LED outputs reflect correct bits
        // Force counter to known value: bits 23,22,21 = 1,1,1
        force uut.cnt = 24'hE00000;
        #1;
        if (led_r !== 1 || led_g !== 1 || led_b !== 1) begin
            $display("FAIL: cnt=0xE00000, LEDs should be 1,1,1, got %b,%b,%b",
                     led_r, led_g, led_b);
            failures = failures + 1;
        end

        // Force: bits 23,22,21 = 0,0,0
        force uut.cnt = 24'h000000;
        #1;
        if (led_r !== 0 || led_g !== 0 || led_b !== 0) begin
            $display("FAIL: cnt=0x000000, LEDs should be 0,0,0, got %b,%b,%b",
                     led_r, led_g, led_b);
            failures = failures + 1;
        end

        // Force: bit 23=1, 22=0, 21=1
        force uut.cnt = 24'hA00000;
        #1;
        if (led_r !== 1 || led_g !== 0 || led_b !== 1) begin
            $display("FAIL: cnt=0xA00000, LEDs should be 1,0,1, got %b,%b,%b",
                     led_r, led_g, led_b);
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
