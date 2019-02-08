#include <stdint.h>
#include <efm32tg232f8.h>

#define RTC_FREQ 32768

void delay_ms(int ms)
{
    uint32_t endValue = ms * RTC_FREQ / 1000;
    RTC->CNT = 0;

    RTC->CTRL |= RTC_CTRL_EN;

    while ( RTC->CNT < endValue );

    RTC->CTRL &= ~RTC_CTRL_EN;
}