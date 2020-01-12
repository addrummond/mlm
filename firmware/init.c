#include <config.h>
#include <em_rtc.h>

static void low_power_init_wait()
{
    // Leave a generous ~200ms for the boost converter to stabilize in EM2.
    // Drawing too much current immediately can cause the converter to shut down
    // upon insertion of a battery than isn't fully charged.

    RTC_Init_TypeDef rtc_init = {
        true, // Start counting when initialization is done
        false, // Enable updating during debug halt.
        false  // Restart counting from 0 when reaching COMP0.
    };

    CMU_ClockEnable(cmuClock_CORELE, true);
    CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_LFRCO);
    CMU_ClockDivSet(cmuClock_RTC, cmuClkDiv_2048);
    CMU_ClockEnable(cmuClock_RTC, true);
    RTC_Init(&rtc_init);
    RTC_IntEnable(RTC_IEN_COMP0);
    NVIC_EnableIRQ(RTC_IRQn);
    RTC_CompareSet(0, RTC->CNT + RTC_RAW_FREQ/5/2048);
    RTC_IntClear(RTC_IFC_COMP0);

    EMU_EnterEM2(false);
}

void common_init()
{
    // https://www.silabs.com/community/mcu/32-bit/forum.topic.html/happy_gecko_em4_conf-Y9Bw

    // necessary to ensure boost converter stability
    low_power_init_wait();

    TIMER_Enable(TIMER1, false);
    CMU_ClockEnable(cmuClock_TIMER1, false);

    CMU_ClockEnable(cmuClock_HFPER, true);
    CMU_ClockEnable(cmuClock_GPIO, true);
    CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_LFRCO);
    CMU_OscillatorEnable(cmuOsc_LFRCO, true, true);
    CMU_ClockEnable(cmuClock_CORELE, true);
    CMU_ClockSelectSet(cmuClock_RTC, cmuSelect_LFRCO);
    CMU_ClockDivSet(cmuClock_RTC, RTC_CMU_CLK_DIV);
    CMU_ClockEnable(cmuClock_RTC, true);

    RTC_Init_TypeDef rtc_init = {
        false, // Start counting when initialization is done
        false, // Enable updating during debug halt.
        false  // Restart counting from 0 when reaching COMP0.
    };
    RTC_Init(&rtc_init);

    rtt_init();
    SEGGER_RTT_printf(0, "\n\nHello RTT console; core clock freq = %u.\n", CMU_ClockFreqGet(cmuClock_CORE));

    gpio_pins_to_initial_states();

    // Give a grace period before calibrating capsense, so that
    // the programming headerœ can be disconnected first.
#ifndef DEBUG
    leds_on(1);
    uint32_t base = leds_on_for_cycles;
    while (leds_on_for_cycles < base + RTC_RAW_FREQ * 8)
        ;
    leds_all_off();
#endif

    setup_capsense();
    calibrate_capsense();
    disable_capsense();
}