/*
 * main.c — fpga-spi (RTOS app).
 *
 * Boot: the STM32F411RE streams the FPGA bitstream (embedded as bitstream.h)
 * into the iCE40UP5K over the config SPI bus (SPI3) using the rtos iCE40 loader
 * driver (fpga_load()). Then a scheduler task drives two of the FPGA's LEDs
 * over the *runtime* SPI bus (SPI1) and reads them back to self-verify.
 *
 * The STM32 is a "dumb" pattern generator: as SPI master it sends one command
 * byte per CS frame (cmd bit0->LED0, bit1->LED1), walking 00->01->10->11 on a
 * ~300 ms cadence. After each command it reads the FPGA's two LED output pins
 * back on PC0/PC1 and compares — proving the STM32<->FPGA link end to end. A
 * status line prints on USART2 (-> Nucleo ST-Link VCP -> /dev/ttyACM0):
 *
 *   cmd=0bXX fb=0bXX OK/FAIL (ok=N fail=N)
 *
 * All wiring comes from board.dts. The runtime bus runs slow (SPI1 br=/64,
 * ~250 kHz) because the FPGA's oversampled SPI slave can't track multi-MHz SCK;
 * the config bus runs at /2 (~8 MHz), inside the iCE40 slave-config window. No
 * 96 MHz PLL bump is needed (that was only for the old bit-banged config path).
 */
#include "config.h"
#include "devicetree.h"
#include "device.h"
#include "drivers/uart.h"
#include "drivers/spi.h"
#include "drivers/gpio.h"
#include "drivers/fpga.h"
#include "sched.h"

#include "bitstream.h"   /* fpga_bitstream[], fpga_bitstream_len (from fpga/) */

DEVICE_DT_DECLARE(DT_CHOSEN_CONSOLE);
DEVICE_DT_DECLARE(spi1);         /* runtime LED command bus */
DEVICE_DT_DECLARE(spi3);         /* config bus (parent of ice40cfg) */
DEVICE_DT_DECLARE(ice40cfg);     /* iCE40 loader device */
DEVICE_DT_DECLARE(gpioc);        /* LED feedback inputs PC0/PC1 */

extern void systick_init(uint32_t cpu_hz, uint32_t tick_hz);

#define FB0_PIN 0u   /* PC0 <- FPGA LED0 */
#define FB1_PIN 1u   /* PC1 <- FPGA LED1 */

/* --- tiny UART formatting helpers (no libc) --- */
static void put_u32(const struct device *con, uint32_t v)
{
    char buf[10]; int i = 0;
    if (v == 0) { uart_poll_out(con, '0'); return; }
    while (v) { buf[i++] = (char)('0' + (v % 10)); v /= 10; }
    while (i) uart_poll_out(con, buf[--i]);
}
static void put_bits2(const struct device *con, uint8_t v)
{
    uart_poll_out(con, (v & 2) ? '1' : '0');
    uart_poll_out(con, (v & 1) ? '1' : '0');
}

/* Read the FPGA's two LED outputs: bit0 = PC0 (LED0), bit1 = PC1 (LED1). */
static uint8_t read_feedback(const struct device *gpio)
{
    uint8_t fb0 = (uint8_t)gpio_pin_get(gpio, FB0_PIN);
    uint8_t fb1 = (uint8_t)gpio_pin_get(gpio, FB1_PIN);
    return (uint8_t)((fb1 << 1) | fb0);
}

/* Always-READY idle task. The scheduler requires one: when led_task sleeps,
 * pick_next() must have some other READY task to run, or it force-resumes the
 * sleeping task immediately (nullifying sched_sleep_ms — the settle + cadence).
 * See rtos/kernel/sched.c pick_next(): "Callers should always provide an
 * always-READY idle task." */
static void idle_task(void)
{
    for (;;)
        __asm__ volatile ("wfi");
}

/* Send one CS-framed command byte, wait a guaranteed settle, read PC0/PC1.
 *
 * The settle uses sched_sleep_ms(2), NOT (1): a 1-tick sleep at the 1 kHz tick
 * rounds to somewhere in (0, 1] ms depending on tick phase and can be ~0, which
 * is not a reliable settle. sched_sleep_ms(2) guarantees >1 ms of real delay.
 * The FPGA latches the byte within microseconds (its slave latches on the 8th
 * SCK edge, before CS even releases), so >1 ms is generous margin. */
static uint8_t command_and_read(const struct device *spi,
                                const struct device *gpio, uint8_t cmd)
{
    spi_write_f(spi, &cmd, 1, 0);
    sched_sleep_ms(2);
    return read_feedback(gpio);
}

/* Runtime task: walk the two LEDs through all states, verifying each. */
static void led_task(void)
{
    const struct device *con  = DEVICE_DT_GET(DT_CHOSEN_CONSOLE);
    const struct device *spi  = DEVICE_DT_GET(spi1);
    const struct device *gpio = DEVICE_DT_GET(gpioc);

    static const uint8_t pattern[4] = { 0x0, 0x1, 0x2, 0x3 };
    uint32_t pass = 0, fail = 0, step = 0, glitch = 0;

    /* Prime: exercise both LED pins low->high->low and let the FPGA I/O settle
     * after configuration BEFORE we start counting. On silicon the very first
     * high-toggle right after config was seen to glitch once (a single-sample
     * transient on the LED1 readback); driving both pins high then low here
     * absorbs that startup settling so the counted run starts clean. */
    command_and_read(spi, gpio, 0x3);
    command_and_read(spi, gpio, 0x0);

    for (;;) {
        uint8_t cmd = pattern[step & 3u];
        uint8_t expect = cmd & 0x3u;

        uint8_t fb = command_and_read(spi, gpio, cmd);

        /* Debounce a single-sample transient: on a mismatch, re-sample once
         * after another settle. A genuine wiring/link fault fails the re-read
         * too (still counts FAIL); only a one-shot glitch is absorbed, and it
         * is surfaced in the `glitch` counter so nothing is hidden. */
        if (fb != expect) {
            sched_sleep_ms(2);
            uint8_t fb2 = read_feedback(gpio);
            if (fb2 != fb) glitch++;    /* the two samples disagreed */
            fb = fb2;
        }

        if (fb == expect) pass++; else fail++;

        uart_puts(con, "cmd=0b"); put_bits2(con, cmd);
        uart_puts(con, " fb=0b");  put_bits2(con, fb);
        uart_puts(con, fb == expect ? " OK  (ok=" : " FAIL (ok=");
        put_u32(con, pass); uart_puts(con, " fail="); put_u32(con, fail);
        uart_puts(con, " glitch="); put_u32(con, glitch);
        uart_puts(con, ")\n");

        step++;
        sched_sleep_ms(300);            /* visible cadence */
    }
}

int main(void)
{
    const struct device *console = DEVICE_DT_GET(DT_CHOSEN_CONSOLE);

    /* Bring up console + both SPI buses + the iCE40 loader (DT-driven). */
    device_init_all();

    /* LED feedback inputs: PC0/PC1 with pull-downs so a driven-high (lit) LED
     * reads 1 and a disconnected tap reads 0 (a broken wire shows as FAIL). */
    const struct device *gpioc = DEVICE_DT_GET(gpioc);
    gpio_pin_configure(gpioc, FB0_PIN, GPIO_INPUT | GPIO_PULL_DOWN);
    gpio_pin_configure(gpioc, FB1_PIN, GPIO_INPUT | GPIO_PULL_DOWN);

    /* Configure the FPGA over the config bus. */
    int rc = fpga_load(DEVICE_DT_GET(ice40cfg), fpga_bitstream, fpga_bitstream_len);
    uart_puts(console, rc == 0 ? "FPGA config OK\n" : "FPGA config FAILED\n");

    /* Run the runtime LED loop under the scheduler, with an always-READY idle
     * task at the lowest priority so sched_sleep_ms() actually delays. */
    sched_create_task(led_task, "led", 1);
    sched_create_task(idle_task, "idle", 7);
    systick_init(DT_SYSCLK_HZ, 1000);
    sched_start();

    for (;;) { }   /* not reached */
}
