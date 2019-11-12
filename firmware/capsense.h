#ifndef CAPSENSE_H
#define CAPSENSE_H

#include <config.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

extern uint32_t touch_counts[2];
extern uint32_t touch_chan;
extern uint32_t touch_index;
extern bool touch_on;

void setup_capsense(void);
void disable_capsense(void);
void cycle_capsense(void);
void clear_capcounts(void);

// TODO REDEFINE THESE IN SOME SENSIBLE WAY NOW THAT THERE'S NO SLIDER
#define NO_TOUCH_DETECTED      999
#define INVALID_TOUCH_POSITION 9999

#define PAD_COUNT_MS 10 // we count alternations on each touch pad for this number of ms

#define RTC_CYCLES_PER_PAD_TOUCH_COUNT_TIMES_10 \
          ((RTC_RAW_FREQ * PAD_COUNT_MS * 10) / 1000) // * 10 for more precision (so we can then round)

#define RTC_CYCLES_PER_PAD_TOUCH_COUNT \
          (RTC_CYCLES_PER_PAD_TOUCH_COUNT_TIMES_10 % 10 >= 5 \
              ? RTC_CYCLES_PER_PAD_TOUCH_COUNT_TIMES_10/10 + 1 \
              : RTC_CYCLES_PER_PAD_TOUCH_COUNT_TIMES_10/10)

#endif