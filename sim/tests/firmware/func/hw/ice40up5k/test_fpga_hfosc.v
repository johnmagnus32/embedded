module hfosc_counter (output [3:0] count);
    wire clk;
    SB_HFOSC #(.CLKHF_DIV("0b10")) osc (
        .CLKHFEN(1'b1), .CLKHFPU(1'b1), .CLKHF(clk)
    );
    reg [3:0] cnt = 0;
    always @(posedge clk) cnt <= cnt + 1;
    assign count = cnt;
endmodule
