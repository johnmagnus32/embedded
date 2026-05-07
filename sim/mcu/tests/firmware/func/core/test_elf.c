/*
 * test_elf.c — Verify ELF loader initializes .data and .bss correctly.
 *
 * Uses a custom startup that copies .data from flash to RAM and zeros .bss,
 * then checks that initialized globals have correct values and BSS is zero.
 */
#define TEST_CUSTOM_VECTORS
#include "test.h"

/* Initialized data — should be in .data (copied from flash to RAM) */
static int data_var = 0x12345678;
static short data_array[] = { 10, 20, 30, 40 };
static const char rodata_str[] = "RODATA";  /* .rodata stays in flash */

/* BSS — should be zeroed */
static int bss_var;
static int bss_array[4];

extern unsigned int _stack_top;
extern unsigned int _data_start, _data_end, _data_flash;
extern unsigned int _bss_start, _bss_end;

void test_main(void);

void __attribute__((naked)) reset_handler(void)
{
    __asm volatile(
        /* Copy .data from flash to RAM */
        "ldr r0, =_data_start\n"
        "ldr r1, =_data_end\n"
        "ldr r2, =_data_flash\n"
        "1: cmp r0, r1\n"
        "bge 2f\n"
        "ldr r3, [r2], #4\n"
        "str r3, [r0], #4\n"
        "b 1b\n"
        "2:\n"
        /* Zero .bss */
        "ldr r0, =_bss_start\n"
        "ldr r1, =_bss_end\n"
        "movs r3, #0\n"
        "3: cmp r0, r1\n"
        "bge 4f\n"
        "str r3, [r0], #4\n"
        "b 3b\n"
        "4:\n"
        "bl test_main\n"
        "b .\n"
    );
}

__attribute__((section(".vector_table"), used))
void (* const vectors[])(void) = {
    (void (*)(void))&_stack_top,
    reset_handler,
};

void test_main(void)
{
    TEST("data_init");
    CHECK(data_var == 0x12345678);

    TEST("data_array_init");
    CHECK(data_array[0] == 10);
    CHECK(data_array[1] == 20);
    CHECK(data_array[2] == 30);
    CHECK(data_array[3] == 40);

    TEST("bss_zeroed");
    CHECK(bss_var == 0);
    CHECK(bss_array[0] == 0);
    CHECK(bss_array[1] == 0);
    CHECK(bss_array[2] == 0);
    CHECK(bss_array[3] == 0);

    TEST("rodata_in_flash");
    /* rodata should be at a flash address (0x08xxxxxx) */
    unsigned int addr = (unsigned int)rodata_str;
    CHECK(addr >= 0x08000000 && addr < 0x08080000);
    CHECK(rodata_str[0] == 'R');
    CHECK(rodata_str[5] == 'A');

    TEST("data_in_ram");
    /* .data variables should be in RAM (0x20xxxxxx) */
    unsigned int daddr = (unsigned int)&data_var;
    CHECK(daddr >= 0x20000000 && daddr < 0x20020000);

    TEST("bss_in_ram");
    unsigned int baddr = (unsigned int)&bss_var;
    CHECK(baddr >= 0x20000000 && baddr < 0x20020000);

    TEST_DONE("elf");
}
