#ifndef CONFIG_H
#define CONFIG_H

#define REGMODE_PORT                           gpioPortB
#define REGMODE_PIN                            13

#define BATSENSE_PORT                          gpioPortC
#define BATSENSE_PIN                           15

#define LONG_PRESS_MS                          300
#define DOUBLE_BUTTON_SLOP_MS                  300
#define CENTER_BUTTON_DEAD_ZONE_MS             300

#define MISSES_REQUIRED_TO_BREAK_HOLD          2

#define IDLE_TIME_BEFORE_DEEPEST_SLEEP_SECONDS 30

#define DISPLAY_READING_TIME_SECONDS           20

// Maximum of 12 LEDs being displayed at once.
#define LED_REFRESH_RATE_HZ                    1200

#endif