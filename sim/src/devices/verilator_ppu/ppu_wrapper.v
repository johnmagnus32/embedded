/*
 * ppu_wrapper.v — Verilator-friendly PPU top-level
 *
 * Replaces SB_HFOSC with an external clock input.
 * SB_SPRAM256KA is provided as a behavioral model below.
 */
module ppu_wrapper (
    input         clk,
    input         SPI_CLK,
    input         SPI_MOSI,
    input         SPI_CS,
    output [7:0]  LCD_D,
    output        LCD_WR,
    output        LCD_DC,
    output        LCD_CS
);
    wire [5:0]  cmd_sprite_wr_idx;
    wire [39:0] cmd_sprite_wr_data;
    wire        cmd_sprite_wr_en;
    wire [6:0]  cmd_sprite_num;
    wire        cmd_sprite_num_en;
    wire        cmd_frame_valid;
    wire [7:0]  cmd_tile_wr_idx;
    wire [31:0] cmd_tile_wr_data;
    wire        cmd_tile_wr_en;
    wire [14:0] cmd_mem_addr;
    wire [7:0]  cmd_mem_wdata;
    wire        cmd_mem_we;
    wire [15:0] cmd_bg_color;

    spi_cmd u_spi_cmd (
        .clk(clk),
        .spi_clk(SPI_CLK), .spi_mosi(SPI_MOSI), .spi_cs(SPI_CS),
        .sprite_wr_idx(cmd_sprite_wr_idx),
        .sprite_wr_data(cmd_sprite_wr_data),
        .sprite_wr_en(cmd_sprite_wr_en),
        .sprite_num(cmd_sprite_num),
        .sprite_num_en(cmd_sprite_num_en),
        .frame_valid(cmd_frame_valid),
        .tile_wr_idx(cmd_tile_wr_idx),
        .tile_wr_data(cmd_tile_wr_data),
        .tile_wr_en(cmd_tile_wr_en),
        .mem_addr(cmd_mem_addr),
        .mem_wdata(cmd_mem_wdata),
        .mem_we(cmd_mem_we),
        .bg_color(cmd_bg_color)
    );

    wire [5:0]  sprite_rd_idx;
    wire [39:0] sprite_rd_data;
    wire [6:0]  sprite_num_active;

    sprite_table u_sprite_table (
        .clk(clk),
        .wr_idx(cmd_sprite_wr_idx),
        .wr_data(cmd_sprite_wr_data),
        .wr_en(cmd_sprite_wr_en),
        .swap(cmd_frame_valid),
        .rd_idx(sprite_rd_idx),
        .rd_data(sprite_rd_data),
        .wr_num_sprites(cmd_sprite_num),
        .wr_num_en(cmd_sprite_num_en),
        .num_sprites(sprite_num_active)
    );

    wire [7:0]  tile_rd_idx;
    wire [31:0] tile_rd_data;

    tile_table u_tile_table (
        .clk(clk),
        .wr_idx(cmd_tile_wr_idx),
        .wr_data(cmd_tile_wr_data),
        .wr_en(cmd_tile_wr_en),
        .rd_idx(tile_rd_idx),
        .rd_data(tile_rd_data)
    );

    wire [13:0] sprite_rd_addr;
    wire [15:0] sprite_rd_pixel;

    sprite_mem u_sprite_mem (
        .clk(clk),
        .wr_addr(cmd_mem_addr), .wr_data(cmd_mem_wdata), .wr_en(cmd_mem_we),
        .rd_addr(sprite_rd_addr), .rd_data(sprite_rd_pixel)
    );

    wire [8:0]  px_x, px_y;
    wire        px_req;
    wire [15:0] px_color;
    wire        px_valid;

    pixel_gen u_pixel_gen (
        .clk(clk),
        .pixel_x(px_x), .pixel_y(px_y), .pixel_req(px_req),
        .pixel_color(px_color), .pixel_valid(px_valid),
        .sprite_rd_idx(sprite_rd_idx),
        .sprite_rd_data(sprite_rd_data),
        .num_sprites(sprite_num_active),
        .tile_rd_idx(tile_rd_idx),
        .tile_rd_data(tile_rd_data),
        .sprite_addr(sprite_rd_addr),
        .sprite_data(sprite_rd_pixel),
        .bg_color(cmd_bg_color)
    );

    lcd_driver u_lcd (
        .clk(clk),
        .frame_valid(cmd_frame_valid),
        .pixel_x(px_x), .pixel_y(px_y), .pixel_req(px_req),
        .pixel_color(px_color), .pixel_valid(px_valid),
        .lcd_data(LCD_D), .lcd_wr(LCD_WR),
        .lcd_dc(LCD_DC), .lcd_cs(LCD_CS)
    );
endmodule

/* Behavioral SPRAM for Verilator (replaces iCE40 primitive) */
module SB_SPRAM256KA (
    input  [13:0] ADDRESS,
    input  [15:0] DATAIN,
    input  [3:0]  MASKWREN,
    input         WREN,
    input         CHIPSELECT,
    input         CLOCK,
    input         STANDBY,
    input         SLEEP,
    input         POWEROFF,
    output reg [15:0] DATAOUT
);
    reg [15:0] mem [0:16383];

    always @(posedge CLOCK) begin
        if (CHIPSELECT && !SLEEP && POWEROFF) begin
            if (WREN) begin
                if (MASKWREN[0]) mem[ADDRESS][3:0]   <= DATAIN[3:0];
                if (MASKWREN[1]) mem[ADDRESS][7:4]   <= DATAIN[7:4];
                if (MASKWREN[2]) mem[ADDRESS][11:8]  <= DATAIN[11:8];
                if (MASKWREN[3]) mem[ADDRESS][15:12] <= DATAIN[15:12];
            end
            DATAOUT <= mem[ADDRESS];
        end
    end
endmodule
