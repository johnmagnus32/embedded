/*
 * ppu_top.v — Generic sprite PPU top-level
 *
 * SPI slave (from STM32) → sprite table + tile table + SPRAM
 * Pixel gen (scanline renderer) → LCD driver (8-bit parallel) → ILI9341
 *
 * The PPU is game-agnostic. The STM32 uploads tile graphics at boot,
 * then sends sprite positions each frame. Any game can use this.
 */
module ppu_top (
    // SPI from STM32 (command input)
    input  SPI_CLK,
    input  SPI_MOSI,
    input  SPI_CS,
    // 8-bit parallel to ILI9341
    output [7:0] LCD_D,
    output LCD_WR,
    output LCD_DC,
    output LCD_CS
);
    // System clock = 20 MHz, from the 48 MHz internal oscillator via the PLL.
    // A frame is ~615k clocks regardless of rate, so wall-clock FPS scales with
    // the clock: 12 MHz gave ~19.5 FPS, 20 MHz gives ~32 FPS (>30 target).
    // Measured Fmax of this design is ~23.5 MHz, so 20 MHz leaves ~15% margin.
    // (SB_HFOSC only offers 48/24/12/6 MHz; 24 exceeds Fmax, so a PLL is needed
    //  for an in-between rate. icepll -i 48 -o 20 -> DIVR=2 DIVF=39 DIVQ=5.)
    wire clk_48, clk, pll_lock;
    SB_HFOSC #(.CLKHF_DIV("0b00")) osc (   // 0b00 = undivided 48 MHz reference
        .CLKHFEN(1'b1), .CLKHFPU(1'b1), .CLKHF(clk_48)
    );
    SB_PLL40_CORE #(
        .FEEDBACK_PATH("SIMPLE"),
        .DIVR(4'd2),
        .DIVF(7'd39),
        .DIVQ(3'd5),
        .FILTER_RANGE(3'd1)
    ) pll (
        .REFERENCECLK(clk_48),
        .PLLOUTCORE(clk),
        .RESETB(1'b1),
        .BYPASS(1'b0),
        .LOCK(pll_lock)
    );

    // --- SPI command decoder ---
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

    // --- Sprite table (double-buffered) ---
    wire [5:0]  sprite_rd_idx;
    wire [39:0] sprite_rd_data;
    wire [6:0]  sprite_num_active;

    wire lcd_frame_start;   // vsync pulse from lcd_driver (frame boundary)

    sprite_table u_sprite_table (
        .clk(clk),
        .wr_idx(cmd_sprite_wr_idx),
        .wr_data(cmd_sprite_wr_data),
        .wr_en(cmd_sprite_wr_en),
        .swap(cmd_frame_valid),        // request a swap (may arrive mid-frame)
        .apply_swap(lcd_frame_start),  // apply it only at the frame boundary
        .rd_idx(sprite_rd_idx),
        .rd_data(sprite_rd_data),
        .wr_num_sprites(cmd_sprite_num),
        .wr_num_en(cmd_sprite_num_en),
        .num_sprites(sprite_num_active)
    );

    // --- Tile table ---
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

    // --- Sprite pixel memory (SPRAM) ---
    wire [13:0] sprite_rd_addr;
    wire [15:0] sprite_rd_pixel;

    sprite_mem u_sprite_mem (
        .clk(clk),
        .wr_addr(cmd_mem_addr), .wr_data(cmd_mem_wdata), .wr_en(cmd_mem_we),
        .rd_addr(sprite_rd_addr), .rd_data(sprite_rd_pixel)
    );

    // --- Pixel generator ---
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

    // --- LCD driver (8-bit parallel) ---
    lcd_driver u_lcd (
        .clk(clk),
        .frame_start(lcd_frame_start),
        .pixel_x(px_x), .pixel_y(px_y), .pixel_req(px_req),
        .pixel_color(px_color), .pixel_valid(px_valid),
        .lcd_data(LCD_D), .lcd_wr(LCD_WR),
        .lcd_dc(LCD_DC), .lcd_cs(LCD_CS)
    );
endmodule
