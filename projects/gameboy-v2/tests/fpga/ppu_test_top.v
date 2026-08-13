// PPU with external clock for gate-level testing on fpga-test machine
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

    wire lcd_frame_start;
    sprite_table u_sprites (
        .clk(clk), .wr_idx(cmd_sprite_wr_idx), .wr_data(cmd_sprite_wr_data),
        .wr_en(cmd_sprite_wr_en), .swap(cmd_frame_valid), .apply_swap(lcd_frame_start),
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
        .clk(clk), .frame_valid(cmd_frame_valid), .frame_start(lcd_frame_start),
        .pixel_x(px_x), .pixel_y(px_y),
        .pixel_req(px_req), .pixel_color(px_color), .pixel_valid(px_valid),
        .lcd_data(LCD_D), .lcd_wr(LCD_WR), .lcd_dc(LCD_DC), .lcd_cs(LCD_CS)
    );
endmodule
