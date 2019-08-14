#ifndef CAPSENSE_H
#define CAPSENSE_H

#include <config.h>
#include <stdbool.h>
#include <stdint.h>

extern uint32_t touch_counts[4];
extern uint32_t touch_acmp;
extern uint32_t touch_chan;
extern uint32_t touch_index;
extern bool touch_on;

void setup_capsense(void);
void disable_capsense(void);
void cycle_capsense(void);
void clear_capcounts(void);

#define NO_TOUCH_DETECTED      99
#define INVALID_TOUCH_POSITION 999

#define PAD_COUNT_MS 10 // we count alternations on each touch pad for this number of ms

#define LED_CYCLES_PER_PAD_TOUCH_COUNT_TIMES_10 \
          ((LED_REFRESH_RATE_HZ * PAD_COUNT_MS * 10) / 1000) // * 10 for more precision (so we can then round)

#define LED_CYCLES_PER_PAD_TOUCH_COUNT \
          (LED_CYCLES_PER_PAD_TOUCH_COUNT_TIMES_10 % 10 >= 5 \
              ? LED_CYCLES_PER_PAD_TOUCH_COUNT_TIMES_10/10 + 1 \
              : LED_CYCLES_PER_PAD_TOUCH_COUNT_TIMES_10/10)

// Get the current finger position as a value from
// -10 (leftmost) to 10 (rightmost), or NO_TOUCH_DETECTED.
int touch_position_10(void);

#endif