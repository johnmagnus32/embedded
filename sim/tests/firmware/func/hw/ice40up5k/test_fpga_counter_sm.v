module counter_sm (
    input clk,
    input start,
    output reg [8:0] count,
    output reg [7:0] out_data,
    output reg out_wr
);
    localparam S_IDLE=0, S_REQ=1, S_WAIT=2, S_HI=3, S_LO=4;
    reg [2:0] state = S_IDLE;
    reg [15:0] color = 16'h867D;

    always @(posedge clk) begin
        out_wr <= 0;
        case (state)
        S_IDLE: if (start) begin count <= 0; state <= S_REQ; end
        S_REQ: state <= S_WAIT;
        S_WAIT: state <= S_HI;
        S_HI: begin out_data <= color[15:8]; out_wr <= 1; state <= S_LO; end
        S_LO: begin out_data <= color[7:0]; out_wr <= 1; count <= count + 1; state <= S_REQ; end
        endcase
    end
endmodule
