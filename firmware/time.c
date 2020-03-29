#include <config.h>
#include <em_rtc.h>
#include <time.h>

static int clock_div;

void delay_ms(int ms)
{
    if (clock_div == 0)
        clock_div = 1;

    uint32_t endValue = (ms * RTC_RAW_FREQ) / clock_div / 1000;
    RTC->CNT = 0;

    RTC->CTRL |= RTC_CTRL_EN;

    while (RTC->CNT < endValue)
        ;

    RTC->CTRL &= ~RTC_CTRL_EN;
}

void set_rtc_clock_div(CMU_ClkDiv_TypeDef div)
{
    CMU_ClockDivSet(cmuClock_RTC, div);
    // Note that, e.g., the value of cmuClkDiv_4096 is 4096
    clock_div = div;
}

int get_rtc_freq()
{
    return RTC_RAW_FREQ / clock_div;
}