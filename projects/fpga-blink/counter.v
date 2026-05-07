/*
 * counter.v — 24-bit counter with LED outputs
 *
 * Pure logic, no hard IP. Testable in any simulator.
 */
module counter (
    input  clk,
    output led_r,
    output led_g,
    output led_b
);
    reg [23:0] cnt = 0;
    always @(posedge clk)
        cnt <= cnt + 1;

    assign led_r = cnt[23];
    assign led_g = cnt[22];
    assign led_b = cnt[21];
endmodule
