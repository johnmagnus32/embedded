/*
 * tb_spi_slave.v — Testbench: send bytes over SPI, verify reception
 */
`timescale 1ns/1ps

module tb_spi_slave;
    reg clk = 0;
    always #41.67 clk = ~clk;  // 12MHz system clock

    reg spi_clk = 0;
    reg spi_mosi = 0;
    reg spi_cs = 1;            // deselected initially

    wire [7:0] data;
    wire valid;

    spi_slave uut (
        .clk(clk),
        .spi_clk(spi_clk),
        .spi_mosi(spi_mosi),
        .spi_cs(spi_cs),
        .data(data),
        .valid(valid)
    );

    integer failures = 0;

    // Task: send one byte over SPI (MSB first)
    task spi_send_byte(input [7:0] byte_val);
        integer i;
        begin
            for (i = 7; i >= 0; i = i - 1) begin
                spi_mosi = byte_val[i];
                #500;               // setup time
                spi_clk = 1;       // rising edge — slave samples
                #500;               // hold
                spi_clk = 0;       // falling edge
                #500;
            end
        end
    endtask

    initial begin
        $dumpfile("spi_slave.vcd");
        $dumpvars(0, tb_spi_slave);

        // Wait for synchronizer to settle
        #1000;

        // Test 1: send 0xA5
        spi_cs = 0;
        spi_send_byte(8'hA5);
        #2000;  // wait for valid pulse to propagate
        if (data !== 8'hA5) begin
            $display("FAIL: expected 0xA5, got 0x%02X", data);
            failures = failures + 1;
        end

        // Test 2: send 0x3C without releasing CS
        spi_send_byte(8'h3C);
        #2000;
        if (data !== 8'h3C) begin
            $display("FAIL: expected 0x3C, got 0x%02X", data);
            failures = failures + 1;
        end

        // Test 3: release CS, re-select, send 0xFF
        spi_cs = 1;
        #2000;
        spi_cs = 0;
        spi_send_byte(8'hFF);
        #2000;
        if (data !== 8'hFF) begin
            $display("FAIL: expected 0xFF, got 0x%02X", data);
            failures = failures + 1;
        end

        if (failures == 0)
            $display("PASS: all SPI tests passed");
        else
            $display("FAILED: %0d test(s) failed", failures);

        $finish;
    end
endmodule
