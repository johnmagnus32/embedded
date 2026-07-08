/*
 * fpga_top.v — The whole die, assembled (ILLUSTRATIVE)
 *
 * Shows the full lifecycle:  blank -> configure -> startup -> user mode.
 *
 * The key structural truth this version gets right: there is NO central CRAM
 * block feeding the fabric. Configuration is a shared WRITE BUS
 * (cfg_clk/cfg_we/cfg_addr/cfg_data) fanned out to every fabric primitive. Each
 * primitive owns its own config cells and latches them in place when the bus
 * address matches it (see fabric_cell.v). "CRAM" = the union of all those
 * distributed cells; you will not find it as a separate array anywhere here.
 *
 * The two "worlds" of an FPGA:
 *   FIXED SILICON : por, config_controller, cram_loader, cdone_pin, the bus.
 *                   Always present; runs before/around your design.
 *   PROGRAMMABLE  : the fabric primitives — dead until their config cells are
 *                   written, then they ARE your circuit.
 *
 * NOT synthesizable / not a real iCE40 — a legibility model. See README.md.
 */
module fpga_top #(
    parameter ADDR_BITS = 18,
    parameter WORD_BITS = 16
)(
    // --- Supply-valid signals (really analog POR comparisons) ---
    input  wire vcc_core_ok,
    input  wire spi_vccio1_ok,
    input  wire vpp_2v5_ok,

    // --- Dedicated configuration pins ---
    input  wire creset_b,     // config reset (active low)
    input  wire spi_ss_b,     // boot-mode strap + slave select
    input  wire spi_sck,      // config clock in
    input  wire spi_si,       // bitstream data in
    output wire cdone_pad,    // open-drain "configured OK" (needs pull-up)

    // --- A little user I/O, to show the fabric working after boot ---
    input  wire        fabric_clk,   // a fabric clock (user mode)
    input  wire [3:0]  user_in,
    output wire        user_out
);
    // =====================================================================
    // FIXED SILICON  (exists before any fabric works)
    // =====================================================================

    // 1. Power-on reset: gate everything until the monitored rails are valid.
    wire power_good;
    por u_por (
        .vcc_core_ok  (vcc_core_ok),
        .spi_vccio1_ok(spi_vccio1_ok),
        .vpp_2v5_ok   (vpp_2v5_ok),
        .power_good   (power_good)
    );

    // 2. The config controller FSM: runs the reset -> load -> startup sequence.
    wire loading, config_done, fabric_reset;
    config_controller u_cfg (
        .power_good  (power_good),
        .creset_b    (creset_b),
        .spi_ss_b    (spi_ss_b),
        .spi_sck     (spi_sck),
        .spi_si      (spi_si),
        .loading     (loading),
        .config_done (config_done),
        .fabric_reset(fabric_reset)
    );

    // 3. The loader: serial bitstream -> address/data writes during S_LOAD.
    wire [ADDR_BITS-1:0] load_addr;
    wire                 load_we;
    wire [WORD_BITS-1:0] load_data;
    cram_loader #(.ADDR_BITS(ADDR_BITS), .WORD_BITS(WORD_BITS)) u_loader (
        .spi_sck  (spi_sck),
        .spi_si   (spi_si),
        .loading  (loading),
        .cram_addr(load_addr),
        .cram_we  (load_we),
        .cram_data(load_data)
    );

    // 4. The configuration WRITE BUS. Holds no storage — just fans the loader's
    //    address/data/we out to every fabric primitive. (The bits live in the
    //    primitives; this is the wire that reaches them.)
    wire                 cfg_clk, cfg_we;
    wire [ADDR_BITS-1:0] cfg_addr;
    wire [WORD_BITS-1:0] cfg_data;
    cram_write_bus #(.ADDR_BITS(ADDR_BITS), .WORD_BITS(WORD_BITS)) u_bus (
        .in_clk (spi_sck), .in_we(load_we), .in_addr(load_addr), .in_data(load_data),
        .cfg_clk(cfg_clk), .cfg_we(cfg_we), .cfg_addr(cfg_addr), .cfg_data(cfg_data)
    );

    // 5. CDONE status pin (open-drain).
    cdone_pin u_cdone (.config_done(config_done), .cdone_pad(cdone_pad));

    // =====================================================================
    // PROGRAMMABLE FABRIC  —  an ARRAY of N identical tiles
    // =====================================================================
    //
    // A real fabric isn't a handful of hand-wired cells; it's a regular grid of
    // IDENTICAL tiles, each with the same structure and a config address derived
    // from its position. We model that with a `generate` loop: change N_CELLS to
    // 100, 1000, whatever — nothing else changes. That regularity is exactly why
    // the tools (and the bitstream addressing) scale.
    //
    // Each tile has TWO config-addressable pieces (this is the whole point that
    // configuration programs both logic AND wiring):
    //   * a logic_cell   — a LUT + FF   (2 CRAM slots: LUT bits, mode bit)
    //   * a routing_mux   — picks which routing track drives this cell's input[0]
    //                       from a small local window of neighbor outputs
    //                       (1 CRAM slot: the mux select)
    //
    // Per-tile config address map (BASE = tile_index * ADDR_PER_CELL):
    //   BASE+0 = this cell's LUT truth table
    //   BASE+1 = this cell's mode bit (comb vs registered)
    //   BASE+2 = this cell's input routing-mux select
    // A real device adds many more slots per tile (more inputs, more muxes, I/O
    // config); the shape is identical.

    localparam integer N_CELLS      = 100;  // <-- scale the fabric here
    localparam integer ADDR_PER_CELL= 3;    // LUT, mode, route (see map above)
    localparam integer ROUTE_WIN    = 8;    // each input can tap 8 nearby tracks

    // The routing "tracks": every cell's output is a track other cells can tap.
    // Index 0 is reserved as a constant-0 track (a source that means "unrouted").
    wire [N_CELLS-1:0] cell_out;            // each tile's LUT/FF output
    wire [N_CELLS-1:0] cell_in0;            // each tile's routed input[0]

    genvar i, j;
    generate
        for (i = 0; i < N_CELLS; i = i + 1) begin : tile
            // --- this tile's config base address ---
            localparam [ADDR_BITS-1:0] BASE = i * ADDR_PER_CELL;

            // --- local routing window: the ROUTE_WIN tracks this input can tap.
            // Track 0 is constant 0 ("not routed"); the rest are the outputs of
            // the preceding neighbor cells (wraps around). This gives a regular,
            // local interconnect just like a real fabric's neighbor routing.
            wire [ROUTE_WIN-1:0] window;
            assign window[0] = 1'b0;                    // the "unrouted" source
            for (j = 1; j < ROUTE_WIN; j = j + 1) begin : win
                assign window[j] = cell_out[(i + N_CELLS - j) % N_CELLS];
            end

            // --- routing mux: CRAM select picks WHICH track feeds this cell ---
            routing_mux #(
                .N_SRC(ROUTE_WIN), .ADDR_BITS(ADDR_BITS), .CFG_ADDR(BASE + 2)
            ) u_route (
                .sources(window),
                .out    (cell_in0[i]),
                .cfg_clk(cfg_clk), .cfg_we(cfg_we), .cfg_addr(cfg_addr), .cfg_data(cfg_data)
            );

            // --- the logic cell: LUT + FF, its 4 inputs are the routed neighbor
            // signal plus 3 bits of the chip's user_in (so the array actually
            // does something observable in a sim). ---
            logic_cell #(
                .ADDR_BITS(ADDR_BITS), .LUT_ADDR(BASE + 0), .MODE_ADDR(BASE + 1)
            ) u_cell (
                .clk    (fabric_clk),
                .in     ({user_in[3:1], cell_in0[i]}),
                .out    (cell_out[i]),
                .cfg_clk(cfg_clk), .cfg_we(cfg_we), .cfg_addr(cfg_addr), .cfg_data(cfg_data)
            );
        end
    endgenerate

    // The chip's user output taps the last tile in the chain (arbitrary choice;
    // a real device routes chosen cell outputs to I/O blocks — themselves
    // configured by more CRAM). Gate on fabric_reset so a blank chip is silent.
    assign user_out = fabric_reset ? 1'b0 : cell_out[N_CELLS-1];

endmodule
