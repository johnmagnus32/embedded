/*
 * config_controller.v — The configuration state machine (ILLUSTRATIVE)
 *
 * This is the "brain" of the boot process: fixed silicon that runs the moment
 * power is valid, loads the bitstream into CRAM, then hands the fabric over to
 * your design. Its states map one-to-one onto the pin sequence an external
 * processor drives (e.g. your STM32's fpga_load_bitstream()):
 *
 *   S_RESET        <- CRESET_B held low  (clear config)
 *   S_SAMPLE_MODE  <- CRESET_B rising    (sample SPI_SS_B: master vs slave)
 *   S_CLEAR_CRAM   <- ~1200us internal erase
 *   S_LOAD         <- clock the bitstream in on SPI_SCK/SPI_SI
 *   S_STARTUP      <- >=49 wake-up clocks, release the fabric
 *   S_USER_MODE    <- CDONE high; your design runs
 *
 * IMPORTANT — real vs. model:
 *   - This is DIGITAL hard IP. On the real chip it was very likely authored in
 *     Verilog/VHDL by Lattice, then pushed through an ASIC flow (synthesis ->
 *     standard cells -> photomask -> fixed transistors). Same language as your
 *     designs, but permanent silicon rather than a bitstream. You (the FPGA
 *     user) cannot change it.
 *   - The exact state encodings, counter widths, and thresholds below are
 *     invented for clarity. The SEQUENCE and its pin meanings are real.
 *   - Clocking off spi_sck is a slave-mode simplification. In master mode the
 *     controller generates its own clock to read an external flash instead.
 */
module config_controller (
    // Supply / reset
    input  wire power_good,     // from POR: rails valid
    input  wire creset_b,       // external config-reset pin (active LOW)

    // SPI slave-config pins
    input  wire spi_ss_b,       // boot-mode strap (sampled) + slave select
    input  wire spi_sck,        // config clock in
    input  wire spi_si,         // bitstream data in

    // Internal control
    output reg  loading,        // 1 during S_LOAD (enables the cram_loader)
    output reg  config_done,    // 1 in user mode -> drives CDONE
    output reg  fabric_reset    // 1 holds the fabric in reset during config
);
    localparam S_WAIT_POR    = 3'd0,  // wait for supplies valid
               S_RESET       = 3'd1,  // CRESET low: prepare to clear CRAM
               S_SAMPLE_MODE = 3'd2,  // CRESET rising edge: sample the strap
               S_CLEAR_CRAM  = 3'd3,  // internal erase (~1200us on real HW)
               S_LOAD        = 3'd4,  // shift bitstream into CRAM
               S_STARTUP     = 3'd5,  // wake-up clocks, release fabric
               S_USER_MODE   = 3'd6;  // done — fabric runs your design

    reg [2:0] state;
    reg       is_slave;          // sampled boot mode: 1 = slave, 0 = master
    reg [1:0] creset_sync;       // edge-detect CRESET_B in this clock domain

    // --- teaching stand-ins for real timing/count conditions ---
    // On real silicon these are internal timers/counters. Here they are just
    // wires you could poke in a testbench to advance the FSM.
    reg cram_cleared;            // pretend the ~1200us erase finished
    reg bitstream_complete;      // pretend the last frame arrived
    reg startup_clocks_done;     // pretend >=49 wake-up clocks elapsed

    // The controller runs on the config clock. Async-reset on loss of power.
    always @(posedge spi_sck or negedge power_good) begin
        if (!power_good) begin
            // No valid power -> everything held blank/off. This is the POR grip.
            state        <= S_WAIT_POR;
            loading      <= 1'b0;
            config_done  <= 1'b0;      // CDONE low = "not configured"
            fabric_reset <= 1'b1;      // fabric held OFF
            is_slave     <= 1'b0;
            creset_sync  <= 2'b00;
        end else begin
            creset_sync <= {creset_sync[0], creset_b};  // for rising-edge detect

            case (state)
                // Power is good; wait here until CRESET is asserted (low) to
                // kick off a configuration cycle.
                S_WAIT_POR: begin
                    loading      <= 1'b0;
                    config_done  <= 1'b0;
                    fabric_reset <= 1'b1;
                    if (creset_b == 1'b0)
                        state <= S_RESET;
                end

                // Held in reset. Leave when CRESET is released (goes high).
                S_RESET:
                    if (creset_b == 1'b1)
                        state <= S_SAMPLE_MODE;

                // THE decisive instant: at CRESET's rising edge the chip latches
                // SPI_SS_B to choose its boot mode. Low = slave (be fed by a
                // processor); high = master (self-load from external flash).
                S_SAMPLE_MODE: begin
                    is_slave <= (spi_ss_b == 1'b0);
                    state    <= S_CLEAR_CRAM;
                end

                // Erase old CRAM before accepting new data (~1200us on real HW).
                S_CLEAR_CRAM:
                    if (cram_cleared)
                        state <= S_LOAD;

                // Stream the bitstream in. cram_loader does the actual shifting
                // and CRAM writes while 'loading' is high.
                S_LOAD: begin
                    loading <= 1'b1;
                    if (bitstream_complete) begin
                        loading <= 1'b0;
                        state   <= S_STARTUP;
                    end
                end

                // Wake-up phase: extra clocks let the internal startup sequencer
                // bring the fabric out of reset in an orderly way.
                S_STARTUP:
                    if (startup_clocks_done) begin
                        config_done  <= 1'b1;   // -> CDONE released high
                        fabric_reset <= 1'b0;   // fabric now runs your design
                        state        <= S_USER_MODE;
                    end

                // Configured. The SPI config pins now become ordinary user GPIO.
                S_USER_MODE:
                    ; // stay here until power loss or a new CRESET pulse

                default: state <= S_WAIT_POR;
            endcase
        end
    end
endmodule
