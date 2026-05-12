/*
 * spi_cmd.v — SPI command decoder (generic sprite protocol)
 *
 * Commands:
 *   0x01: Update sprite table (N entries × 5 bytes)
 *   0x02: Upload tile pixels to SPRAM
 *   0x03: Set background color
 *   0x04: Upload tile table (N entries × 4 bytes)
 *
 * Frame valid: asserted on CS rising edge after cmd 0x01.
 */
module spi_cmd (
    input         clk,
    input         spi_clk,
    input         spi_mosi,
    input         spi_cs,
    // Sprite table write
    output reg [5:0]  sprite_wr_idx,
    output reg [39:0] sprite_wr_data,
    output reg        sprite_wr_en,
    output reg [6:0]  sprite_num,
    output reg        sprite_num_en,
    output reg        frame_valid,
    // Tile table write
    output reg [7:0]  tile_wr_idx,
    output reg [31:0] tile_wr_data,
    output reg        tile_wr_en,
    // SPRAM write
    output reg [14:0] mem_addr,
    output reg [7:0]  mem_wdata,
    output reg        mem_we,
    // Background color
    output reg [15:0] bg_color
);
    // SPI shift register (synchronized to clk domain)
    reg [2:0] spi_clk_sync;
    reg [1:0] spi_cs_sync;
    always @(posedge clk) begin
        spi_clk_sync <= {spi_clk_sync[1:0], spi_clk};
        spi_cs_sync <= {spi_cs_sync[0], spi_cs};
    end
    wire spi_clk_rise = (spi_clk_sync[2:1] == 2'b01);
    wire cs_active = ~spi_cs_sync[1];
    wire cs_deassert = (spi_cs_sync == 2'b01);

    reg [7:0] shift_reg;
    reg [2:0] bit_cnt;
    reg byte_ready;
    reg [7:0] rx_byte;

    always @(posedge clk) begin
        byte_ready <= 0;
        if (!cs_active) begin
            bit_cnt <= 0;
        end else if (spi_clk_rise) begin
            shift_reg <= {shift_reg[6:0], spi_mosi};
            bit_cnt <= bit_cnt + 1;
            if (bit_cnt == 7) begin
                rx_byte <= {shift_reg[6:0], spi_mosi};
                byte_ready <= 1;
            end
        end
    end

    // Command state machine
    localparam S_CMD = 0, S_SPRITE_NUM = 1, S_SPRITE_DATA = 2,
               S_MEM_ADDR = 3, S_MEM_DATA = 4,
               S_BG_COLOR = 5, S_TILE_NUM = 6, S_TILE_DATA = 7;

    reg [2:0] state = S_CMD;
    reg [7:0] cmd;
    reg [2:0] byte_idx;     // byte within current entry
    reg [7:0] sprites_remaining;
    reg [7:0] tiles_remaining;
    reg [39:0] sprite_accum;
    reg [31:0] tile_accum;
    reg [14:0] mem_addr_reg;

    initial bg_color = 16'h867D;  // default sky blue

    always @(posedge clk) begin
        sprite_wr_en <= 0;
        sprite_num_en <= 0;
        tile_wr_en <= 0;
        mem_we <= 0;
        frame_valid <= 0;

        if (cs_deassert) begin
            if (cmd == 8'h01) frame_valid <= 1;
            state <= S_CMD;
        end

        if (byte_ready && cs_active) begin
            case (state)
            S_CMD: begin
                cmd <= rx_byte;
                case (rx_byte)
                    8'h01: state <= S_SPRITE_NUM;
                    8'h02: begin state <= S_MEM_ADDR; byte_idx <= 0; end
                    8'h03: begin state <= S_BG_COLOR; byte_idx <= 0; end
                    8'h04: state <= S_TILE_NUM;
                endcase
            end

            // --- Cmd 0x01: Sprite table ---
            S_SPRITE_NUM: begin
                sprites_remaining <= rx_byte;
                sprite_num <= rx_byte[6:0];
                sprite_num_en <= 1;
                sprite_wr_idx <= 0;
                byte_idx <= 0;
                state <= (rx_byte == 0) ? S_CMD : S_SPRITE_DATA;
            end
            S_SPRITE_DATA: begin
                sprite_accum <= {sprite_accum[31:0], rx_byte};
                byte_idx <= byte_idx + 1;
                if (byte_idx == 4) begin
                    // All 5 bytes received for this sprite
                    sprite_wr_data <= {sprite_accum[31:0], rx_byte};
                    sprite_wr_en <= 1;
                    sprite_wr_idx <= sprite_wr_idx + 1;
                    sprites_remaining <= sprites_remaining - 1;
                    byte_idx <= 0;
                    if (sprites_remaining == 1) state <= S_CMD;
                end
            end

            // --- Cmd 0x02: SPRAM pixel upload ---
            S_MEM_ADDR: begin
                if (byte_idx == 0) begin
                    mem_addr_reg[14:8] <= rx_byte[6:0];
                    byte_idx <= 1;
                end else begin
                    mem_addr_reg[7:0] <= rx_byte;
                    mem_addr <= {mem_addr_reg[14:8], rx_byte};
                    state <= S_MEM_DATA;
                end
            end
            S_MEM_DATA: begin
                mem_wdata <= rx_byte;
                mem_addr <= mem_addr;
                mem_we <= 1;
                mem_addr <= mem_addr + 1;
            end

            // --- Cmd 0x03: Background color ---
            S_BG_COLOR: begin
                if (byte_idx == 0) begin
                    bg_color[15:8] <= rx_byte;
                    byte_idx <= 1;
                end else begin
                    bg_color[7:0] <= rx_byte;
                    state <= S_CMD;
                end
            end

            // --- Cmd 0x04: Tile table ---
            S_TILE_NUM: begin
                tiles_remaining <= rx_byte;
                tile_wr_idx <= 0;
                byte_idx <= 0;
                state <= (rx_byte == 0) ? S_CMD : S_TILE_DATA;
            end
            S_TILE_DATA: begin
                tile_accum <= {tile_accum[23:0], rx_byte};
                byte_idx <= byte_idx + 1;
                if (byte_idx == 3) begin
                    tile_wr_data <= {tile_accum[23:0], rx_byte};
                    tile_wr_en <= 1;
                    tile_wr_idx <= tile_wr_idx + 1;
                    tiles_remaining <= tiles_remaining - 1;
                    byte_idx <= 0;
                    if (tiles_remaining == 1) state <= S_CMD;
                end
            end
            endcase
        end
    end
endmodule
