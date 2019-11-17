#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>

typedef enum button_press {
    BUTTON_PRESS_NONE,
    BUTTON_PRESS_HOLD,
    BUTTON_PRESS_TAP,
    BUTTON_PRESS_DOUBLE_TAP,
} button_press;

bool button_pressed(void);
button_press check_button_press(void);

#endif