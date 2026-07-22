module spi_slave (
    input clk,
    input spi_clk,
    input spi_mosi,
    input spi_cs,
    output [7:0] reg_out
);
    reg [2:0] spi_clk_sync;
    reg [1:0] spi_cs_sync;
    always @(posedge clk) begin
        spi_clk_sync <= {spi_clk_sync[1:0], spi_clk};
        spi_cs_sync <= {spi_cs_sync[0], spi_cs};
    end
    wire spi_clk_rise = (spi_clk_sync[2:1] == 2'b01);
    wire cs_active = ~spi_cs_sync[1];

    reg [7:0] shift_reg;
    reg [2:0] bit_cnt;
    reg [7:0] received;

    always @(posedge clk) begin
        if (!cs_active) begin
            bit_cnt <= 0;
        end else if (spi_clk_rise) begin
            shift_reg <= {shift_reg[6:0], spi_mosi};
            bit_cnt <= bit_cnt + 1;
            if (bit_cnt == 7)
                received <= {shift_reg[6:0], spi_mosi};
        end
    end

    assign reg_out = received;
endmodule
