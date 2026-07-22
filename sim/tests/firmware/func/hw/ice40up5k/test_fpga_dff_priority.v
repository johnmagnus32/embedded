module dff_priority (
    input clk,
    input d,
    input enable,
    input reset,
    output reg q
);
    always @(posedge clk)
        if (enable) begin
            if (reset) q <= 0;
            else q <= d;
        end
endmodule
