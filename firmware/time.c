#include <config.h>
#include <em_cmu.h>
#include <em_rtc.h>
#include <rtt.h>
#include <time.h>

static int clock_div;

// Returns the actual time delayed for in 16ths of a millisecond.
uint32_t delay_ms(int ms)
{
    if (clock_div == 0)
        clock_div = 1;

    uint32_t endValue = (ms * RTC_RAW_FREQ) / clock_div / 1000;
    RTC->CNT = 0;

    RTC->CTRL |= RTC_CTRL_EN;

    uint32_t cnt;
    while ((cnt = RTC->CNT) < endValue)
        ;

    RTC->CTRL &= ~RTC_CTRL_EN;

    return ((cnt * 16) * 1000) / (RTC_RAW_FREQ / clock_div);
}

volatile uint32_t *DWT_CONTROL = (uint32_t *)0xE0001000;
volatile uint32_t *DWT_CYCCNT = (uint32_t *)0xE0001004;

// Returns the actual time delayed for in 16ths of a millisecond.
uint32_t delay_ms_cyc_func(uint32_t tocks, uint32_t tick_cycles)
{
    for (; tocks > 0; --tocks) {
        *DWT_CONTROL |= 1; // Enable cycle counter
        *DWT_CYCCNT = 0;

        while (*DWT_CYCCNT < CPU_CLOCK_FREQ_HZ/1000)
            __NOP(), __NOP(), __NOP(), __NOP();

        *DWT_CONTROL &= ~1U;
    }

    uint32_t last;

    *DWT_CONTROL |= 1;
    *DWT_CYCCNT = 0;
    while ((last = *DWT_CYCCNT) < tick_cycles) {
        __NOP(), __NOP(), __NOP(), __NOP();
    }
    *DWT_CONTROL &= ~1U;

    return ((tocks * 256) * 16) + ((last * 16) / (CPU_CLOCK_FREQ_HZ/1000));
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