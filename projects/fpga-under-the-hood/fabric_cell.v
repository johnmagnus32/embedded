/*
 * fabric_cell.v — Fabric primitives AND their configuration cells (ILLUSTRATIVE)
 *
 * READ THIS FIRST — it is the whole point of the project:
 *
 * There is NO separate "config memory" that gets copied into the LUTs. The SRAM
 * cells holding a LUT's truth table ARE configuration RAM. "CRAM" is just the
 * name for the ENTIRE population of these cells scattered across the fabric:
 * the 16 bits inside every LUT, the mode bit in every cell, the connect bit at
 * every routing junction. They are not stored anywhere else and not duplicated.
 *
 * So each primitive below OWNS its config cells as internal `reg`s and writes
 * them directly from a shared configuration bus at boot. The exact same cell
 * the bitstream writes is the cell that drives the logic. That is the real
 * relationship between "the LUTs" and "CRAM": they are the same silicon.
 *
 * NOT synthesizable — this DESCRIBES fixed iCE40 fabric silicon. The shared,
 * address-matched write bus is a behavioral stand-in for real frame/column
 * programming (see cram.v and cram_loader.v for that nuance).
 */

/* ------------------------------------------------------------------------
 * lut4 — a 4-input Look-Up Table.
 *
 * A LUT is a tiny SRAM: the 4 inputs form an address selecting 1 of 16 stored
 * bits, and that bit is the output. Those 16 cells ARE this LUT's CRAM bits —
 * the bitstream writes them in place; the logic reads the same cells.
 *
 *   mem = 16'h8000 -> 4-input AND     mem = 16'h6996 -> 4-input XOR
 * ---------------------------------------------------------------------- */
module lut4 #(
    parameter integer         ADDR_BITS = 18,
    parameter [ADDR_BITS-1:0] CFG_ADDR  = 0   // this LUT's slot in the CRAM map
)(
    // --- fabric side (USER MODE) ---
    input  wire [3:0]           in,           // logic inputs = address into table
    output wire                 out,          // the selected stored bit

    // --- configuration write bus (CONFIG time; shared by every cell) ---
    input  wire                 cfg_clk,
    input  wire                 cfg_we,
    input  wire [ADDR_BITS-1:0] cfg_addr,
    input  wire [15:0]          cfg_data
);
    // THESE 16 CELLS ARE CRAM. There is no other copy. The bitstream writes
    // `mem` during config; the fabric reads `mem` forever after.
    reg [15:0] mem;

    always @(posedge cfg_clk)
        if (cfg_we && cfg_addr == CFG_ADDR)   // this LUT is the addressed target
            mem <= cfg_data;                  // truth table written IN PLACE

    assign out = mem[in];                     // ...same cells produce the logic
endmodule


/* ------------------------------------------------------------------------
 * logic_cell — one LUT plus its flip-flop, plus the cell's own mode bit.
 *
 * The "combinational vs registered" choice is ALSO a CRAM cell (`ff_enable`),
 * living right here in the cell — not in some central table. It listens on the
 * same config bus at its own address.
 * ---------------------------------------------------------------------- */
module logic_cell #(
    parameter integer         ADDR_BITS = 18,
    parameter [ADDR_BITS-1:0] LUT_ADDR  = 0,  // address of this cell's LUT bits
    parameter [ADDR_BITS-1:0] MODE_ADDR = 1   // address of this cell's mode bit
)(
    // fabric side
    input  wire                 clk,          // fabric clock (user mode)
    input  wire [3:0]           in,
    output wire                 out,
    // config bus (shared)
    input  wire                 cfg_clk,
    input  wire                 cfg_we,
    input  wire [ADDR_BITS-1:0] cfg_addr,
    input  wire [15:0]          cfg_data
);
    // The LUT owns its 16 CRAM cells internally.
    wire lut_out;
    lut4 #(.ADDR_BITS(ADDR_BITS), .CFG_ADDR(LUT_ADDR)) u_lut (
        .in(in), .out(lut_out),
        .cfg_clk(cfg_clk), .cfg_we(cfg_we), .cfg_addr(cfg_addr), .cfg_data(cfg_data)
    );

    // The mode select is another CRAM cell, physically in this logic cell.
    reg ff_enable;                            // 1 = registered, 0 = combinational
    always @(posedge cfg_clk)
        if (cfg_we && cfg_addr == MODE_ADDR)
            ff_enable <= cfg_data[0];

    reg ff_q;
    always @(posedge clk) ff_q <= lut_out;
    assign out = ff_enable ? ff_q : lut_out;
endmodule


/* ------------------------------------------------------------------------
 * routing_switch — how config bits define the WIRING.
 *
 * Every wire junction in the fabric has one CRAM cell (`connect`) deciding
 * whether the two wires join. Your "netlist" is nothing but thousands of these
 * connect-bits. This cell, too, lives at the junction and is written over the
 * same config bus — it is part of CRAM.
 * ---------------------------------------------------------------------- */
module routing_switch #(
    parameter integer         ADDR_BITS = 18,
    parameter [ADDR_BITS-1:0] CFG_ADDR  = 0
)(
    input  wire                 from_wire,
    output wire                 to_wire,
    // config bus (shared)
    input  wire                 cfg_clk,
    input  wire                 cfg_we,
    input  wire [ADDR_BITS-1:0] cfg_addr,
    input  wire [15:0]          cfg_data
);
    reg connect;                              // one CRAM cell, at this junction
    always @(posedge cfg_clk)
        if (cfg_we && cfg_addr == CFG_ADDR)
            connect <= cfg_data[0];
    assign to_wire = connect ? from_wire : 1'bz;
endmodule


/* ------------------------------------------------------------------------
 * routing_mux — the realistic generalization of routing_switch.
 *
 * A real fabric input isn't fed by a single on/off switch; it can be driven
 * from ONE OF MANY possible sources (other cells' outputs, routing tracks,
 * primary inputs). That choice is a MUX, and the mux's select is — you guessed
 * it — its own CRAM cells. Physically a real mux is a tree of the pass-transistor
 * switches above, one config bit each; a behavioral mux with a stored `sel` is
 * the clean, equivalent abstraction.
 *
 * This is the primitive that makes "wire cell A's output to cell B's input"
 * scale: every cell input owns one of these, selecting its source by CRAM.
 * ---------------------------------------------------------------------- */
module routing_mux #(
    parameter integer         N_SRC     = 8,   // number of possible sources
    parameter integer         ADDR_BITS = 18,
    parameter [ADDR_BITS-1:0] CFG_ADDR  = 0
)(
    input  wire [N_SRC-1:0]     sources,       // the routing tracks this input can tap
    output wire                 out,           // the chosen source
    // config bus (shared)
    input  wire                 cfg_clk,
    input  wire                 cfg_we,
    input  wire [ADDR_BITS-1:0] cfg_addr,
    input  wire [15:0]          cfg_data
);
    localparam integer SEL_BITS = (N_SRC <= 1) ? 1 : $clog2(N_SRC);

    // The select IS this mux's CRAM: which of the N_SRC tracks drives the input.
    reg [SEL_BITS-1:0] sel;
    always @(posedge cfg_clk)
        if (cfg_we && cfg_addr == CFG_ADDR)
            sel <= cfg_data[SEL_BITS-1:0];

    assign out = sources[sel];                 // configured connection
endmodule
