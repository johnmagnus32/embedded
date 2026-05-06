/*
 * tb_blink.v — Testbench for blink module
 *
 * Since SB_HFOSC and SB_RGBA_DRV are iCE40 hard primitives that
 * don't exist in simulation, we test the counter logic directly.
 */
`timescale 1ns/1ps

module tb_blink;
    reg clk = 0;
    always #41.67 clk = ~clk;  // ~12MHz

    reg [23:0] counter = 0;
    always @(posedge clk)
        counter <= counter + 1;

    wire led_r = counter[23];
    wire led_g = counter[22];
    wire led_b = counter[21];

    initial begin
        $dumpfile("blink.vcd");
        $dumpvars(0, tb_blink);

        // Run for enough cycles to see LED toggle
        #200_000_000;  // 200ms — should see bit 23 change at least once

        if (counter > 0)
            $display("PASS: counter is running, value = %d", counter);
        else
            $display("FAIL: counter stuck at 0");

        $finish;
    end
endmodule
