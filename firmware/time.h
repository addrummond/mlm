#ifndef TIME_H
#define TIME_H

#include <em_cmu.h>
#include <macroutils.h>
#include <stdint.h>

#define CPU_CLOCK_FREQ_HZ 14000000

#define RTC_CLK_DIV       32 // we use this whenever possible to save power
#define RTC_RAW_FREQ      32768
#define RTC_FREQ          (RTC_RAW_FREQ/RTC_CLK_DIV)
#define RTC_CMU_CLK_DIV   MACROUTILS_CONCAT(cmuClkDiv_, RTC_CLK_DIV)

uint32_t delay_ms(int ms);
uint32_t delay_ms_cyc_func(uint32_t tocks, uint32_t tick_cycles);
void set_rtc_clock_div_func(CMU_ClkDiv_TypeDef div, unsigned rightshift);
int get_rtc_freq(void);

// Counting too high on DWT_CYCCNT seems to lead to obscure problems.
#define delay_ms_cyc(ms) delay_ms_cyc_func((ms) / 256, (((ms) % 256) * (CPU_CLOCK_FREQ_HZ / 1000)))

#define RTC_CLOCK_DIV_RIGHTSHIFT_1     0
#define RTC_CLOCK_DIV_RIGHTSHIFT_32    5
#define RTC_CLOCK_DIV_RIGHTSHIFT_2048  11
#define RTC_CLOCK_DIV_RIGHTSHIFT_32768 15

// Need two layers of macro expansion so that e.g. cmuClkDiv_32768 has the opportunity to expand to 32768.
#define set_rtc_clock_div_(div) set_rtc_clock_div_func(div, MACROUTILS_EVAL(RTC_CLOCK_DIV_RIGHTSHIFT_ ## div))
#define set_rtc_clock_div(div) set_rtc_clock_div_(div)

#endif