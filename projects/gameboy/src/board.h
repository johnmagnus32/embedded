/*
 * board.h — Board-level device pointers and utilities
 */

#ifndef BOARD_H
#define BOARD_H

#include "device.h"
#include <stdint.h>

/* Device pointers (initialized in main.c) */
extern const struct device *uart;
extern const struct device *display;
extern const struct device *audio_dev;
extern const struct device *dev_gpiob;
extern const struct device *dev_gpioc;

/* Utility (main.c) */
void uart_print(const char *s);
void print_int(int n);

#endif
