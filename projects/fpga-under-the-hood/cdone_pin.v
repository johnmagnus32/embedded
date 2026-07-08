/*
 * cdone_pin.v — The CDONE status output (ILLUSTRATIVE)
 *
 * CDONE tells the outside world "configuration finished, your design is now
 * running." The subtle part — and the reason the status LED on a real board is
 * wired the way it is — is that CDONE is OPEN-DRAIN:
 *
 *   - During configuration: the FPGA actively pulls CDONE LOW.
 *   - When done: the FPGA RELEASES the pin (high-impedance). It does NOT drive
 *     it high — an external pull-up resistor is what pulls it up to logic 1.
 *
 * Consequences you can see on hardware:
 *   - You must add an external pull-up (or CDONE floats when released).
 *   - An LED wired (3.3V -> R -> LED -> CDONE) lights while CDONE is LOW, i.e.
 *     WHILE CONFIGURING, and goes dark when done. "Lit = OK" needs a transistor
 *     because open-drain can only sink current, never source it.
 *
 * A real chip's CDONE also has a weak INTERNAL pull-up; modeled in the comment
 * only. Open-drain behavior itself (drive-low / release) is real.
 */
module cdone_pin (
    input  wire config_done,   // 1 once startup completes
    output wire cdone_pad      // the physical pin (needs external pull-up)
);
    // 1'bz = "let go" (high-impedance). The external pull-up decides the level.
    // 1'b0 = actively held low.
    assign cdone_pad = config_done ? 1'bz    // released -> pull-up takes it HIGH
                                   : 1'b0;   // configuring -> held LOW
endmodule
