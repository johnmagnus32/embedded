/*
 * log.c — Deferred logging with ring buffer
 *
 * Producer (any task):  log_printf() → formats string → writes to ring buffer
 * Consumer (log task):  drains ring buffer → sends to UART
 *
 * The ring buffer decouples the caller from slow UART transmission.
 * This is a simplified version of Zephyr's mpsc_pbuf + log thread.
 */

#include "log.h"
#include "sched.h"
#include "devicetree.h"
#include "device.h"
#include "drivers/uart.h"
#include <stdarg.h>
#include <stdint.h>

/* Ring buffer */
#define LOG_BUF_SIZE 512

static char log_buf[LOG_BUF_SIZE];
static volatile uint16_t log_head;  /* producer writes here */
static volatile uint16_t log_tail;  /* consumer reads here */

DEVICE_DT_DECLARE(DT_CHOSEN_CONSOLE);

static void buf_put(char c)
{
    uint16_t next = (log_head + 1) % LOG_BUF_SIZE;
    if (next == log_tail) return;  /* full — drop character */
    log_buf[log_head] = c;
    log_head = next;
}

static int buf_get(char *c)
{
    if (log_tail == log_head) return 0;  /* empty */
    *c = log_buf[log_tail];
    log_tail = (log_tail + 1) % LOG_BUF_SIZE;
    return 1;
}

/* Simple integer-to-string (no libc needed) */
static void put_int(int val)
{
    if (val < 0) { buf_put('-'); val = -val; }
    if (val == 0) { buf_put('0'); return; }

    char digits[10];
    int n = 0;
    while (val > 0) {
        digits[n++] = '0' + (val % 10);
        val /= 10;
    }
    while (n > 0) buf_put(digits[--n]);
}

/* Minimal printf into ring buffer (supports %s, %d, %x, %c) */
void log_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    /* Prepend task name */
    buf_put('[');
    const char *name = sched_current_name();
    while (*name) buf_put(*name++);
    buf_put(']');
    buf_put(' ');

    while (*fmt) {
        if (*fmt != '%') {
            buf_put(*fmt++);
            continue;
        }
        fmt++;
        switch (*fmt) {
        case 's': {
            const char *s = va_arg(ap, const char *);
            while (*s) buf_put(*s++);
            break;
        }
        case 'd': put_int(va_arg(ap, int)); break;
        case 'x': {
            unsigned int v = va_arg(ap, unsigned int);
            buf_put('0'); buf_put('x');
            for (int i = 28; i >= 0; i -= 4) {
                int nibble = (v >> i) & 0xF;
                buf_put(nibble < 10 ? '0' + nibble : 'a' + nibble - 10);
            }
            break;
        }
        case 'c': buf_put((char)va_arg(ap, int)); break;
        case '%': buf_put('%'); break;
        default: buf_put('%'); buf_put(*fmt); break;
        }
        fmt++;
    }
    buf_put('\n');
    va_end(ap);
}

void log_init(void)
{
    log_head = 0;
    log_tail = 0;
}

/* Log task: drain ring buffer to UART, sleep when empty */
void log_task(void)
{
    const struct device *console = DEVICE_DT_GET(DT_CHOSEN_CONSOLE);
    char c;

    while (1) {
        int count = 0;
        while (buf_get(&c) && count < 64) {
            if (c == '\n') uart_poll_out(console, '\r');
            uart_poll_out(console, c);
            count++;
        }
        if (count == 0) {
            sched_sleep_ms(5);  /* nothing to send, sleep 5ms */
        } else {
            sched_yield();      /* more might be pending, yield but stay ready */
        }
    }
}
