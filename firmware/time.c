#include <config.h>
#include <em_cmu.h>
#include <em_rtc.h>
#include <rtt.h>
#include <time.h>

static int clock_div_rightshift;

// Returns the actual time delayed for in 16ths of a millisecond.
uint32_t delay_ms(int ms)
{
    uint32_t endValue = (ms * (RTC_RAW_FREQ >> clock_div_rightshift)) / 1000;
    RTC->CNT = 0;

    RTC->CTRL |= RTC_CTRL_EN;

    uint32_t cnt;
    while ((cnt = RTC->CNT) < endValue)
        ;

    RTC->CTRL &= ~RTC_CTRL_EN;

    return ((cnt * 16) * 1000) / (RTC_RAW_FREQ >> clock_div_rightshift);
}

volatile uint32_t *DWT_CONTROL = (uint32_t *)0xE0001000;
volatile uint32_t *DWT_CYCCNT = (uint32_t *)0xE0001004;

// Returns the actual time delayed for in 16ths of a millisecond.
uint32_t delay_ms_cyc_func(uint32_t tocks, uint32_t tick_cycles)
{
    *DWT_CONTROL |= 1U; // Enable cycle counter

    for (; tocks > 0; --tocks) {
        *DWT_CYCCNT = 0;

        while (*DWT_CYCCNT < CPU_CLOCK_FREQ_HZ/1000)
            __NOP(), __NOP(), __NOP(), __NOP();
    }

    uint32_t last;

    *DWT_CYCCNT = 0;
    while ((last = *DWT_CYCCNT) < tick_cycles) {
        __NOP(), __NOP(), __NOP(), __NOP();
    }

    *DWT_CONTROL ^= 1U;

    return ((tocks * 256) * 16) + ((last * 16) / (CPU_CLOCK_FREQ_HZ/1000));
}

void set_rtc_clock_div_func(CMU_ClkDiv_TypeDef div, unsigned rightshift)
{
    CMU_ClockDivSet(cmuClock_RTC, div);
    clock_div_rightshift = rightshift;
}

int get_rtc_freq()
{
    return RTC_RAW_FREQ >> clock_div_rightshift;
}