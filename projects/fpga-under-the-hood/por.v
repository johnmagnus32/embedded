/*
 * por.v — Power-On Reset detector (ILLUSTRATIVE model of an ANALOG block)
 *
 * The POR watches the supply rails and only allows configuration to begin once
 * they are valid. This is why the datasheet says configuration "waits for VCC"
 * and why an out-of-order power-up (like deriving 1.2V from 3.3V) is tolerated:
 * the POR simply holds everything in reset until the monitored rails are good,
 * regardless of the order they arrived in.
 *
 * IMPORTANT — this is NOT how the real block was authored. The real POR is an
 * ANALOG circuit: bandgap references and voltage comparators drawn at the
 * transistor level and verified in SPICE, not Verilog. Voltage thresholds
 * ("is VCC above 1.1V?") are not something HDL can express. This module only
 * models the block's *digital effect* (a clean power_good signal) so the rest
 * of the model has something to gate on.
 *
 * On iCE40 only three rails are POR-monitored: VCC (core), SPI_VCCIO1, VPP_2V5.
 */
module por (
    // In real silicon these are analog voltage comparisons, not logic inputs.
    // Modeled as "this rail has crossed its valid threshold" booleans.
    input  wire vcc_core_ok,     // 1.2V core above threshold
    input  wire spi_vccio1_ok,   // 3.3V bank-1 I/O supply above threshold
    input  wire vpp_2v5_ok,      // NVCM/operating supply above threshold

    output wire power_good        // 1 = safe to start configuration
);
    // All three monitored rails must be valid. Order does not matter — this is
    // a level check, so whichever rail is last to arrive gates the release.
    assign power_good = vcc_core_ok & spi_vccio1_ok & vpp_2v5_ok;
endmodule
