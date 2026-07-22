/*
 * fpga_loader.c — app glue for FPGA bring-up.
 *
 * The iCE40 SPI-slave configuration protocol now lives in the reusable rtos
 * driver rtos/drivers/fpga/ice40.c (behind the fpga.h API); this file just
 * hands that driver the bitstream embedded in flash and logs the result.
 */
#include "fpga_loader.h"
#include "board.h"
#include "device.h"
#include "drivers/fpga.h"

/* Bitstream embedded in flash via linker section (see linker.ld). */
extern const uint8_t _fpga_bitstream_start[];
extern const uint8_t _fpga_bitstream_end[];

/* The iCE40 config-loader device (DTS node ice40cfg, compatible
 * "lattice,ice40up5k-cfg"). Declared here, resolved by the device model. */
DEVICE_DT_DECLARE(ice40cfg);

int fpga_load_bitstream(void)
{
    extern void uart_print(const char *s);
    extern void print_int(int n);

    unsigned int len = (unsigned int)(_fpga_bitstream_end - _fpga_bitstream_start);
    uart_print("FPGA: loading ");
    print_int(len);
    uart_print(" bytes...\n");

    int rc = fpga_load(DEVICE_DT_GET(ice40cfg), _fpga_bitstream_start, len);
    if (rc != 0) {
        uart_print("FPGA: config FAILED\n");
        return -1;
    }

    uart_print("FPGA: config OK\n");
    return 0;
}
