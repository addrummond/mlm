#ifndef CONFIG_H
#define CONFIG_H

#define BUTTON_GPIO_PORT                       gpioPortF
#define BUTTON_GPIO_PIN                        2
#define BUTTON_GPIO_EM4WUEN                    GPIO_EM4WUEN_EM4WUEN_F2

#define REGMODE_PORT                           gpioPortB
#define REGMODE_PIN                            13

#define BATSENSE_PORT                          gpioPortB
#define BATSENSE_PIN                           14

#define LONG_BUTTON_PRESS_MS                   400

#define IDLE_TIME_BEFORE_DEEPEST_SLEEP_SECONDS 30

#define DISPLAY_READING_TIME_SECONDS           15

// We may be displaying up to 12 leds at once. This refresh rate should ensure
// that even in this worst case scenario each LED is cycled at 240Hz. This
// should be imperceptible to most people.
#define LED_REFRESH_RATE_HZ                    (240*12)

#endif