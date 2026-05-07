/*
 * test.h — Test harness using ARM semihosting.
 *
 * Tests signal pass/fail via semihosting exit codes (no UART needed).
 * Diagnostic output goes to emulator stderr via SYS_WRITE0.
 */
#ifndef TEST_H
#define TEST_H

/* Semihosting calls */
static inline void semi_exit(int code)
{
    register int r0 __asm__("r0") = 0x18; /* SYS_EXIT */
    register int r1 __asm__("r1") = code;
    __asm volatile("bkpt #0xAB" :: "r"(r0), "r"(r1));
}

static inline void semi_putc(char c)
{
    register int r0 __asm__("r0") = 0x03; /* SYS_WRITEC */
    register int r1 __asm__("r1") = (int)&c;
    __asm volatile("bkpt #0xAB" :: "r"(r0), "r"(r1));
}

static inline void semi_puts(const char *s)
{
    register int r0 __asm__("r0") = 0x04; /* SYS_WRITE0 */
    register int r1 __asm__("r1") = (int)s;
    __asm volatile("bkpt #0xAB" :: "r"(r0), "r"(r1));
}

/* Returns host wall-clock time in microseconds */
static inline unsigned int semi_clock_us(void)
{
    register int r0 __asm__("r0") = 0x10; /* SYS_CLOCK */
    register int r1 __asm__("r1") = 0;
    __asm volatile("bkpt #0xAB" : "+r"(r0) : "r"(r1));
    return r0;
}

static inline void semi_putdec(unsigned int v)
{
    char buf[11];
    int i = 10;
    buf[i] = 0;
    if (v == 0) { semi_puts("0"); return; }
    while (v > 0) { buf[--i] = '0' + (v % 10); v /= 10; }
    semi_puts(&buf[i]);
}

static int t_fail_count;
static const char *t_current;

static inline void semi_puthex(unsigned int v)
{
    char buf[9];
    for (int i = 7; i >= 0; i--) { buf[i] = "0123456789abcdef"[v & 0xf]; v >>= 4; }
    buf[8] = 0;
    semi_puts(buf);
}

/* DWT cycle counter */
#define DWT_CYCCNT  (*(volatile unsigned int *)0xE0001004)
#define DWT_CONTROL (*(volatile unsigned int *)0xE0001000)
#define DEMCR       (*(volatile unsigned int *)0xE000EDFC)

static inline void cycle_counter_init(void)
{
    DEMCR |= (1 << 24);
    DWT_CYCCNT = 0;
    DWT_CONTROL |= 1;
}

static inline unsigned int cycles(void) { return DWT_CYCCNT; }

#define CHECK(cond) do { \
    if (!(cond)) { \
        semi_puts("FAIL:"); semi_puts(t_current); \
        semi_puts(":"); semi_puts(#cond); semi_puts("\n"); \
        t_fail_count++; \
    } \
} while (0)

#define TEST(name) t_current = name

#define CHECK_RANGE(val, lo, hi) do { \
    unsigned int _v = (val); \
    if (_v < (unsigned int)(lo) || _v > (unsigned int)(hi)) { \
        semi_puts("FAIL:"); semi_puts(t_current); \
        semi_puts(": got "); semi_puthex(_v); \
        semi_puts(" expected "); semi_puthex(lo); \
        semi_puts("-"); semi_puthex(hi); semi_puts("\n"); \
        t_fail_count++; \
    } \
} while (0)

#define TEST_DONE(suite) do { \
    if (t_fail_count == 0) { \
        semi_puts("PASS:"); semi_puts(suite); semi_puts("\n"); \
        semi_exit(0); \
    } else { \
        semi_puts("FAILED:"); semi_puts(suite); semi_puts("\n"); \
        semi_exit(1); \
    } \
} while (0)

/* Vector table + startup boilerplate */
#ifndef TEST_CUSTOM_VECTORS
extern unsigned int _stack_top;
void test_main(void);

void __attribute__((naked)) reset_handler(void)
{
    __asm volatile("bl test_main\n b .\n");
}

__attribute__((section(".vector_table"), used))
void (* const vectors[])(void) = {
    (void (*)(void))&_stack_top,
    reset_handler,
};
#endif

#endif
