.syntax unified
.cpu cortex-m4
.thumb

/* Vector table — must be at 0x08000000 */
.section .vector_table, "a"
.word _stack_top        /* Initial stack pointer */
.word reset_handler     /* Reset handler */
.word 0                 /* NMI */
.word 0                 /* HardFault */
/* ... rest are 0 (unused for this simple example) */

.section .text
.global reset_handler
.thumb_func
reset_handler:
    /* 1. Copy .data from flash to RAM */
    ldr r0, =_data_start
    ldr r1, =_data_end
    ldr r2, =_data_flash
copy_data:
    cmp r0, r1
    bge done_data
    ldr r3, [r2], #4
    str r3, [r0], #4
    b copy_data
done_data:

    /* 2. Zero .bss */
    ldr r0, =_bss_start
    ldr r1, =_bss_end
    movs r3, #0
zero_bss:
    cmp r0, r1
    bge done_bss
    str r3, [r0], #4
    b zero_bss
done_bss:

    /* 3. Call main */
    bl main

    /* If main returns, loop forever */
hang:
    b hang
