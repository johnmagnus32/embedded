/*
 * sim_cells.v — Simulation models for iCE40 hard IP
 * Only used during simulation (iverilog), not synthesis.
 */
module SB_SPRAM256KA (
    input  [13:0] ADDRESS,
    input  [15:0] DATAIN,
    input  [3:0]  MASKWREN,
    input         WREN,
    input         CHIPSELECT,
    input         CLOCK,
    input         STANDBY,
    input         SLEEP,
    input         POWEROFF,
    output reg [15:0] DATAOUT
);
    reg [15:0] mem [0:16383];

    always @(posedge CLOCK) begin
        if (CHIPSELECT) begin
            if (WREN) begin
                if (MASKWREN[0]) mem[ADDRESS][3:0]   <= DATAIN[3:0];
                if (MASKWREN[1]) mem[ADDRESS][7:4]   <= DATAIN[7:4];
                if (MASKWREN[2]) mem[ADDRESS][11:8]  <= DATAIN[11:8];
                if (MASKWREN[3]) mem[ADDRESS][15:12] <= DATAIN[15:12];
            end
            DATAOUT <= mem[ADDRESS];
        end
    end
endmodule
