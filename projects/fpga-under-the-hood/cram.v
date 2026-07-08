/*
 * cram.v — "CRAM" is a DISTRIBUTED concept, not a memory block (ILLUSTRATIVE)
 *
 * A previous version of this file modeled CRAM as one central SRAM array whose
 * outputs fed the LUTs. THAT WAS WRONG, and this file exists now mainly to say
 * so, because the misconception is common:
 *
 *   There is no separate configuration-memory block. The configuration bits
 *   live INSIDE the fabric primitives themselves — the 16 cells in each LUT,
 *   the mode bit in each logic cell, the connect bit at each routing junction
 *   (see fabric_cell.v, where each primitive owns its own `reg`s). "CRAM" is
 *   simply the NAME for the union of all those distributed cells.
 *
 * So there is nothing to instantiate here. Configuration is not "write a CRAM
 * block, then copy it into LUTs" — it is a single act: the loader drives a
 * shared write bus (cfg_addr/cfg_data/cfg_we), and each primitive whose address
 * matches latches its own bits IN PLACE. The cell the bitstream writes is the
 * exact cell the logic then reads.
 *
 * This module is a thin pass-through kept only so fpga_top can name the bus in
 * one spot; it holds NO storage. The real "memory" is spread across every
 * fabric_cell instance.
 */
module cram_write_bus #(
    parameter ADDR_BITS = 18,
    parameter WORD_BITS = 16
)(
    // from the loader
    input  wire                 in_clk,
    input  wire                 in_we,
    input  wire [ADDR_BITS-1:0] in_addr,
    input  wire [WORD_BITS-1:0] in_data,

    // fanned out to EVERY fabric primitive's config port (they self-select by
    // matching cfg_addr). No storage lives here — the cells are in the fabric.
    output wire                 cfg_clk,
    output wire                 cfg_we,
    output wire [ADDR_BITS-1:0] cfg_addr,
    output wire [WORD_BITS-1:0] cfg_data
);
    assign cfg_clk  = in_clk;
    assign cfg_we   = in_we;
    assign cfg_addr = in_addr;
    assign cfg_data = in_data;
endmodule
