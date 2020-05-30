#ifndef INIT_H
#define INIT_H

#include <stdbool.h>

void common_init(bool watchdog_wakeup);
void rtc_init(void);
void gpio_pins_to_initial_states(void);


#endif