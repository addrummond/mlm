#include <em_rtc.h>
#include <rtt.h>

// The RTC interrupt seems to be triggered immediately by the configuration code
// in turn_on_wake timer for some reason. That's a bug either in this code or in
// the libraries / chip. To work around this, we ignore the interrupt when it's
// first triggered via the following global flag. Bit of a hack, but I can't
// see anything wrong with the way the interrupt is set up in
// turn_on_wake_timer.
static bool RTC_IRQHandler_first_time;

static void (*rtc_count_callbacks[4])(void);

void RTC_IRQHandler()
{
    RTC_IntClear(RTC_IFC_COMP0);

    if (RTC_IRQHandler_first_time) {
        RTC_IRQHandler_first_time = false;
        return;
    }

    for (int i = 0; i < sizeof(rtc_count_callbacks)/sizeof(rtc_count_callbacks[0]); ++i) {
        if (rtc_count_callbacks[i])
            rtc_count_callbacks[i]();
    }
}

void add_rtc_interrupt_handler(void (*callback)(void))
{
    RTC_IRQHandler_first_time = true;
    int i;
    for (i = 0; i < sizeof(rtc_count_callbacks)/sizeof(rtc_count_callbacks[0]); ++i) {
        if (!rtc_count_callbacks[i]) {
            rtc_count_callbacks[i] = callback;
            break;
        }
    }
    if (i == sizeof(rtc_count_callbacks)/sizeof(rtc_count_callbacks[0]))
        SEGGER_RTT_printf(0, "WARNING: RTC count callbacks array full\n");
}

void remove_rtc_interrupt_handler(void (*callback)(void))
{
    int i, j;
    for (i = 0, j = 0; i < sizeof(rtc_count_callbacks)/sizeof(rtc_count_callbacks[0]); ++i) {
        if (rtc_count_callbacks[j] != callback) {
            rtc_count_callbacks[j] = rtc_count_callbacks[i];
            ++j;
        }
    }
    for (; j < i; ++j)
        rtc_count_callbacks[j] = 0;
}

void clear_rtc_interrupt_handlers()
{
    for (int i = 0; i < sizeof(rtc_count_callbacks)/sizeof(rtc_count_callbacks[0]); ++i)
        rtc_count_callbacks[i] = 0;
}