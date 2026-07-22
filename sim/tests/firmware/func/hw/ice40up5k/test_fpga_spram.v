module spram_test (
    input clk,
    input [3:0] addr,
    input [7:0] wdata,
    input we,
    output [7:0] rdata
);
    wire [15:0] rdata_full;
    SB_SPRAM256KA spram (
        .ADDRESS({10'b0, addr}),
        .DATAIN({8'b0, wdata}),
        .DATAOUT(rdata_full),
        .MASKWREN(4'b1111),
        .WREN(we),
        .CHIPSELECT(1'b1),
        .CLOCK(clk),
        .STANDBY(1'b0),
        .SLEEP(1'b0),
        .POWEROFF(1'b1)
    );
    assign rdata = rdata_full[7:0];
endmodule
