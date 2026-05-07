/*
 * sprite_mem.v — Sprite memory (SPRAM wrapper)
 *
 * 32KB SPRAM stores sprite pixel data (RGB565, 2 bytes per pixel).
 * Write port: SPI uploads sprite data on boot.
 * Read port: pixel generator reads during rendering.
 *
 * SPRAM is single-port, so we time-multiplex:
 *   - During SPI upload (cs active): write has priority
 *   - During rendering: read has priority
 *
 * For simplicity, we use one 16KB SPRAM bank (16-bit wide, 14-bit addr).
 * Stores up to 8192 pixels (enough for several small sprites).
 */
module sprite_mem (
    input         clk,

    // Write port (from SPI — byte-addressed, writes 8 bits at a time)
    input  [14:0] wr_addr,
    input  [7:0]  wr_data,
    input         wr_en,

    // Read port (from pixel gen — word-addressed, reads 16-bit RGB565)
    input  [13:0] rd_addr,
    output [15:0] rd_data
);
    // Collect two bytes into one 16-bit word for SPRAM write
    reg [7:0] byte_buf = 0;
    reg       byte_phase = 0;  // 0 = low byte, 1 = high byte
    reg [13:0] word_addr = 0;
    reg [15:0] word_data = 0;
    reg        word_we = 0;

    always @(posedge clk) begin
        word_we <= 0;
        if (wr_en) begin
            if (!byte_phase) begin
                byte_buf <= wr_data;
                byte_phase <= 1;
            end else begin
                word_addr <= wr_addr[14:1];
                word_data <= {wr_data, byte_buf};  // high byte, low byte
                word_we <= 1;
                byte_phase <= 0;
            end
        end
    end

    // SPRAM instance (iCE40UP5K has 4 × 256Kbit SPRAM blocks)
    // Using one 16Kx16 block
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
