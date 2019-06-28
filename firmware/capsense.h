#ifndef CAPSENSE_H
#define CAPSENSE_H

#include <stdint.h>

extern uint32_t touch_counts[4];
extern uint32_t touch_acmp;
extern uint32_t touch_chan;
extern uint32_t touch_index;

void setup_capsense(void);
void cycle_capsense(void);
void clear_capcounts(void);

#endif