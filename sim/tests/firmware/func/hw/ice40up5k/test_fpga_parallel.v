module parallel_out (
    input clk,
    input [7:0] din,
    input load,
    output [7:0] dout
);
    reg [7:0] reg_out = 0;
    always @(posedge clk)
        if (load) reg_out <= din;
    assign dout = reg_out;
endmodule
