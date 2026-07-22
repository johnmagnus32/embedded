module bram_test (
    input clk,
    input we,
    input [7:0] addr,
    input [7:0] wdata,
    output [7:0] rdata
);
    reg [7:0] mem [0:255];
    reg [7:0] rdata_reg;
    always @(posedge clk) begin
        if (we) mem[addr] <= wdata;
        rdata_reg <= mem[addr];
    end
    assign rdata = rdata_reg;
endmodule
