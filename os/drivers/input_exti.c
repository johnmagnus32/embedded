/*
 * input_exti.c — EXTI button input driver
 *
 * Configures EXTI rising-edge interrupts for button pins.
 * Reads button state via GPIO driver.
 */

#include <stdint.h>
#include "devicetree.h"
#include "device.h"
#include "drivers/input.h"
#include "drivers/gpio.h"

#define EXTI_IMR_OFF   0x00
#define EXTI_RTSR_OFF  0x08
#define EXTI_PR_OFF    0x14
#define NVIC_ISER0     (*(volatile uint32_t *)0xE000E100)

#define REG(base, off) (*(volatile uint32_t *)((base) + (off)))

struct input_exti_config {
    uint32_t exti_base;
    uint32_t gpio_port;   /* GPIO port base for reading pins */
    uint8_t  gpio_clk_bus;
    uint8_t  gpio_clk_bit;
    uint8_t  pin_mask;    /* bitmask of pins 0-4 */
};

struct input_exti_data {
    const struct device *gpio;
};

/* Application callback — set by app, called from ISR */
input_callback_t input_app_callback;

DEVICE_DT_DECLARE(gpiob);

static int input_exti_is_pressed(const struct device *dev, uint8_t pin)
{
    struct input_exti_data *data = dev->data;
    return gpio_pin_get(data->gpio, pin);
}

static int input_exti_init(const struct device *dev)
{
    const struct input_exti_config *cfg = dev->config;
    struct input_exti_data *data = dev->data;

    data->gpio = DEVICE_DT_GET(gpiob);

    /* Configure pins as input with pull-up */
    for (uint8_t i = 0; i < 5; i++) {
        if (cfg->pin_mask & (1 << i))
            gpio_pin_configure(data->gpio, i, GPIO_INPUT | GPIO_PULL_UP);
    }

    /* EXTI: rising edge on lines 0-4 */
    REG(cfg->exti_base, EXTI_RTSR_OFF) = cfg->pin_mask;
    REG(cfg->exti_base, EXTI_IMR_OFF)  = cfg->pin_mask;

    /* Enable EXTI0-4 in NVIC (IRQ 6-10) */
    NVIC_ISER0 = (1 << 6) | (1 << 7) | (1 << 8) | (1 << 9) | (1 << 10);

    return 0;
}

/* ISR handlers — clear pending and call app callback */
static void exti_common_handler(uint8_t pin, uint32_t exti_base)
{
    REG(exti_base, EXTI_PR_OFF) = (1 << pin);
    if (input_app_callback)
        input_app_callback(pin);
}

/* These symbols must match the vector table names */
void exti0_handler(void) { exti_common_handler(0, DT_BUTTONS_BASE); }
void exti1_handler(void) { exti_common_handler(1, DT_BUTTONS_BASE); }
void exti2_handler(void) { exti_common_handler(2, DT_BUTTONS_BASE); }
void exti3_handler(void) { exti_common_handler(3, DT_BUTTONS_BASE); }
void exti4_handler(void) { exti_common_handler(4, DT_BUTTONS_BASE); }

static const struct input_driver_api input_exti_api = {
    .is_pressed = input_exti_is_pressed,
};

/* ---- DT_INST instantiation ---- */

#define INPUT_EXTI_DEFINE(n)                                            \
    static const struct input_exti_config input_exti_cfg_##n = {       \
        .exti_base   = DT_INST_ST_STM32_EXTI_##n##_REG_ADDR,         \
        .gpio_port   = DT_INST_ST_STM32_EXTI_##n##_PROP_GPIO_PORT_BASE, \
        .gpio_clk_bus = 0,                                             \
        .gpio_clk_bit = 1,                                             \
        .pin_mask    = DT_INST_ST_STM32_EXTI_##n##_PROP_PIN_MASK,     \
    };                                                                  \
    static struct input_exti_data input_exti_data_##n;                  \
    DEVICE_DT_DEFINE(DT_INST_ST_STM32_EXTI_##n##_LABEL,               \
                     input_exti_init, &input_exti_data_##n,             \
                     &input_exti_cfg_##n, &input_exti_api);

DT_INST_FOREACH_STATUS_OKAY(ST_STM32_EXTI, INPUT_EXTI_DEFINE)
