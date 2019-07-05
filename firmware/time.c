#include <config.h>
#include <em_rtc.h>
#include <time.h>

void delay_ms(int ms)
{
    uint32_t endValue = (ms * RTC_FREQ) / 1000;
    RTC->CNT = 0;

    RTC->CTRL |= RTC_CTRL_EN;

    while (RTC->CNT < endValue)
        ;

    RTC->CTRL &= ~RTC_CTRL_EN;
}