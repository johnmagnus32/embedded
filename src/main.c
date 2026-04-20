/*
 * Bare-metal "Hello World" for STM32F411RE
 *
 * Prints to USART2 (PA2=TX), which is connected to the ST-Link
 * virtual COM port on the Nucleo board. Open a serial terminal
 * at 115200 baud to see the output.
 *
 * No HAL, no RTOS — just direct register writes.
 */

#include <stdint.h>

/* ---- Register addresses (from STM32F411 reference manual) ---- */

/* RCC (Reset and Clock Control) */
#define RCC_BASE        0x40023800
#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x30))
#define RCC_APB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x40))

/* GPIOA */
#define GPIOA_BASE      0x40020000
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_AFRL      (*(volatile uint32_t *)(GPIOA_BASE + 0x20))

/* USART2 */
#define USART2_BASE     0x40004400
#define USART2_SR       (*(volatile uint32_t *)(USART2_BASE + 0x00))
#define USART2_DR       (*(volatile uint32_t *)(USART2_BASE + 0x04))
#define USART2_BRR      (*(volatile uint32_t *)(USART2_BASE + 0x08))
#define USART2_CR1      (*(volatile uint32_t *)(USART2_BASE + 0x0C))

/* ---- UART setup ---- */

static void uart_init(void)
{
    /* Enable clocks: GPIOA (bit 0) and USART2 (bit 17) */
    RCC_AHB1ENR |= (1 << 0);   /* GPIOA clock */
    RCC_APB1ENR |= (1 << 17);  /* USART2 clock */

    /* Configure PA2 as alternate function (AF7 = USART2_TX) */
    GPIOA_MODER &= ~(3 << (2 * 2));   /* clear bits 5:4 */
    GPIOA_MODER |=  (2 << (2 * 2));   /* set to AF mode (0b10) */
    GPIOA_AFRL  &= ~(0xF << (2 * 4)); /* clear AF bits for PA2 */
    GPIOA_AFRL  |=  (7 << (2 * 4));   /* AF7 = USART2 */

    /*
     * Baud rate: 115200
     * Default clock after reset = HSI = 16 MHz
     * APB1 clock = HSI / 1 = 16 MHz (default, no prescaler)
     * BRR = fclk / baud = 16000000 / 115200 ≈ 138.89
     * Mantissa = 8, Fraction = 11 (8.6875 × 16 = 139)
     * Or just write 0x008B (integer 139 works close enough)
     */
    USART2_BRR = 0x008B;  /* 16MHz / 115200 ≈ 138.89 */

    /* Enable USART2: UE (bit 13) + TE (bit 3) */
    USART2_CR1 = (1 << 13) | (1 << 3);
}

static void uart_putc(char c)
{
    /* Wait until TXE (transmit data register empty) */
    while (!(USART2_SR & (1 << 7)))
        ;
    USART2_DR = c;
}

static void uart_puts(const char *s)
{
    while (*s) {
        if (*s == '\n')
            uart_putc('\r');
        uart_putc(*s++);
    }
}

/* ---- Main ---- */

int main(void)
{
    uart_init();

    uart_puts("Hello from bare metal STM32F411RE!\n");
    uart_puts("No RTOS, no HAL, just registers.\n");

    int count = 0;
    while (1) {
        uart_puts("tick\n");
        /* Crude delay (~1 second at 16MHz) */
        for (volatile int i = 0; i < 1600000; i++)
            ;
        count++;
    }
}
