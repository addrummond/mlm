#include <config.h>
#include <em_cmu.h>
#include <em_rtc.h>
#include <rtt.h>
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

volatile uint32_t *DWT_CONTROL = (uint32_t *)0xE0001000;
volatile uint32_t *DWT_CYCCNT = (uint32_t *)0xE0001004;

void delay_ms_cyc(uint32_t ms)
{
    static uint32_t clock_freq;

    if (clock_freq == 0)
        clock_freq = CMU_ClockFreqGet(cmuClock_CORE);

    // Can't count much more than 256ms without overflow (assuming 14MHz clock)
    uint32_t tocks = ms / 256;
    uint32_t ticks = ms % 256;
    uint32_t tick_cycles = ticks * clock_freq / 1000;

    do {
        *DWT_CONTROL |= 1; // Enable cycle counter
        *DWT_CYCCNT = 0;

        while (*DWT_CYCCNT < tick_cycles)
            __NOP(), __NOP(), __NOP(), __NOP();

        *DWT_CONTROL &= ~1U;
    } while (tocks-- > 0);
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