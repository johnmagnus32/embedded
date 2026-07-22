/*
 * sprite_mem.v — Sprite memory (SPRAM wrapper)
 *
 * Write port: SPI uploads byte-at-a-time. Pairs bytes into 16-bit words.
 * Read port: pixel generator reads 16-bit RGB565 pixels.
 */
module sprite_mem (
    input         clk,
    input  [14:0] wr_addr,
    input  [7:0]  wr_data,
    input         wr_en,
    input  [13:0] rd_addr,
    output [15:0] rd_data
);
    reg [7:0] byte_buf = 0;
    reg       byte_phase = 0;
    reg [13:0] word_addr = 0;
    reg [15:0] word_data = 0;
    reg        word_we = 0;

    always @(posedge clk) begin
        word_we <= 0;
        if (wr_en) begin
            if (!byte_phase) begin
                byte_buf <= wr_data;
                word_addr <= wr_addr[14:1];
                byte_phase <= 1;
            end else begin
                word_data <= {wr_data, byte_buf};
                word_we <= 1;
                byte_phase <= 0;
            end
        end
    end

    wire [15:0] spram_out;
    wire [13:0] addr_mux = word_we ? word_addr : rd_addr;

    SB_SPRAM256KA spram (
        .ADDRESS(addr_mux),
        .DATAIN(word_data),
        .MASKWREN(4'b1111),
        .WREN(word_we),
        .CHIPSELECT(1'b1),
        .CLOCK(clk),
        .STANDBY(1'b0),
        .SLEEP(1'b0),
        .POWEROFF(1'b1),
        .DATAOUT(spram_out)
    );

    assign rd_data = spram_out;
endmodule
