#include <em_rtc.h>

// The RTC interrupt seems to be triggered immediately by the configuration code
// in turn_on_wake timer for some reason. That's a bug either in this code or in
// the libraries / chip. To work around this, we ignore the interrupt when it's
// first triggered via the following global flag. Bit of a hack, but I can't
// see anything wrong with the way the interrupt is set up in
// turn_on_wake_timer.
static bool RTC_IRQHandler_first_time;

static void (*rtc_count_callback)(void);

void RTC_IRQHandler()
{
    RTC_IntClear(RTC_IFC_COMP0);

    if (RTC_IRQHandler_first_time) {
        RTC_IRQHandler_first_time = false;
        return;
    }

    if (rtc_count_callback)
        rtc_count_callback();
}

void set_rtc_interrupt_handler(void (*callback)(void))
{
    RTC_IRQHandler_first_time = true;
    rtc_count_callback = callback;
}