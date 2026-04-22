/* Minimal test firmware with vector table */

#define USART2_SR  (*(volatile unsigned int *)0x40004400)
#define USART2_DR  (*(volatile unsigned int *)0x40004404)

static void uart_putc(char c)
{
    while (!(USART2_SR & (1 << 7)))
        ;
    USART2_DR = c;
}

void main(void)
{
    const char *msg = "Hello from the ARM emulator!\r\n";
    while (*msg)
        uart_putc(*msg++);

    while (1) ;
}

/* Vector table */
extern unsigned int _stack_top;
void main(void);

__attribute__((section(".vector_table"), used))
void *vectors[] = {
    (void *)0x20020000,   /* Initial SP */
    (void *)main,         /* Reset handler */
};
