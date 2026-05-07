/*
 * blink.v — Top-level: hard IP wrapper + counter logic
 *
 * This file is for synthesis only. It instantiates iCE40-specific
 * hard IP (oscillator, LED driver) and wires them to the testable
 * counter module.
 */
module blink (
    output LED_R,
    output LED_G,
    output LED_B
);
    wire clk;
    SB_HFOSC #(.CLKHF_DIV("0b10")) osc (
        .CLKHFEN(1'b1),
        .CLKHFPU(1'b1),
        .CLKHF(clk)
    );

    wire r, g, b;
    counter u_counter (
        .clk(clk),
        .led_r(r),
        .led_g(g),
        .led_b(b)
    );

    SB_RGBA_DRV #(
        .CURRENT_MODE("0b1"),
        .RGB0_CURRENT("0b000001"),
        .RGB1_CURRENT("0b000001"),
        .RGB2_CURRENT("0b000001")
    ) rgb_drv (
        .CURREN(1'b1),
        .RGBLEDEN(1'b1),
        .RGB0PWM(r),
        .RGB1PWM(g),
        .RGB2PWM(b),
        .RGB0(LED_R),
        .RGB1(LED_G),
        .RGB2(LED_B)
    );
endmodule
