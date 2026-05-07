/*
 * spi_cmd.v — SPI slave with command protocol
 *
 * Commands from STM32:
 *   0x01 [dino_y] [dino_frame] [n_obs] [obs0_xl obs0_xh obs0_type] ...
 *        → frame update (writes to game registers)
 *   0x02 [addr_h addr_l] [data...]
 *        → sprite upload (writes to SPRAM until CS deasserts)
 */
module spi_cmd (
    input        clk,
    input        spi_clk,
    input        spi_mosi,
    input        spi_cs,

    // Frame update outputs
    output reg [7:0] dino_y,
    output reg [1:0] dino_frame,
    output reg [8:0] obs0_x,
    output reg [1:0] obs0_type,
    output reg [8:0] obs1_x,
    output reg [1:0] obs1_type,
    output reg       frame_valid,

    // Sprite memory write port
    output reg [14:0] mem_addr,
    output reg [7:0]  mem_wdata,
    output reg        mem_we
);
    // SPI clock synchronizer (3-stage for safe edge detection)
    reg [2:0] sclk_sync = 0;
    always @(posedge clk)
        sclk_sync <= {sclk_sync[1:0], spi_clk};
    wire sclk_rise = (sclk_sync[2:1] == 2'b01);

    // CS synchronizer
    reg [1:0] cs_sync = 2'b11;
    always @(posedge clk)
        cs_sync <= {cs_sync[0], spi_cs};
    wire cs_active = !cs_sync[1];
    wire cs_deassert = (cs_sync == 2'b10);

    // Shift register
    reg [7:0] shift = 0;
    reg [2:0] bit_count = 0;
    wire byte_done = (bit_count == 7) && sclk_rise && cs_active;

    always @(posedge clk) begin
        if (!cs_active) begin
            bit_count <= 0;
        end else if (sclk_rise) begin
            shift <= {shift[6:0], spi_mosi};
            bit_count <= bit_count + 1;
        end
    end

    // Command state machine
    localparam S_CMD     = 0;  // waiting for command byte
    localparam S_FRAME   = 1;  // receiving frame update bytes
    localparam S_SPRITE_ADDR = 2;  // receiving sprite address
    localparam S_SPRITE_DATA = 3;  // receiving sprite data

    reg [1:0] state = S_CMD;
    reg [3:0] byte_idx = 0;
    reg [14:0] sprite_addr = 0;

    always @(posedge clk) begin
        frame_valid <= 0;
        mem_we <= 0;

        if (cs_deassert) begin
            state <= S_CMD;
            byte_idx <= 0;
            if (state == S_FRAME)
                frame_valid <= 1;
        end else if (byte_done) begin
            case (state)
            S_CMD: begin
                byte_idx <= 0;
                if ({shift[6:0], spi_mosi} == 8'h01)
                    state <= S_FRAME;
                else if ({shift[6:0], spi_mosi} == 8'h02)
                    state <= S_SPRITE_ADDR;
            end

            S_FRAME: begin
                case (byte_idx)
                0: dino_y     <= {shift[6:0], spi_mosi};
                1: dino_frame <= {shift[6:0], spi_mosi};
                2: obs0_x[7:0] <= {shift[6:0], spi_mosi};
                3: obs0_x[8]   <= spi_mosi;
                4: obs0_type   <= {shift[6:0], spi_mosi};
                5: obs1_x[7:0] <= {shift[6:0], spi_mosi};
                6: obs1_x[8]   <= spi_mosi;
                7: obs1_type   <= {shift[6:0], spi_mosi};
                endcase
                byte_idx <= byte_idx + 1;
            end

            S_SPRITE_ADDR: begin
                if (byte_idx == 0) begin
                    sprite_addr[14:8] <= {shift[6:0], spi_mosi};
                    byte_idx <= 1;
                end else begin
                    sprite_addr[7:0] <= {shift[6:0], spi_mosi};
                    byte_idx <= 0;
                    state <= S_SPRITE_DATA;
                end
            end

            S_SPRITE_DATA: begin
                mem_addr <= sprite_addr;
                mem_wdata <= {shift[6:0], spi_mosi};
                mem_we <= 1;
                sprite_addr <= sprite_addr + 1;
            end
            endcase
        end
    end
endmodule
