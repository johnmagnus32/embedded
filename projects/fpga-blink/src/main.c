/*
 * main.c — fpga-blink (RTOS app).
 *
 * On boot the STM32F411RE streams the FPGA bitstream (embedded in flash as
 * bitstream.h) into the iCE40UP5K over the config SPI bus, using the rtos iCE40
 * loader driver (rtos/drivers/fpga/ice40.c -> fpga_load()). Once CDONE goes
 * high the FPGA free-runs its blink; the STM32 just prints status and idles.
 *
 * Everything hardware-specific comes from the device tree (board.dts): the
 * console (USART2), the config SPI bus (SPI3), and the iCE40 loader node
 * (ice40cfg) with its CRESET/CDONE/SS pins. This replaces the old bare-metal
 * register-poking version — the TN1248 sequence now lives in the shared driver.
 *
 * Note: unlike the old bare-metal build, there is NO 96 MHz PLL bump here. That
 * was only needed because the bit-banged config clock had to clear the iCE40
 * 1 MHz slave-config floor; the rtos SPI *peripheral* driver produces ~8 MHz
 * SCK straight off the 16 MHz HSI (BR=/2), comfortably inside the 1-25 MHz
 * window. Status prints on USART2 -> Nucleo ST-Link VCP -> /dev/ttyACM0.
 */
#include "config.h"
#include "devicetree.h"
#include "device.h"
#include "drivers/uart.h"
#include "drivers/spi.h"
#include "drivers/fpga.h"
#include "sched.h"

#include "bitstream.h"   /* fpga_bitstream[], fpga_bitstream_len (from fpga/) */

DEVICE_DT_DECLARE(DT_CHOSEN_CONSOLE);
DEVICE_DT_DECLARE(spi3);
DEVICE_DT_DECLARE(ice40cfg);

extern void systick_init(uint32_t cpu_hz, uint32_t tick_hz);

static void idle_task(void)
{
    for (;;)
        __asm__ volatile ("wfi");
}

int main(void)
{
    const struct device *console = DEVICE_DT_GET(DT_CHOSEN_CONSOLE);

    /* Bring up console + config bus + loader (device model, DT-driven). */
    device_init_all();

    int rc = fpga_load(DEVICE_DT_GET(ice40cfg), fpga_bitstream, fpga_bitstream_len);
    uart_puts(console, rc == 0 ? "FPGA config OK\n" : "FPGA config FAILED\n");

    /* The FPGA now free-runs the blink; nothing left for the STM32 to do.
     * Run the (otherwise idle) scheduler so the system is in a normal RTOS
     * state rather than spinning in main. */
    sched_create_task(idle_task, "idle", 7);
    systick_init(DT_SYSCLK_HZ, 1000);
    sched_start();

    for (;;) { }   /* not reached */
}
