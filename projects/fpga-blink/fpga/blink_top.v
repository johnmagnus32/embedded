/*
 * blink_top.v — Toggle a single external GPIO on the ice40-breakout board.
 *
 * This is the FPGA half of the "STM32 configures FPGA at boot" exercise.
 * It is deliberately minimal: the internal oscillator drives a counter and
 * one high bit of that counter drives one user I/O pin. No RGB LED driver,
 * no SB_RGBA_DRV hard IP — just a plain output you clip an external
 * LED + series resistor onto (LED anode -> pin, cathode -> resistor -> GND).
 *
 * Target: iCE40UP5K-SG48 on the ice40-breakout board (projects/ice40-breakout).
 * LED pin: FPGA package pin 2 (net IOB_6A), broken out on header H5 pin 1.
 * See blink.pcf for the pin constraint.
 */
module blink_top (
    output LED
);
    /* Internal high-frequency oscillator.
     * CLKHF_DIV="0b10" divides the 48 MHz HFOSC by 4 -> 12 MHz, matching the
     * clock the counter's blink-rate comment assumes. */
    wire clk;
    SB_HFOSC #(.CLKHF_DIV("0b10")) osc (
        .CLKHFEN(1'b1),
        .CLKHFPU(1'b1),
        .CLKHF(clk)
    );

    /* Pure counter logic lives in blink.v (testable in simulation). */
    blink u_blink (
        .clk(clk),
        .led(LED)
    );
endmodule
