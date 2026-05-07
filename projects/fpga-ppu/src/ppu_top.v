/*
 * ppu_top.v — Dino PPU top-level
 *
 * Wires: SPI slave (from STM32) → game regs + sprite mem → pixel gen → LCD driver → ILI9341
 */
module ppu_top (
    // SPI from STM32
    input  SPI_CLK,
    input  SPI_MOSI,
    input  SPI_CS,
    // SPI to ILI9341
    output LCD_SCLK,
    output LCD_MOSI,
    output LCD_CS,
    output LCD_DC
);
    // 12MHz system clock
    wire clk;
    SB_HFOSC #(.CLKHF_DIV("0b10")) osc (
        .CLKHFEN(1'b1), .CLKHFPU(1'b1), .CLKHF(clk)
    );

    // SPI command decoder
    wire [7:0]  cmd_dino_y;
    wire [1:0]  cmd_dino_frame;
    wire [8:0]  cmd_obs0_x, cmd_obs1_x;
    wire [1:0]  cmd_obs0_type, cmd_obs1_type;
    wire        cmd_frame_valid;
    wire [14:0] cmd_mem_addr;
    wire [7:0]  cmd_mem_wdata;
    wire        cmd_mem_we;

    spi_cmd u_spi_cmd (
        .clk(clk),
        .spi_clk(SPI_CLK), .spi_mosi(SPI_MOSI), .spi_cs(SPI_CS),
        .dino_y(cmd_dino_y), .dino_frame(cmd_dino_frame),
        .obs0_x(cmd_obs0_x), .obs0_type(cmd_obs0_type),
        .obs1_x(cmd_obs1_x), .obs1_type(cmd_obs1_type),
        .frame_valid(cmd_frame_valid),
        .mem_addr(cmd_mem_addr), .mem_wdata(cmd_mem_wdata), .mem_we(cmd_mem_we)
    );

    // Game state registers (double-buffered)
    wire [7:0] reg_dino_y;
    wire [1:0] reg_dino_frame;
    wire [8:0] reg_obs0_x, reg_obs1_x;
    wire [1:0] reg_obs0_type, reg_obs1_type;

    game_regs u_regs (
        .clk(clk), .frame_valid(cmd_frame_valid),
        .in_dino_y(cmd_dino_y), .in_dino_frame(cmd_dino_frame),
        .in_obs0_x(cmd_obs0_x), .in_obs0_type(cmd_obs0_type),
        .in_obs1_x(cmd_obs1_x), .in_obs1_type(cmd_obs1_type),
        .dino_y(reg_dino_y), .dino_frame(reg_dino_frame),
        .obs0_x(reg_obs0_x), .obs0_type(reg_obs0_type),
        .obs1_x(reg_obs1_x), .obs1_type(reg_obs1_type)
    );

    // Sprite memory
    wire [13:0] sprite_rd_addr;
    wire [15:0] sprite_rd_data;

    sprite_mem u_sprite_mem (
        .clk(clk),
        .wr_addr(cmd_mem_addr), .wr_data(cmd_mem_wdata), .wr_en(cmd_mem_we),
        .rd_addr(sprite_rd_addr), .rd_data(sprite_rd_data)
    );

    // Pixel generator
    wire [8:0]  px_x, px_y;
    wire        px_req;
    wire [15:0] px_color;
    wire        px_valid;

    pixel_gen u_pixel_gen (
        .clk(clk),
        .pixel_x(px_x), .pixel_y(px_y), .pixel_req(px_req),
        .dino_y(reg_dino_y), .dino_frame(reg_dino_frame),
        .obs0_x(reg_obs0_x), .obs0_type(reg_obs0_type),
        .obs1_x(reg_obs1_x), .obs1_type(reg_obs1_type),
        .sprite_addr(sprite_rd_addr), .sprite_data(sprite_rd_data),
        .pixel_color(px_color), .pixel_valid(px_valid)
    );

    // LCD driver (SPI master to ILI9341)
    lcd_driver u_lcd (
        .clk(clk),
        .pixel_x(px_x), .pixel_y(px_y), .pixel_req(px_req),
        .pixel_color(px_color), .pixel_valid(px_valid),
        .lcd_sclk(LCD_SCLK), .lcd_mosi(LCD_MOSI),
        .lcd_cs(LCD_CS), .lcd_dc(LCD_DC)
    );
endmodule
