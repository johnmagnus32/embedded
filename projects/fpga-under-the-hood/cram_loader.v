/*
 * cram_loader.v — Serial bitstream -> CRAM datapath (ILLUSTRATIVE)
 *
 * This is the workhorse of the "LOAD" phase. In slave-SPI configuration the
 * external processor (your STM32) clocks the bitstream in one bit at a time on
 * SPI_SI, using SPI_SCK as the clock. This block:
 *   1. shifts each incoming bit into a word (MSB-first, SPI mode 0),
 *   2. once a full word ("frame") is assembled, writes it to the next CRAM
 *      address and advances the address counter.
 *
 * Repeat for the whole bitstream and CRAM fills up, cell by cell — and the
 * fabric morphs into your design as it goes.
 *
 * NOT real: frame = 16 bits and a flat incrementing address are simplifications.
 * A real iCE40 bitstream carries a sync word, command opcodes, and per-column
 * frame addressing (see Project IceStorm for the reverse-engineered format).
 * The datapath *shape* (shift register + address counter + write pulse) is real.
 */
module cram_loader #(
    parameter ADDR_BITS = 18,
    parameter WORD_BITS = 16
)(
    input  wire                 spi_sck,   // config clock in (slave mode)
    input  wire                 spi_si,    // one bitstream bit per rising edge
    input  wire                 loading,   // high only during the LOAD state

    output reg  [ADDR_BITS-1:0] cram_addr, // which frame to write next
    output reg                  cram_we,   // write pulse (1 per completed frame)
    output reg  [WORD_BITS-1:0] cram_data  // the assembled frame
);
    reg [$clog2(WORD_BITS)-1:0] bit_count;

    always @(posedge spi_sck) begin
        if (!loading) begin
            bit_count <= 0;
            cram_we   <= 1'b0;
            cram_addr <= 0;
        end else begin
            // 1. Shift the incoming serial bit into the frame word, MSB-first.
            cram_data <= {cram_data[WORD_BITS-2:0], spi_si};

            if (bit_count == WORD_BITS-1) begin
                // 2. A full frame has arrived: commit it and step the address.
                cram_we   <= 1'b1;            // pulse write into CRAM
                cram_addr <= cram_addr + 1'b1;
                bit_count <= 0;
            end else begin
                cram_we   <= 1'b0;
                bit_count <= bit_count + 1'b1;
            end
        end
    end
endmodule
