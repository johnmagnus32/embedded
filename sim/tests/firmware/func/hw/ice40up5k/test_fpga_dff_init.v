module dff_init (
    input clk,
    input d,
    input set,
    output reg q_plain,
    output reg q_with_set
);
    always @(posedge clk) q_plain <= d;
    always @(posedge clk)
        if (set) q_with_set <= 1;
        else q_with_set <= d;
endmodule
