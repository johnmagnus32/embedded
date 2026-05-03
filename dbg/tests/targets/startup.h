/* startup.h — Minimal vector table + startup for test targets */
#ifndef STARTUP_H
#define STARTUP_H

extern unsigned int _stack_top;
void main(void);

void __attribute__((naked)) reset_handler(void)
{
    __asm volatile("bl main\n b .\n");
}

__attribute__((section(".vector_table"), used))
void (* const vectors[])(void) = {
    (void (*)(void))&_stack_top,
    reset_handler,
};

#endif
