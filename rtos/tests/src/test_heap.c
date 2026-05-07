/*
 * test_heap.c — Heap allocator tests
 */
#include "sched.h"
#include "heap.h"
#include "test.h"
#include <stdint.h>

/* We need a heap region — use a static buffer since QEMU board
 * doesn't have the same linker symbols as STM32 */
static uint8_t heap_mem[4096] __attribute__((aligned(8)));

void test_heap_run(void)
{
    TEST_SUITE("heap");

    heap_init(heap_mem, sizeof(heap_mem));

    /* Test 1: basic alloc returns non-NULL */
    void *p1 = heap_alloc(64);
    TEST_ASSERT(p1 != 0, "heap_alloc(64) should return non-NULL");

    /* Test 2: second alloc returns different pointer */
    void *p2 = heap_alloc(64);
    TEST_ASSERT(p2 != 0, "second alloc should return non-NULL");
    TEST_ASSERT(p1 != p2, "two allocs should return different pointers");

    /* Test 3: free and re-alloc */
    heap_free(p1);
    heap_free(p2);
    void *p3 = heap_alloc(64);
    TEST_ASSERT(p3 != 0, "alloc after free should succeed");

    /* Test 4: write to allocated memory doesn't corrupt */
    uint8_t *buf = (uint8_t *)heap_alloc(128);
    TEST_ASSERT(buf != 0, "alloc 128 bytes");
    for (int i = 0; i < 128; i++) buf[i] = (uint8_t)i;
    /* Alloc another block — shouldn't corrupt first */
    void *p4 = heap_alloc(128);
    int data_ok = 1;
    for (int i = 0; i < 128; i++) {
        if (buf[i] != (uint8_t)i) { data_ok = 0; break; }
    }
    TEST_ASSERT(data_ok, "alloc should not corrupt existing data");
    heap_free(buf);
    heap_free(p4);

    /* Test 5: alloc too large fails */
    void *big = heap_alloc(8192);
    TEST_ASSERT(big == 0, "alloc larger than heap should return NULL");

    /* Test 6: many small allocs then free all */
    void *ptrs[16];
    int alloc_ok = 1;
    for (int i = 0; i < 16; i++) {
        ptrs[i] = heap_alloc(32);
        if (!ptrs[i]) { alloc_ok = 0; break; }
    }
    TEST_ASSERT(alloc_ok, "16x alloc(32) should succeed in 4KB heap");
    for (int i = 0; i < 16; i++) heap_free(ptrs[i]);

    /* Test 7: after freeing all, can alloc large block again */
    heap_free(p3);
    void *large = heap_alloc(2048);
    TEST_ASSERT(large != 0, "alloc(2048) should succeed after freeing everything");
    heap_free(large);

    uart_print("  heap tests done\n");
}
