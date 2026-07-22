module counter (input clk, input rst, output [3:0] count);
    reg [3:0] cnt = 0;
    always @(posedge clk)
        if (rst) cnt <= 0;
        else cnt <= cnt + 1;
    assign count = cnt;
endmodule
