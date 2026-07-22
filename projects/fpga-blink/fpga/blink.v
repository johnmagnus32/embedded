/*
 * blink.v — Pure counter logic, no hard IP. Testable in any simulator.
 *
 * Drives a single output high/low from one bit of a free-running counter.
 * At 12 MHz, bit 22 toggles at 12e6 / 2^23 ~= 1.4 Hz (a visible blink).
 * The hard IP (oscillator) and pin mapping live in blink_top.v / blink.pcf.
 */
module blink (
    input  clk,
    output led
);
    reg [23:0] cnt = 0;
    always @(posedge clk)
        cnt <= cnt + 1;

    assign led = cnt[22];
endmodule
