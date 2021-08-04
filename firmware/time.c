#include <config.h>
#include <efm32tg108f32.h>
#include <core_cm3.h>
#include <em_cmu.h>
#include <em_core.h>
#include <em_rtc.h>
#include <rtt.h>
#include <time.h>

static int clock_div_rightshift;

void delay_ms_cyc_prepare_func()
{
    DWT->CYCCNT = 0;
    DWT->CTRL |= 1U;
}

// Returns the actual time delayed for in 16ths of a millisecond.
uint32_t delay_ms_cyc_loop_func(uint32_t tocks, uint32_t tick_cycles)
{
    for (; tocks > 0; --tocks) {
        DWT->CYCCNT = 0;

        while (DWT->CYCCNT < CPU_CLOCK_FREQ_HZ/1000)
            __NOP(), __NOP(), __NOP(), __NOP();
    }

    uint32_t last;

    DWT->CYCCNT = 0;
    while ((last = DWT->CYCCNT) < tick_cycles) {
        __NOP(), __NOP(), __NOP(), __NOP();
    }

    DWT->CTRL &= ~1U;

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

static CORE_DECLARE_IRQ_STATE;

void int_disable()
{
    CORE_ENTER_ATOMIC();
}

void int_enable()
{
    CORE_EXIT_ATOMIC();
}
