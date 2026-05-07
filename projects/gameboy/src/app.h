/*
 * app.h — Shared declarations for application modules
 */

#ifndef APP_H
#define APP_H

#include "device.h"
#include <stdint.h>

/* Device pointers — initialized in main.c */
extern const struct device *uart;
extern const struct device *display;
extern const struct device *audio_dev;
extern const struct device *adc_dev;
extern const struct device *dev_gpiob;
extern const struct device *dev_gpioc;

/* Utility functions */
void uart_print(const char *s);
void print_int(int n);

/* Button init — configures GPIO interrupts */
void buttons_init(void);
int button_pressed(uint8_t pin);

/* Task entry points */
void task_a(void);
void task_b(void);
void idle_task(void);
void task_game(void);
void task_audio(void);

/* SFX triggers (called from input, consumed by audio) */
void sfx_jump(void);
void sfx_beep(void);

#endif
