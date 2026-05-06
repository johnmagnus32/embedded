/*
 * blink.v — Blink the RGB LED on iCEBreaker using iCE40UP5K
 *
 * Uses the internal 48MHz oscillator divided down to ~1Hz.
 * The iCE40UP5K has a hard RGB LED driver (SB_RGBA_DRV) for the
 * onboard LED — you can't drive it with regular I/O.
 */
module blink (
    output LED_R,
    output LED_G,
    output LED_B
);

    // Internal 48MHz oscillator (no external crystal needed)
    wire clk;
    SB_HFOSC #(.CLKHF_DIV("0b10")) osc (  // 48MHz / 4 = 12MHz
        .CLKHFEN(1'b1),
        .CLKHFPU(1'b1),
        .CLKHF(clk)
    );

    // 24-bit counter: at 12MHz, bit 23 toggles at ~1.4Hz
    reg [23:0] counter = 0;
    always @(posedge clk)
        counter <= counter + 1;

    // RGB LED driver (active-low accent current source)
    SB_RGBA_DRV #(
        .CURRENT_MODE("0b1"),       // half current mode
        .RGB0_CURRENT("0b000001"),  // ~4mA
        .RGB1_CURRENT("0b000001"),
        .RGB2_CURRENT("0b000001")
    ) rgb_drv (
        .CURREN(1'b1),
        .RGBLEDEN(1'b1),
        .RGB0PWM(counter[23]),      // Red
        .RGB1PWM(counter[22]),      // Green
        .RGB2PWM(counter[21]),      // Blue
        .RGB0(LED_R),
        .RGB1(LED_G),
        .RGB2(LED_B)
    );

endmodule
