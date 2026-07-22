`timescale 1ns/1ps
// Gate-level e2e test: upload sprite, verify non-background pixels appear on LCD
module tb_ppu_gate_level;
    reg clk = 0;
    always #5 clk = ~clk;
    reg spi_clk = 0, spi_mosi = 0, spi_cs = 1;
    wire [7:0] LCD_D;
    wire LCD_WR, LCD_DC, LCD_CS;

    ppu_test_top uut (
        .clk(clk), .SPI_CLK(spi_clk), .SPI_MOSI(spi_mosi), .SPI_CS(spi_cs),
        .LCD_D(LCD_D), .LCD_WR(LCD_WR), .LCD_DC(LCD_DC), .LCD_CS(LCD_CS)
    );

    task spi_byte(input [7:0] b);
        integer i;
        for (i = 7; i >= 0; i = i - 1) begin
            spi_mosi = b[i];
            spi_clk = 0; #60;
            spi_clk = 1; #60;
        end
        spi_clk = 0;
    endtask

    integer total, non_bg, t;
    reg prev_wr;

    initial begin
        spi_cs = 1; #200;
        // Upload 2 pixels to SPRAM
        spi_cs = 0; #100;
        spi_byte(8'h02); spi_byte(8'h00); spi_byte(8'h00);
        spi_byte(8'h00); spi_byte(8'hF8);  // 0xF800 (red)
        spi_byte(8'hE0); spi_byte(8'h07);  // 0x07E0 (green)
        #100; spi_cs = 1; #200;
        // Upload tile table entry
        spi_cs = 0; #100;
        spi_byte(8'h04); spi_byte(8'h01);
        spi_byte(8'h00); spi_byte(8'h00); spi_byte(8'h01); spi_byte(8'h02);
        #100; spi_cs = 1; #200;
        // Send sprite frame: 1 sprite at y=100
        spi_cs = 0; #100;
        spi_byte(8'h01); spi_byte(8'h01);
        spi_byte(8'h00); spi_byte(8'h00); spi_byte(8'd100); spi_byte(8'h00); spi_byte(8'h00);
        #100; spi_cs = 1; #2000;

        // Monitor LCD output for enough time to reach scanline 100
        total = 0; non_bg = 0; prev_wr = LCD_WR;
        for (t = 0; t < 200000; t = t + 1) begin
            #10;
            if (LCD_WR && !prev_wr) begin
                total = total + 1;
                if (LCD_D != 8'h86 && LCD_D != 8'h7D && LCD_D != 8'h00)
                    non_bg = non_bg + 1;
            end
            prev_wr = LCD_WR;
        end

        $display("GATE-LEVEL: total=%0d non_bg=%0d", total, non_bg);
        if (non_bg > 0)
            $display("PASS: tb_ppu_gate_level (%0d sprite pixels)", non_bg);
        else
            $display("FAIL: tb_ppu_gate_level (no sprite pixels)");
        $finish;
    end
endmodule
