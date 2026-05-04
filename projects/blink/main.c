/*
 * blink.c — Bare-metal LED blink + UART for STM32F411RE Nucleo
 * LD2 = PA5, USART1 TX = PA9, RX = PA10
 */
#include <stdint.h>

/* RCC */
#define RCC_AHB1ENR  (*(volatile uint32_t *)0x40023830)
#define RCC_APB2ENR  (*(volatile uint32_t *)0x40023844)

/* GPIOA */
#define GPIOA_MODER  (*(volatile uint32_t *)0x40020000)
#define GPIOA_AFRH   (*(volatile uint32_t *)0x40020024)
#define GPIOA_ODR    (*(volatile uint32_t *)0x40020014)

/* USART1 (on APB2) */
#define USART1_SR    (*(volatile uint32_t *)0x40011000)
#define USART1_DR    (*(volatile uint32_t *)0x40011004)
#define USART1_BRR   (*(volatile uint32_t *)0x40011008)
#define USART1_CR1   (*(volatile uint32_t *)0x4001100C)

extern uint32_t _stack_top;
void reset_handler(void);

__attribute__((section(".vector_table"), used))
void (* const vectors[])(void) = {
    (void (*)(void))&_stack_top,
    reset_handler,
};

static void delay(volatile uint32_t count)
{
    while (count--) ;
}

static void uart_putc(char c)
{
    while (!(USART1_SR & (1 << 7))) ;
    USART1_DR = c;
}

static void uart_puts(const char *s)
{
    while (*s) uart_putc(*s++);
}

void reset_handler(void)
{
    /* Enable GPIOA + USART1 clocks */
    RCC_AHB1ENR |= (1 << 0);       /* GPIOA */
    RCC_APB2ENR |= (1 << 4);       /* USART1 */
    delay(10);

    /* PA5 = output (LED) */
    GPIOA_MODER &= ~(3 << 10);
    GPIOA_MODER |=  (1 << 10);

    /* PA9 = AF7 (USART1_TX), PA10 = AF7 (USART1_RX) */
    GPIOA_MODER &= ~((3 << 18) | (3 << 20));
    GPIOA_MODER |=  ((2 << 18) | (2 << 20));
    GPIOA_AFRH  &= ~((0xF << 4) | (0xF << 8));
    GPIOA_AFRH  |=  ((7 << 4) | (7 << 8));

    /* USART1: 115200 baud at 16MHz HSI (APB2 = 16MHz) */
    USART1_BRR = 0x008B;
    USART1_CR1 |= (1 << 13);
    delay(10);
    USART1_CR1 |= (1 << 3) | (1 << 2);

    int count = 0;
    while (1) {
        GPIOA_ODR ^= (1 << 5);
        uart_puts("blink ");
        char buf[12];
        int i = 11;
        buf[i] = 0;
        int v = count;
        if (v == 0) { buf[--i] = '0'; }
        else { while (v > 0) { buf[--i] = '0' + (v % 10); v /= 10; } }
        uart_puts(&buf[i]);
        uart_puts("\r\n");
        count++;
        delay(500000);
    }
}
