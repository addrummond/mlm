#include <button.h>
#include <config.h>
#include <em_gpio.h>
#include <em_rtc.h>
#include <time.h>

bool button_pressed()
{
    return GPIO_PinInGet(BUTTON_GPIO_PORT, BUTTON_GPIO_PIN) == 0;
}

// Should be called after it's been confirmed that the pin is currently low.
button_press check_button_press()
{
    RTC->CNT = 0;
    RTC->CTRL |= RTC_CTRL_EN;
    int v = 1;
    while (((v = GPIO_PinInGet(BUTTON_GPIO_PORT, BUTTON_GPIO_PIN)) == 0) && RTC->CNT < (RTC_FREQ * LONG_BUTTON_PRESS_MS)/1000)
        ;
    int vv = 1;
    if (v == 1 && RTC->CNT < DOUBLE_TAP_INTERVAL_MS) {
        while (((vv = GPIO_PinInGet(BUTTON_GPIO_PORT, BUTTON_GPIO_PIN)) == 1) && RTC->CNT < (RTC_FREQ * DOUBLE_TAP_INTERVAL_MS)/1000)
            ;
    }
    RTC->CTRL &= ~RTC_CTRL_EN;

    if (v == 0 && vv == 1)
        return BUTTON_PRESS_HOLD;

    while (GPIO_PinInGet(BUTTON_GPIO_PORT, BUTTON_GPIO_PIN) == 0)
        ;

    if (v == 1 && vv == 0)
        return BUTTON_PRESS_DOUBLE_TAP;
    else
        return BUTTON_PRESS_TAP;
}