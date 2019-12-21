#ifndef CONFIG_H
#define CONFIG_H

#define REGMODE_PORT                           gpioPortB
#define REGMODE_PIN                            13

#define BATSENSE_PORT                          gpioPortC
#define BATSENSE_PIN                           15

#define LONG_PRESS_MS                          300

#define MISSES_REQUIRED_TO_BREAK_HOLD          1

#define IDLE_TIME_BEFORE_DEEPEST_SLEEP_SECONDS 30

#define DISPLAY_READING_TIME_SECONDS           10

// Maximum of 12 LEDs being displayed at once.
#define LED_REFRESH_RATE_HZ                    (80*12)

#endif