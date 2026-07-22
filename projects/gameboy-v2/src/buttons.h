#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>

#define BUTTON_A      0
#define BUTTON_B      1
#define BUTTON_LEFT   2
#define BUTTON_RIGHT  3
#define BUTTON_UP     4
#define BUTTON_DOWN   5
#define BUTTON_START  6
#define BUTTON_SELECT 7

void buttons_init(void);
int button_pressed(uint8_t button);
void wait_for_button_press(uint8_t button);

#endif
