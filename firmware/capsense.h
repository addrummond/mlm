#ifndef CAPSENSE_H
#define CAPSENSE_H

#include <config.h>
#include <macroutils.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

typedef enum touch_position {
    INVALID_TOUCH_POSITION = -99,
    NO_TOUCH_DETECTED = 0,
    LEFT_BUTTON = -1,
    RIGHT_BUTTON = 1,
    CENTER_BUTTON = 2,
    LEFT_AND_RIGHT_BUTTONS = 3
} touch_position;

typedef enum le_capsense_mode {
    LE_CAPSENSE_SENSE,
    LE_CAPSENSE_SLEEP
} le_capsense_mode;

typedef enum press {
    PRESS_TAP,
    PRESS_HOLD
} press;

void setup_capsense(void);
void disable_capsense(void);
void cycle_capsense(void);
uint32_t get_touch_count_func(uint32_t *chan_value, uint32_t *chan, uint32_t millisecond_sixteenths, const char *src_pos_for_debugging);
#define get_touch_count(chan_value, chan, millisecond_sixteenths) get_touch_count_func((chan_value), (chan), (millisecond_sixteenths), __FILE__ ":" MACROUTILS_SYMBOL(__LINE__))
void calibrate_capsense(void);
void calibrate_le_capsense(void);
void setup_le_capsense(le_capsense_mode mode);
void recalibrate_le_capsense(void);
void disable_le_capsense(void);
touch_position get_touch_position(uint32_t chan1, uint32_t chan2, uint32_t chan3);
bool le_center_pad_is_touched(uint32_t chan2);
press get_pad_press(touch_position touch_pos);
void reset_lesense_irq_handler_state(void);
bool check_lesense_irq_handler(void);

extern uint32_t lesense_result;

#define PAD_COUNT_MS       10 // we count alternations on each touch pad for this number of ms
#define LE_PAD_CLOCK_COUNT 2

#define RAW_RTC_CYCLES_PER_PAD_TOUCH_COUNT_TIMES_10 \
          ((RTC_RAW_FREQ * PAD_COUNT_MS * 10) / 1000) // * 10 for more precision (so we can then round)

#define RAW_RTC_CYCLES_PER_PAD_TOUCH_COUNT \
          (RAW_RTC_CYCLES_PER_PAD_TOUCH_COUNT_TIMES_10 % 10 >= 5 \
              ? RAW_RTC_CYCLES_PER_PAD_TOUCH_COUNT_TIMES_10/10 + 1 \
              : RAW_RTC_CYCLES_PER_PAD_TOUCH_COUNT_TIMES_10/10)

#endif