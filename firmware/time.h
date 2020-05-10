#ifndef TIME_H
#define TIME_H

#include <em_cmu.h>
#include <macroutils.h>

#define RTC_CLK_DIV      32 // we use this whenever possible to save power
#define RTC_RAW_FREQ     32768
#define RTC_FREQ         (RTC_RAW_FREQ/RTC_CLK_DIV)
#define RTC_CMU_CLK_DIV  MACROUTILS_CONCAT(cmuClkDiv_, RTC_CLK_DIV)

void delay_ms(int ms);
void delay_ms_cyc_func(uint32_t tocks, uint32_t tick_cycles);
void set_rtc_clock_div(CMU_ClkDiv_TypeDef div);
int get_rtc_freq(void);

// Counting too high on DWT_CYCCNT seems to lead to obscure problems.
#define delay_ms_cyc(ms) delay_ms_cyc_func((ms) / 256, (((ms) % 256) * (14000000 / 1000)))

#endif