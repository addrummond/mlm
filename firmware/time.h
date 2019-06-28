#ifndef TIME_H
#define TIME_H

#include <em_wdog.h>
#include <macroutils.h>

#define RTC_CLK_DIV      32
#define RTC_FREQ         (32768/RTC_CLK_DIV)
#define RTC_CMU_CLK_DIV  MACROUTILS_CONCAT(cmuClkDiv_, RTC_CLK_DIV)

void delay_ms(int ms);

#endif