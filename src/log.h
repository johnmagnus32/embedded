/*
 * log.h — Deferred logging
 *
 * log_printf() writes to a ring buffer and returns immediately.
 * The log task drains the buffer to UART in the background.
 * Same pattern as Zephyr's LOG_INF → mpsc_pbuf → log thread.
 */

#ifndef LOG_H
#define LOG_H

/* Initialize logging (call before scheduler starts) */
void log_init(void);

/* Non-blocking: writes to ring buffer, returns immediately */
void log_printf(const char *fmt, ...);

/* The log task entry point (runs as a scheduler task) */
void log_task(void);

#endif
