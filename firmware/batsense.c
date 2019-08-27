#include <batsense.h>
#include <config.h>
#include <em_gpio.h>
#include <em_rtc.h>
#include <time.h>

bool low_battery()
{
    // We pull the pin low. If the battery volate is above
    // the zener diode breakdown voltage, then we'll still
    // get a positive reading. Otherwise, we'll get a negative
    // reading indicating a low battery.

    GPIO_PinModeSet(BATSENSE_PORT, BATSENSE_PIN, gpioModeInputPullFilter, 0);
    delay_ms(2); // probably not needed, but just to make sure things stabilize
    bool r = GPIO_PinInGet(BATSENSE_PORT, BATSENSE_PIN);
    GPIO_PinModeSet(BATSENSE_PORT, BATSENSE_PIN, gpioModeDisabled, 0);
    return !r;
}