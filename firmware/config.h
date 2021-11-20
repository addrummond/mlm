#ifndef CONFIG_H
#define CONFIG_H

// Define this to disable deep sleep mode. Recommended, as it doesn't seem to be
// very successful in reducing power consumption over the regular sleep mode,
// and may occasionally cause startup issues with the boost converter.
#define DISABLE_DEEP_SLEEP

#define NORMAL_PAD_SCAN_HZ                                  45
#define SLOW_PAD_SCAN_HZ                                    1

#define LONG_PRESS_MS                                       300
#define ISO_LONG_PRESS_MS                                   600
#define DOUBLE_BUTTON_SLOP_MS                               300
#define CENTER_BUTTON_DEAD_ZONE_MS                          200

#define LE_CAPSENSE_CALIBRATION_INTERVAL_SECONDS            10
#define DEEP_SLEEP_TIMEOUT_SECONDS                          (1*60*60)
#define DEEP_SLEEP_TIMEOUT_SECONDS_DEBUG_MODE               20
#define LE_CAPSENSE_DEEP_SLEEP_CALIBRATION_INTERVAL_SECONDS 30

#define MISSES_REQUIRED_TO_BREAK_HOLD                       2

#define DISPLAY_READING_TIME_SECONDS                        20

// You'll need to tweak the logic in
// display_reading_interrupt_cycle_interrupt_handler in main.c if you change
// this.
#define LED_REFRESH_RATE_HZ                                 1200
#define CYCLES_PER_COUNTER_DECREMENT                        4

// Length of the grace period prior to capsense calibration.
#define GRACE_PERIOD_SECONDS                                8

// This is the value for a 2mm thick transparent acrylic window.
#define WINDOW_ATTENUATION_STOPS                            0.1586

#define WINDOW_FOV_ATTENUATION_STOPS                        1.54

#endif
