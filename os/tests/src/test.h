/*
 * test.h — Minimal test framework
 */
#ifndef TEST_H
#define TEST_H

extern void uart_print(const char *s);
extern void print_int(int n);

extern int test_pass_count;
extern int test_fail_count;

#define TEST_ASSERT(cond, msg) do { \
    if (cond) { test_pass_count++; } \
    else { test_fail_count++; uart_print("  FAIL: "); uart_print(msg); uart_print("\n"); } \
} while (0)

#define TEST_ASSERT_EQ(a, b, msg) do { \
    int _a = (a), _b = (b); \
    if (_a == _b) { test_pass_count++; } \
    else { test_fail_count++; uart_print("  FAIL: "); uart_print(msg); \
           uart_print(" (got "); print_int(_a); uart_print(", expected "); print_int(_b); uart_print(")\n"); } \
} while (0)

#define TEST_ASSERT_RANGE(val, lo, hi, msg) do { \
    int _v = (val); \
    if (_v >= (lo) && _v <= (hi)) { test_pass_count++; } \
    else { test_fail_count++; uart_print("  FAIL: "); uart_print(msg); \
           uart_print(" (got "); print_int(_v); uart_print(", expected "); \
           print_int(lo); uart_print("-"); print_int(hi); uart_print(")\n"); } \
} while (0)

#define TEST_SUITE(name) uart_print("--- " name " ---\n")

#endif
