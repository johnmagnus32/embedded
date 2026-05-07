/*
 * display_ili9341.c — ILI9341 display driver over SPI
 *
 * Uses the SPI driver for data transfer and GPIO driver for DC pin.
 * No hardcoded register addresses — all from devicetree.
 */

#include <stdint.h>
#include "devicetree.h"
#include "device.h"
#include "drivers/display.h"
#include "drivers/spi.h"
#include "drivers/gpio.h"

struct ili9341_config {
    uint32_t dc_port;   /* GPIO port base for DC pin */
    uint8_t  dc_pin;
    uint8_t  dc_clk_bus;
    uint8_t  dc_clk_bit;
};

struct ili9341_data {
    const struct device *spi;
    const struct device *dc_gpio;
};

DEVICE_DT_DECLARE(spi1);
DEVICE_DT_DECLARE(gpioa);
DEVICE_DT_DECLARE(gpiob);

static void ili9341_send_cmd(struct ili9341_data *data,
                             const struct ili9341_config *cfg, uint8_t cmd)
{
    gpio_pin_set(data->dc_gpio, cfg->dc_pin, 0); /* DC low = command */
    spi_write_f(data->spi, &cmd, 1, SPI_HOLD_CS);
}

static void ili9341_send_data(struct ili9341_data *data,
                              const struct ili9341_config *cfg, uint8_t d)
{
    gpio_pin_set(data->dc_gpio, cfg->dc_pin, 1); /* DC high = data */
    spi_write_f(data->spi, &d, 1, SPI_HOLD_CS);
}

static void ili9341_end(struct ili9341_data *data)
{
    spi_cs_release(data->spi);
}

static void ili9341_set_window(struct ili9341_data *data,
                               const struct ili9341_config *cfg,
                               uint16_t x0, uint16_t y0,
                               uint16_t x1, uint16_t y1)
{
    uint8_t buf[4];
    ili9341_send_cmd(data, cfg, 0x2A);
    buf[0] = x0 >> 8; buf[1] = x0 & 0xFF;
    buf[2] = x1 >> 8; buf[3] = x1 & 0xFF;
    gpio_pin_set(data->dc_gpio, cfg->dc_pin, 1);
    spi_write_f(data->spi, buf, 4, SPI_HOLD_CS);

    ili9341_send_cmd(data, cfg, 0x2B);
    buf[0] = y0 >> 8; buf[1] = y0 & 0xFF;
    buf[2] = y1 >> 8; buf[3] = y1 & 0xFF;
    gpio_pin_set(data->dc_gpio, cfg->dc_pin, 1);
    spi_write_f(data->spi, buf, 4, SPI_HOLD_CS);

    ili9341_send_cmd(data, cfg, 0x2C);
}

static void ili9341_fill_rect(const struct device *dev,
                              uint16_t x, uint16_t y,
                              uint16_t w, uint16_t h, uint16_t color)
{
    const struct ili9341_config *cfg = dev->config;
    struct ili9341_data *data = dev->data;
    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;

    ili9341_set_window(data, cfg, x, y, x + w - 1, y + h - 1);
    gpio_pin_set(data->dc_gpio, cfg->dc_pin, 1);
    for (uint32_t i = 0; i < (uint32_t)w * h; i++) {
        spi_write_f(data->spi, &hi, 1, SPI_HOLD_CS);
        spi_write_f(data->spi, &lo, 1, SPI_HOLD_CS);
    }
    ili9341_end(data);
}

static void ili9341_set_rotation(const struct device *dev, uint8_t rotation)
{
    const struct ili9341_config *cfg = dev->config;
    struct ili9341_data *data = dev->data;
    ili9341_send_cmd(data, cfg, 0x36);
    ili9341_send_data(data, cfg, rotation);
    ili9341_end(data);
}

static void ili9341_vsync(const struct device *dev)
{
    const struct ili9341_config *cfg = dev->config;
    struct ili9341_data *data = dev->data;
    ili9341_send_cmd(data, cfg, 0x00); /* NOP triggers emulator flush */
    ili9341_end(data);
}

static void ili9341_delay_ms(uint32_t ms)
{
    /* Simple busy-wait; used only during init before scheduler starts */
    for (volatile uint32_t i = 0; i < ms * 2000; i++);
}

static int ili9341_init(const struct device *dev)
{
    const struct ili9341_config *cfg = dev->config;
    struct ili9341_data *data = dev->data;

    data->spi = DEVICE_DT_GET(spi1);
    data->dc_gpio = DEVICE_DT_GET(gpiob);

    /* Configure DC pin as output */
    gpio_pin_configure(data->dc_gpio, cfg->dc_pin, GPIO_OUTPUT);

    /* ILI9341 init sequence */
    ili9341_send_cmd(data, cfg, 0x01);  /* Software reset */
    ili9341_end(data);
    ili9341_delay_ms(120);

    ili9341_send_cmd(data, cfg, 0x28);  /* Display OFF during config */
    ili9341_end(data);

    ili9341_send_cmd(data, cfg, 0x3A);  /* Pixel format */
    ili9341_send_data(data, cfg, 0x55); /* 16-bit RGB565 */
    ili9341_end(data);

    ili9341_send_cmd(data, cfg, 0x36);  /* Memory Access Control */
    ili9341_send_data(data, cfg, 0x48); /* MY + BGR (matches panel subpixel order) */
    ili9341_end(data);

    ili9341_send_cmd(data, cfg, 0x11);  /* Sleep out */
    ili9341_end(data);
    ili9341_delay_ms(120);

    ili9341_send_cmd(data, cfg, 0x29);  /* Display ON */
    ili9341_end(data);

    return 0;
}

static const struct display_driver_api ili9341_api = {
    .fill_rect    = ili9341_fill_rect,
    .set_rotation = ili9341_set_rotation,
    .vsync        = ili9341_vsync,
};

/* ---- DT_INST instantiation ---- */

#define ILI9341_DEFINE(n)                                               \
    static const struct ili9341_config ili9341_cfg_##n = {              \
        .dc_port    = DT_INST_ILITEK_ILI9341_##n##_PROP_DC_PORT_BASE, \
        .dc_pin     = DT_INST_ILITEK_ILI9341_##n##_PROP_DC_PIN,       \
        .dc_clk_bus = 0,                                               \
        .dc_clk_bit = 0,                                               \
    };                                                                  \
    static struct ili9341_data ili9341_data_##n;                        \
    DEVICE_DT_DEFINE(DT_INST_ILITEK_ILI9341_##n##_LABEL,              \
                     ili9341_init, &ili9341_data_##n,                   \
                     &ili9341_cfg_##n, &ili9341_api);

DT_INST_FOREACH_STATUS_OKAY(ILITEK_ILI9341, ILI9341_DEFINE)
