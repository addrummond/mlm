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

static uint32_t clock_freq;
volatile uint32_t *DWT_CONTROL = (uint32_t *) 0xE0001000;
volatile uint32_t *DWT_CYCCNT = (uint32_t *) 0xE0001004;

void delay_ms_cyc(uint32_t ms)
{
    if (ms > 300) {
        SEGGER_RTT_printf(0, "Bad time given to cycle counter\n");
        return;
    }

    *DWT_CONTROL |= 1; // Enable cycle counter
    *DWT_CYCCNT = 0;

    if (clock_freq == 0)
        clock_freq = CMU_ClockFreqGet(cmuClock_CORE);

    uint32_t cycles = ms * clock_freq / 1000;

    while (*DWT_CYCCNT < cycles)
        __NOP(), __NOP(), __NOP(), __NOP();

    *DWT_CONTROL &= ~1U;
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