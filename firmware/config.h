#ifndef CONFIG_H
#define CONFIG_H

#define LONG_PRESS_MS                            300
#define DOUBLE_BUTTON_SLOP_MS                    300
#define CENTER_BUTTON_DEAD_ZONE_MS               300

#define LE_CAPSENSE_CALIBRATION_INTERVAL_SECONDS 10

#define MISSES_REQUIRED_TO_BREAK_HOLD            2

#define DISPLAY_READING_TIME_SECONDS             20

#define LED_REFRESH_RATE_HZ                      1200

// This is the value for a 2mm thick transparent acrylic window.
#define WINDOW_ATTENUATION_STOPS                 0.1586

// See windowcalcs.md
#define WINDOW_FOV_ATTENUATION_STOPS             0.3468

// TODO: move these out of config.h
#define REGMODE_PORT                             gpioPortF
#define REGMODE_PIN                              2
#define BATSENSE_PORT                            gpioPortC
#define BATSENSE_PIN                             15

#endif
