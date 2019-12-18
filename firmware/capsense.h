#ifndef CAPSENSE_H
#define CAPSENSE_H

#include <config.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

typedef enum touch_position {
    INVALID_TOUCH_POSITION = -99,
    NO_TOUCH_DETECTED = 0,
    LEFT_BUTTON = -1,
    RIGHT_BUTTON = 1,
    CENTER_BUTTON = 2
} touch_position;

void setup_capsense(void);
void disable_capsense(void);
void cycle_capsense(void);
void get_touch_count(uint32_t *chan1, uint32_t *chan2);
void calibrate_capsense();
void setup_le_capsense();
void disable_le_capsense();
touch_position get_touch_position(uint32_t chan1, uint32_t chan2, uint32_t chan3);

#define PAD_COUNT_MS       10 // we count alternations on each touch pad for this number of ms
#define LE_PAD_CLOCK_COUNT 2

#define RTC_CYCLES_PER_PAD_TOUCH_COUNT_TIMES_10 \
          ((RTC_RAW_FREQ * PAD_COUNT_MS * 10) / 1000) // * 10 for more precision (so we can then round)

#define RTC_CYCLES_PER_PAD_TOUCH_COUNT \
          (RTC_CYCLES_PER_PAD_TOUCH_COUNT_TIMES_10 % 10 >= 5 \
              ? RTC_CYCLES_PER_PAD_TOUCH_COUNT_TIMES_10/10 + 1 \
              : RTC_CYCLES_PER_PAD_TOUCH_COUNT_TIMES_10/10)

#endif