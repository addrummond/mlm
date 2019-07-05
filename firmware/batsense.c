#include <batsense.h>
#include <batsense.h>
#include <config.h>
#include <em_gpio.h>
#include <em_rtc.h>
#include <time.h>

static const int PERIOD = (RTC_FREQ * 200) / 1000; // 1000 ms

int get_battery_voltage()
{
    return 0;
}