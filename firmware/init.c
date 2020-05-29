#include <capsense.h>
#include <config.h>
#include <em_cmu.h>
#include <em_emu.h>
#include <em_gpio.h>
#include <em_rmu.h>
#include <em_rtc.h>
#include <em_timer.h>
#include <em_wdog.h>
#include <leds.h>
#include <myemu.h>
#include <rtt.h>
#include <time.h>

void gpio_pins_to_initial_states(bool include_capsense)
{
    // We disable debug pins a bit later on if the debugger is not attached.

    if (include_capsense) {
        GPIO_PinModeSet(gpioPortC, 0, gpioModeDisabled, 0);
        GPIO_PinModeSet(gpioPortC, 1, gpioModeDisabled, 0);
        GPIO_PinModeSet(gpioPortC, 14, gpioModeDisabled, 0);
    }

    GPIO_PinModeSet(BATSENSE_PORT, BATSENSE_PIN, gpioModeDisabled, 0);
    GPIO_PinModeSet(gpioPortB, 14, gpioModeDisabled, 0);
    GPIO_PinModeSet(gpioPortB, 13, gpioModeDisabled, 0);
    GPIO_PinModeSet(gpioPortE, 13, gpioModeDisabled, 0);
    GPIO_PinModeSet(gpioPortE, 12, gpioModeDisabled, 0);
#define M(n) GPIO_PinModeSet(DPIN ## n ## _GPIO_PORT, DPIN ## n ## _GPIO_PIN, gpioModeDisabled, 0);
    DPIN_FOR_EACH(M)
#undef M
    GPIO_PinModeSet(gpioPortF, 2, gpioModeDisabled, 0);
}

static void low_power_init_wait()
{
    // Leave a generous ~200ms for the boost converter to stabilize in EM2.
    // Drawing too much current immediately can cause the converter to shut down
    // upon insertion of a battery that isn't fully charged.

    RTC_Init_TypeDef rtc_init = {
        true, // Start counting when initialization is done
        false, // Enable updating during debug halt.
        false  // Restart counting from 0 when reaching COMP0.
    };

    CMU_ClockEnable(cmuClock_CORELE, true);
    CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_LFRCO);
    set_rtc_clock_div(cmuClkDiv_2048);
    CMU_ClockEnable(cmuClock_RTC, true);
    RTC_Init(&rtc_init);
    RTC_IntEnable(RTC_IEN_COMP0);
    NVIC_EnableIRQ(RTC_IRQn);
    RTC_CompareSet(0, RTC->CNT + RTC_RAW_FREQ/5/2048);
    RTC_IntClear(RTC_IFC_COMP0);

    my_emu_enter_em2(false);
    NVIC_DisableIRQ(RTC_IRQn);
}

void rtc_init()
{
    RTC_Init_TypeDef rtc_init = {
        false, // Start counting when initialization is done
        false, // Enable updating during debug halt.
        false  // Restart counting from 0 when reaching COMP0.
    };
    RTC_Init(&rtc_init);
    set_rtc_clock_div(RTC_CLK_DIV);
}

void common_init(bool watchdog_wakeup)
{
    // https://www.silabs.com/community/mcu/32-bit/forum.topic.html/happy_gecko_em4_conf-Y9Bw

    // necessary to ensure boost converter stability
    if (! watchdog_wakeup)
        low_power_init_wait();

    CMU_ClockEnable(cmuClock_HFPER, true);
    CMU_ClockEnable(cmuClock_GPIO, true);
    CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_LFRCO);
    CMU_ClockEnable(cmuClock_CORELE, true);
    CMU_OscillatorEnable(cmuOsc_LFRCO, true, true);
    CMU_ClockSelectSet(cmuClock_RTC, cmuSelect_LFRCO);

    if (! watchdog_wakeup) {
        CMU_ClockEnable(cmuClock_RTC, true);
        rtc_init();
    }

    rtt_init();
    SEGGER_RTT_printf(0, "\n\nCore clock freq = %u.\n", CMU_ClockFreqGet(cmuClock_CORE));

    if (! watchdog_wakeup)
        gpio_pins_to_initial_states(true);
}