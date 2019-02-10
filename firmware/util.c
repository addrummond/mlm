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

// sign_of and iabs are useful for printing signed numbers to the RTT console
// (since RTT itself doesn't handle them).

const char *sign_of(int32_t n)
{
    if (n >= 0)
        return "+";
    return "-";
}

uint32_t iabs(int32_t n)
{
    if (n >= 0)
        return (uint32_t)n;
    return (~0U - (uint32_t)n) + 1;
}