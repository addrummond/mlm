#include <capsense.h>
#include <config.h>
#include <em_cmu.h>
#include <em_emu.h>
#include <em_gpio.h>
#include <em_rtc.h>
#include <em_timer.h>
#include <leds.h>
#include <rtt.h>
#include <time.h>

static void gpio_pins_to_initial_states()
{
    // Setting pins to input with a pull up as the default should minimize power consumption.
    GPIO_PinModeSet(BATSENSE_PORT, BATSENSE_PIN, gpioModeInputPull, 1);
    GPIO_PinModeSet(gpioPortF, 0, gpioModeInputPull, 1);
    GPIO_PinModeSet(gpioPortF, 1, gpioModeInputPull, 1);
    GPIO_PinModeSet(gpioPortC, 14, gpioModeInputPull, 1);
    //GPIO_PinModeSet(gpioPortD, 7, gpioModeInputPull, 1);
    //GPIO_PinModeSet(gpioPortD, 6, gpioModeInputPull, 1);
    GPIO_PinModeSet(gpioPortB, 14, gpioModeInputPull, 1);
    GPIO_PinModeSet(gpioPortB, 13, gpioModeInputPull, 1);
    //GPIO_PinModeSet(gpioPortB, 11, gpioModeInputPull, 0);
    //GPIO_PinModeSet(gpioPortB, 8, gpioModeInputPull, 1);
    //GPIO_PinModeSet(gpioPortB, 7, gpioModeInputPull, 1);
    GPIO_PinModeSet(gpioPortC, 0, gpioModeInputPull, 1);
    GPIO_PinModeSet(gpioPortC, 1, gpioModeInputPull, 1);
    //GPIO_PinModeSet(gpioPortA, 0, gpioModeInputPull, 1);
    GPIO_PinModeSet(gpioPortE, 13, gpioModeInputPull, 1);
    GPIO_PinModeSet(gpioPortE, 12, gpioModeInputPull, 1);

    // leaving the DPINs as floating seems to work better
#define M(n) GPIO_PinModeSet(DPIN ## n ## _GPIO_PORT, DPIN ## n ## _GPIO_PIN, gpioModeInput, 0);
    DPIN_FOR_EACH(M)
#undef M

    // The regmode pin has external pulldown. Activating the internal
    // pulldown too could cause a small current to flow (if the EFM32
    // ground isn't at exactly the level of the regulator ground).
    GPIO_PinModeSet(gpioPortF, 2, gpioModeInput, 0);
}

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
#if !defined(DEBUG) && !defined(NOGRACE)
    leds_on(23);
    uint32_t base = leds_on_for_cycles;
    while (leds_on_for_cycles < base + RTC_RAW_FREQ * 8)
        ;
    leds_all_off();
#endif

    setup_capsense();
    calibrate_capsense();
    disable_capsense();
}