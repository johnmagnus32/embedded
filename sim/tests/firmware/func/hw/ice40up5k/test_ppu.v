module ppu_test_top (
    input  clk,
    input  SPI_CLK,
    input  SPI_MOSI,
    input  SPI_CS,
    output [7:0] LCD_D,
    output LCD_WR,
    output LCD_DC,
    output LCD_CS
);
    wire [5:0] cmd_sprite_wr_idx;
    wire [39:0] cmd_sprite_wr_data;
    wire cmd_sprite_wr_en;
    wire [6:0] cmd_sprite_num;
    wire cmd_sprite_num_en;
    wire cmd_frame_valid;
    wire [7:0] cmd_tile_wr_idx;
    wire [31:0] cmd_tile_wr_data;
    wire cmd_tile_wr_en;
    wire [14:0] cmd_mem_addr;
    wire [7:0] cmd_mem_wdata;
    wire cmd_mem_we;
    wire [15:0] cmd_bg_color;

    spi_cmd u_spi (
        .clk(clk), .spi_clk(SPI_CLK), .spi_mosi(SPI_MOSI), .spi_cs(SPI_CS),
        .sprite_wr_idx(cmd_sprite_wr_idx), .sprite_wr_data(cmd_sprite_wr_data),
        .sprite_wr_en(cmd_sprite_wr_en), .sprite_num(cmd_sprite_num),
        .sprite_num_en(cmd_sprite_num_en), .frame_valid(cmd_frame_valid),
        .tile_wr_idx(cmd_tile_wr_idx), .tile_wr_data(cmd_tile_wr_data),
        .tile_wr_en(cmd_tile_wr_en), .mem_addr(cmd_mem_addr),
        .mem_wdata(cmd_mem_wdata), .mem_we(cmd_mem_we), .bg_color(cmd_bg_color)
    );

    wire [5:0] sprite_rd_idx;
    wire [39:0] sprite_rd_data;
    wire [6:0] num_sprites;

    sprite_table u_sprites (
        .clk(clk), .wr_idx(cmd_sprite_wr_idx), .wr_data(cmd_sprite_wr_data),
        .wr_en(cmd_sprite_wr_en), .swap(cmd_frame_valid),
        .rd_idx(sprite_rd_idx), .rd_data(sprite_rd_data),
        .wr_num_sprites(cmd_sprite_num), .wr_num_en(cmd_sprite_num_en),
        .num_sprites(num_sprites)
    );

    wire [7:0] tile_rd_idx;
    wire [31:0] tile_rd_data;

    tile_table u_tiles (
        .clk(clk), .wr_idx(cmd_tile_wr_idx), .wr_data(cmd_tile_wr_data),
        .wr_en(cmd_tile_wr_en), .rd_idx(tile_rd_idx), .rd_data(tile_rd_data)
    );

    wire [13:0] sprite_addr;
    wire [15:0] sprite_data;

    sprite_mem u_mem (
        .clk(clk), .wr_addr(cmd_mem_addr), .wr_data(cmd_mem_wdata),
        .wr_en(cmd_mem_we), .rd_addr(sprite_addr), .rd_data(sprite_data)
    );

    wire [8:0] px_x, px_y;
    wire px_req;
    wire [15:0] px_color;
    wire px_valid;

    pixel_gen u_pixel (
        .clk(clk), .pixel_x(px_x), .pixel_y(px_y),
        .pixel_req(px_req), .pixel_color(px_color), .pixel_valid(px_valid),
        .sprite_rd_idx(sprite_rd_idx), .sprite_rd_data(sprite_rd_data),
        .num_sprites(num_sprites), .tile_rd_idx(tile_rd_idx),
        .tile_rd_data(tile_rd_data), .sprite_addr(sprite_addr),
        .sprite_data(sprite_data), .bg_color(cmd_bg_color)
    );

    lcd_driver u_lcd (
        .clk(clk), .frame_valid(cmd_frame_valid),
        .pixel_x(px_x), .pixel_y(px_y),
        .pixel_req(px_req), .pixel_color(px_color), .pixel_valid(px_valid),
        .lcd_data(LCD_D), .lcd_wr(LCD_WR), .lcd_dc(LCD_DC), .lcd_cs(LCD_CS)
    );
endmodule
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
                sprite_wr_idx <= 6'h3F;  // will wrap to 0 on first increment
                byte_idx <= 0;
                state <= (rx_byte == 0) ? S_CMD : S_SPRITE_DATA;
            end
            S_SPRITE_DATA: begin
                if (byte_idx == 0) begin
                    // First byte of new sprite — advance write index
                    sprite_wr_idx <= sprite_wr_idx + 1;
                end
                sprite_accum <= {sprite_accum[31:0], rx_byte};
                byte_idx <= byte_idx + 1;
                if (byte_idx == 4) begin
                    sprite_wr_data <= {sprite_accum[31:0], rx_byte};
                    sprite_wr_en <= 1;
                    sprites_remaining <= sprites_remaining - 1;
                    byte_idx <= 0;
                    if (sprites_remaining == 1) state <= S_CMD;
                end else if (byte_idx == 0 && sprite_wr_idx != 0) begin
                end
            end

            // --- Cmd 0x02: SPRAM pixel upload ---
            S_MEM_ADDR: begin
                if (byte_idx == 0) begin
                    mem_addr_reg[14:8] <= rx_byte[6:0];
                    byte_idx <= 1;
                end else begin
                    mem_addr_reg[7:0] <= rx_byte;
                    // Set to addr-1 so first increment in S_MEM_DATA gives correct start
                    mem_addr <= {mem_addr_reg[14:8], rx_byte} - 1;
                    state <= S_MEM_DATA;
                end
            end
            S_MEM_DATA: begin
                mem_addr <= mem_addr + 1;
                mem_wdata <= rx_byte;
                mem_we <= 1;
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
                tile_wr_idx <= 8'hFF;  // wraps to 0 on first increment
                byte_idx <= 0;
                state <= (rx_byte == 0) ? S_CMD : S_TILE_DATA;
            end
            S_TILE_DATA: begin
                if (byte_idx == 0)
                    tile_wr_idx <= tile_wr_idx + 1;
                tile_accum <= {tile_accum[23:0], rx_byte};
                byte_idx <= byte_idx + 1;
                if (byte_idx == 3) begin
                    tile_wr_data <= {tile_accum[23:0], rx_byte};
                    tile_wr_en <= 1;
                    tiles_remaining <= tiles_remaining - 1;
                    byte_idx <= 0;
                    if (tiles_remaining == 1) state <= S_CMD;
                end
            end
            endcase
        end
    end
endmodule
/*
 * sprite_table.v — Double-buffered sprite table (64 entries)
 *
 * Each entry: {x[8:0], y[7:0], tile[7:0], flags[7:0]} = 33 bits (stored as 40)
 * SPI writes to bank B. On frame_valid, banks swap.
 * Pixel gen reads from bank A (stable during rendering).
 */
module sprite_table (
    input         clk,
    // Write port (from spi_cmd)
    input  [5:0]  wr_idx,
    input  [39:0] wr_data,  // {x_hi[0], x_lo[7:0], y[7:0], tile[7:0], flags[7:0]}
    input         wr_en,
    input         swap,     // pulse on frame_valid to swap banks
    // Read port (from pixel_gen)
    input  [5:0]  rd_idx,
    output [39:0] rd_data,
    // Number of active sprites (written by spi_cmd)
    input  [6:0]  wr_num_sprites,
    input         wr_num_en,
    output [6:0]  num_sprites
);
    reg [39:0] bank0 [0:63];
    reg [39:0] bank1 [0:63];
    reg active_bank = 0;  // pixel_gen reads from this bank
    reg [6:0] num_a = 0, num_b = 0;

    // Write always goes to the inactive bank
    always @(posedge clk) begin
        if (wr_en) begin
            if (active_bank == 0) bank1[wr_idx] <= wr_data;
            else                  bank0[wr_idx] <= wr_data;
        end
        if (wr_num_en) begin
            if (active_bank == 0) num_b <= wr_num_sprites;
            else                  num_a <= wr_num_sprites;
        end
        if (swap) active_bank <= ~active_bank;
    end

    // Read from active bank (registered — matches SB_RAM40_4K behavior)
    reg [39:0] rd_data_reg;
    always @(posedge clk)
        rd_data_reg <= active_bank ? bank1[rd_idx] : bank0[rd_idx];
    assign rd_data = rd_data_reg;
    assign num_sprites = active_bank ? num_b : num_a;
endmodule
/*
 * tile_table.v — Tile metadata table (256 entries)
 *
 * Each entry: {base_addr[15:0], width[7:0], height[7:0]} = 32 bits
 * Written by SPI command 0x04. Read by pixel_gen to look up tile dimensions.
 */
module tile_table (
    input         clk,
    // Write port (from spi_cmd)
    input  [7:0]  wr_idx,
    input  [31:0] wr_data,  // {base_addr[15:0], width[7:0], height[7:0]}
    input         wr_en,
    // Read port (from pixel_gen)
    input  [7:0]  rd_idx,
    output reg [31:0] rd_data
);
    reg [31:0] entries [0:255];

    always @(posedge clk) begin
        if (wr_en) entries[wr_idx] <= wr_data;
        rd_data <= entries[rd_idx];
    end
endmodule
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
/*
 * pixel_gen.v — Scanline-based sprite renderer
 *
 * Uses explicit wait states for all registered memory reads.
 */
module pixel_gen (
    input         clk,
    input  [8:0]  pixel_x,
    input  [8:0]  pixel_y,
    input         pixel_req,
    output reg [15:0] pixel_color,
    output reg    pixel_valid,
    output reg [5:0] sprite_rd_idx,
    input  [39:0]    sprite_rd_data,
    input  [6:0]     num_sprites,
    output reg [7:0] tile_rd_idx,
    input  [31:0]    tile_rd_data,
    output reg [13:0] sprite_addr,
    input  [15:0]     sprite_data,
    input  [15:0] bg_color
);
    localparam TRANSPARENT = 16'hF81F;
    localparam MAX_PER_LINE = 8;

    reg [8:0] active_x     [0:MAX_PER_LINE-1];
    reg [7:0] active_y     [0:MAX_PER_LINE-1];
    reg [7:0] active_tile  [0:MAX_PER_LINE-1];
    reg [3:0] num_active;
    reg [15:0] active_base  [0:MAX_PER_LINE-1];
    reg [7:0]  active_width [0:MAX_PER_LINE-1];
    reg [7:0]  active_height[0:MAX_PER_LINE-1];
    reg [13:0] active_row_addr [0:MAX_PER_LINE-1];  // pre-computed per scanline

    localparam S_IDLE = 0, S_SCAN_SET = 1, S_SCAN_WAIT = 2, S_SCAN_READ = 3,
               S_TILE_SET = 4, S_TILE_WAIT = 5, S_TILE_READ = 6,
               S_COMPOSE = 7, S_SPRAM_SET = 8, S_SPRAM_WAIT = 9, S_OUTPUT = 10;
    reg [3:0] state = S_IDLE;
    reg [5:0] scan_idx;
    reg [3:0] tile_lookup_idx;
    reg [3:0] compose_idx;
    reg [8:0] last_y = 9'h1FF;
    reg found_pixel = 0;

    wire [8:0] sp_x = {sprite_rd_data[24], sprite_rd_data[39:32]};
    wire [7:0] sp_y = sprite_rd_data[23:16];
    wire [7:0] sp_tile = sprite_rd_data[15:8];

    always @(posedge clk) begin
        pixel_valid <= 0;

        case (state)
        S_IDLE: begin
            if (pixel_req && pixel_y != last_y) begin
                last_y <= pixel_y;
                scan_idx <= 0;
                num_active <= 0;
                sprite_rd_idx <= 0;
                state <= S_SCAN_SET;
            end else if (pixel_req) begin
                compose_idx <= 0;
                found_pixel <= 0;
                state <= (num_active > 0) ? S_COMPOSE : S_OUTPUT;
            end
        end

        // --- Sprite scan: three-phase (set addr, wait for BRAM, then read) ---
        S_SCAN_SET: begin
            // sprite_rd_idx was set last cycle, BRAM needs 1 cycle to latch
            state <= S_SCAN_WAIT;
        end

        S_SCAN_WAIT: begin
            // BRAM RDATA now valid, rd_data_reg captures this edge
            state <= S_SCAN_READ;
        end

        S_SCAN_READ: begin
            // Data is now valid for current sprite_rd_idx
            if (scan_idx < num_sprites) begin
                if (sp_x != 9'h1FF && pixel_y >= sp_y && pixel_y < sp_y + 40) begin
                    active_x[num_active] <= sp_x;
                    active_y[num_active] <= sp_y;
                    active_tile[num_active] <= sp_tile;
                    num_active <= num_active + 1;
                end
                scan_idx <= scan_idx + 1;
                sprite_rd_idx <= scan_idx + 1;
                state <= S_SCAN_SET;
            end else begin
                // Done scanning
                if (num_active > 0) begin
                    tile_rd_idx <= active_tile[0];
                    tile_lookup_idx <= 0;
                    state <= S_TILE_SET;
                end else begin
                    state <= S_OUTPUT;
                end
            end
        end

        // --- Tile lookup: three-phase ---
        S_TILE_SET: begin
            state <= S_TILE_WAIT;
        end

        S_TILE_WAIT: begin
            state <= S_TILE_READ;
        end

        S_TILE_READ: begin
            active_base[tile_lookup_idx] <= tile_rd_data[31:16];
            active_width[tile_lookup_idx] <= tile_rd_data[15:8];
            active_height[tile_lookup_idx] <= tile_rd_data[7:0];
            // Pre-compute row start address: base + (y_offset << width_shift)
            // tile_rd_data[15:8] is width_shift (log2 of pixel width)
            active_row_addr[tile_lookup_idx] <= tile_rd_data[31:16] +
                ((pixel_y - active_y[tile_lookup_idx]) << tile_rd_data[11:8]);
            if (tile_lookup_idx + 1 >= num_active) begin
                compose_idx <= 0;
                found_pixel <= 0;
                state <= S_COMPOSE;
            end else begin
                tile_lookup_idx <= tile_lookup_idx + 1;
                tile_rd_idx <= active_tile[tile_lookup_idx + 1];
                state <= S_TILE_SET;
            end
        end

        // --- Pixel compositing ---
        S_COMPOSE: begin
            if (compose_idx < num_active && !found_pixel) begin
                if (pixel_x >= active_x[compose_idx] &&
                    pixel_x < active_x[compose_idx] + (8'd1 << active_width[compose_idx][3:0]) &&
                    pixel_y >= active_y[compose_idx] &&
                    pixel_y < active_y[compose_idx] + active_height[compose_idx]) begin
                    // addr = row_start + x_offset
                    sprite_addr <= active_row_addr[compose_idx] +
                                   (pixel_x - active_x[compose_idx]);
                    state <= S_SPRAM_SET;
                end else begin
                    compose_idx <= compose_idx + 1;
                end
            end else begin
                state <= S_OUTPUT;
            end
        end

        S_SPRAM_SET: begin
            // Wait one cycle for SPRAM address to propagate
            state <= S_SPRAM_WAIT;
        end

        S_SPRAM_WAIT: begin
            if (sprite_data != TRANSPARENT) begin
                pixel_color <= sprite_data;
                found_pixel <= 1;
            end else begin
                compose_idx <= compose_idx + 1;
            end
            state <= (sprite_data != TRANSPARENT) ? S_OUTPUT : S_COMPOSE;
        end

        S_OUTPUT: begin
            if (!found_pixel)
                pixel_color <= bg_color;
            pixel_valid <= 1;
            state <= S_IDLE;
        end
        endcase
    end
endmodule
/*
 * lcd_driver.v — ILI9341 8-bit parallel (8080) interface
 *
 * Sends ILI9341 init commands (column/row address, memory write) at start
 * of each frame, then outputs pixels via 8-bit parallel bus.
 * Two write cycles per pixel (high byte, low byte).
 *
 * Pin mapping:
 *   D[7:0] — 8-bit data bus
 *   WR     — write strobe (active low, rising edge latches)
 *   DC     — data/command (1=data, 0=command)
 *   CS     — chip select (active low, active during frame)
 */
module lcd_driver (
    input         clk,
    input         frame_valid,
    // Pixel interface (to pixel_gen)
    output reg [8:0] pixel_x,
    output reg [8:0] pixel_y,
    output reg       pixel_req,
    input  [15:0]    pixel_color,
    input            pixel_valid,
    // Parallel LCD pins
    output reg [7:0] lcd_data,
    output reg       lcd_wr,
    output reg       lcd_dc,
    output reg       lcd_cs
);
    localparam W = 240;
    localparam H = 320;

    localparam S_INIT = 0, S_CMD = 1, S_CMD_WR = 2,
               S_REQ = 3, S_WAIT = 4, S_HI = 5, S_LO = 6, S_DONE = 7;
    reg [2:0] state = S_INIT;
    reg [15:0] cur_color;
    reg [1:0] wr_phase;
    reg [19:0] frame_timer = 0;

    // Init command sequence ROM
    // Format: {dc, data} pairs. dc=0 for command, dc=1 for parameter.
    // Sequence: 0x2A + 0,0,0,239, 0x2B + 0,0,1,63, 0x2C
    reg [8:0] init_rom [0:10];  // {dc, data[7:0]}
    reg [3:0] init_idx;

    initial begin
        // Column address set: 0..239
        init_rom[0]  = {1'b0, 8'h2A};  // cmd
        init_rom[1]  = {1'b1, 8'h00};  // col_start hi
        init_rom[2]  = {1'b1, 8'h00};  // col_start lo
        init_rom[3]  = {1'b1, 8'h00};  // col_end hi
        init_rom[4]  = {1'b1, 8'hEF};  // col_end lo (239)
        // Row address set: 0..319
        init_rom[5]  = {1'b0, 8'h2B};  // cmd
        init_rom[6]  = {1'b1, 8'h00};  // row_start hi
        init_rom[7]  = {1'b1, 8'h00};  // row_start lo
        init_rom[8]  = {1'b1, 8'h01};  // row_end hi
        init_rom[9]  = {1'b1, 8'h3F};  // row_end lo (319)
        // Memory write
        init_rom[10] = {1'b0, 8'h2C};  // cmd
    end

    // Frame timing

    // No initial block — all outputs set explicitly by state machine.
    // This avoids Yosys inserting inverters for non-zero initial values.

    always @(posedge clk) begin
        pixel_req <= 0;

        case (state)
        S_INIT: begin
            lcd_cs <= 0;
            lcd_dc <= 0;
            init_idx <= 0;
            state <= S_CMD;
        end

        S_CMD: begin
            // Set DC and data, WR stays high (setup phase)
            lcd_dc <= init_rom[init_idx][8];
            lcd_data <= init_rom[init_idx][7:0];
            state <= S_CMD_WR;
        end

        S_CMD_WR: begin
            // Strobe WR low then high
            if (!wr_phase[0]) begin
                lcd_wr <= 0;
                wr_phase <= 1;
            end else begin
                lcd_wr <= 1;
                wr_phase <= 0;
                if (init_idx == 10) begin
                    pixel_x <= 0;
                    pixel_y <= 0;
                    state <= S_REQ;
                end else begin
                    init_idx <= init_idx + 1;
                    state <= S_CMD;
                end
            end
        end

        S_REQ: begin
            lcd_dc <= 1;
            pixel_req <= 1;
            state <= S_WAIT;
        end

        S_WAIT: begin
            if (pixel_valid) begin
                cur_color <= pixel_color;
                state <= S_HI;
                wr_phase <= 0;
            end
        end

        S_HI: begin
            case (wr_phase)
                0: begin lcd_data <= cur_color[15:8]; lcd_wr <= 0; wr_phase <= 1; end
                1: begin lcd_wr <= 1; state <= S_LO; wr_phase <= 0; end
            endcase
        end

        S_LO: begin
            case (wr_phase)
                0: begin lcd_data <= cur_color[7:0]; lcd_wr <= 0; wr_phase <= 1; end
                1: begin
                    lcd_wr <= 1;
                    if (pixel_x == W - 1) begin
                        pixel_x <= 0;
                        if (pixel_y == H - 1) begin
                            state <= S_DONE;
                        end else begin
                            pixel_y <= pixel_y + 1;
                            state <= S_REQ;
                        end
                    end else begin
                        pixel_x <= pixel_x + 1;
                        state <= S_REQ;
                    end
                end
            endcase
        end

        S_DONE: begin
            lcd_cs <= 1;
            frame_timer <= frame_timer + 1;
            if (frame_timer >= 20'd800000)  begin
                frame_timer <= 0;
                state <= S_INIT;
            end
        end
        endcase
    end
endmodule
