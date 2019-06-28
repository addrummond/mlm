#include <config.h>

#include <capsense.h>
#include <em_acmp.h>
#include <em_chip.h>
#include <em_cmu.h>
#include <em_emu.h>
#include <em_device.h>
#include <em_emu.h>
#include <em_gpio.h>
#include <em_prs.h>
#include <em_rtc.h>
#include <em_timer.h>
#include <leds.h>
#include <rtt.h>
#include <sensor.h>
#include <state.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <units.h>
#include <util.h>

void GPIO_EVEN_IRQHandler()
{
    GPIO_IntClear(GPIO_IntGet());

    int v = GPIO_PinInGet(BUTTON_GPIO_PORT, BUTTON_GPIO_PIN);

    //SEGGER_RTT_printf(0, "Hi!\n");
}

void sleep_awaiting_button_press()
{
    // Set pin PF2 mode to input with pull-up resistor
    GPIO_PinModeSet(BUTTON_GPIO_PORT, BUTTON_GPIO_PIN, gpioModeInputPullFilter, 1);
    GPIO->CTRL = 1;
    GPIO->EM4WUEN = BUTTON_GPIO_EM4WUEN ;
    GPIO->EM4WUPOL = 0; // Low signal is button pushed state

    GPIO->CMD = 1; // EM4WUCLR = 1, to clear all previous events

    EMU_EnterEM4();
}

void setup_button_press_interrupt()
{
    // Enable GPIO_ODD interrupt vector in NVIC
    NVIC_ClearPendingIRQ(GPIO_EVEN_IRQn);
    NVIC_EnableIRQ(GPIO_EVEN_IRQn);

    // Configure PF2 as input with pullup
    GPIO_PinModeSet(BUTTON_GPIO_PORT, BUTTON_GPIO_PIN, gpioModeInputPullFilter, 1);

    // Configure PF2 (AMR CLK) interrupt on rising or falling edge
    GPIO_IntConfig(BUTTON_GPIO_PORT, BUTTON_GPIO_PIN, false, true, true);
}

void RTC_IRQHandler()
{
    RTC_IntClear(RTC_IFC_COMP0);

    //EMU_EnterEM4();
}

void turn_on_wake_timer()
{
    RTC_Init_TypeDef init = {
        true, // Start counting when initialization is done
        true, // Enable updating during debug halt.
        true  //Restart counting from 0 when reaching COMP0.
    };
    RTC_Init(&init);

    // The interrupt from the RTC gets the highest
    // priority as the second input argument is 0.
    NVIC_SetPriority(RTC_IRQn, 0);

    // Enabling Interrupt from RTC
    RTC_IntEnable(RTC_IEN_COMP0);
    NVIC_EnableIRQ(RTC_IRQn);

    SEGGER_RTT_printf(0, "Setting compare to %u\n", IDLE_TIME_BEFORE_DEEPEST_SLEEP_SECONDS * RTC_FREQ);
    RTC_CompareSet(0, IDLE_TIME_BEFORE_DEEPEST_SLEEP_SECONDS * RTC_FREQ);
}

void handle_MODE_JUST_WOKEN()
{
    led_on(1);
    delay_ms(500);
    leds_all_off();

    // If it was a brief tap on the button, go to AWAKE_AT_REST.
    // Otherwise, if they've held the button down for a little bit,
    // start doing a reading.
    RTC->CNT = 0;
    RTC->CTRL |= RTC_CTRL_EN;
    int v = 1;
    while (((v = GPIO_PinInGet(BUTTON_GPIO_PORT, BUTTON_GPIO_PIN)) == 0) && RTC->CNT < (RTC_FREQ * LONG_BUTTON_PRESS_MS)/1000)
        ;
    RTC->CTRL &= ~RTC_CTRL_EN;
    SEGGER_RTT_printf(0, "AFT\n");
    if (v == 0) {
        // They're holding the button down.
        g_state.mode = MODE_DOING_READING;
    } else {
        // It was just a tap.
        g_state.mode = MODE_AWAKE_AT_REST;
    }
}

void handle_MODE_AWAKE_AT_REST()
{
    // Await further button presses in EM2.
    setup_button_press_interrupt();

    // TODO TODO PROBLEM STARTS HERE
    // If we've been in EM2 for a while and nothing has happened,
    // we want to go into EM4.
    turn_on_wake_timer();

    EMU_EnterEM2(true); // true = restore oscillators, clocks and voltage scaling
    g_state.mode = MODE_JUST_WOKEN;
}

void handle_MODE_DOING_READING()
{
    // TODO temporary
    g_state.mode = MODE_AWAKE_AT_REST;
}

void state_loop()
{
    for (;;) {
        switch (g_state.mode) {
            case MODE_JUST_WOKEN: {
                SEGGER_RTT_printf(0, "MODE_JUST_WOKEN\n");

                handle_MODE_JUST_WOKEN();
            } break;
            case MODE_AWAKE_AT_REST: {
                SEGGER_RTT_printf(0, "MODE_AWAKE_AT_REST\n");

                handle_MODE_AWAKE_AT_REST();
            } break;
            case MODE_DOING_READING: {
                SEGGER_RTT_printf(0, "MODE_DOING_READING\n");

                handle_MODE_DOING_READING();
            } break;
        }
    }
}

int testmain()
{
    // ********** LOW POWER INIT **********
    // https://www.silabs.com/community/mcu/32-bit/forum.topic.html/happy_gecko_em4_conf-Y9Bw

    CHIP_Init();

    CMU_ClockEnable(cmuClock_HFPER, true);
    CMU_ClockEnable(cmuClock_GPIO, true);
    CMU_ClockEnable(cmuClock_CORELE, true);
    CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_LFRCO);
    CMU_OscillatorEnable(cmuOsc_LFRCO, true, true);
    CMU_ClockSelectSet(cmuClock_RTC, cmuSelect_LFRCO);
    CMU_ClockDivSet(cmuClock_RTC, RTC_CMU_CLK_DIV);

    CMU_ClockEnable(cmuClock_RTC, true);

    GPIO_PinModeSet(BUTTON_GPIO_PORT, BUTTON_GPIO_PIN, gpioModeInputPullFilter, 1);

    SEGGER_RTT_printf(0, "Reading state from flash\n");

    read_state_from_flash();

    state_loop();

    // ********** REGULAR INIT **********

    /*CHIP_Init();

    CMU_ClockEnable(cmuClock_HFPER, true);
    CMU_ClockEnable(cmuClock_GPIO, true);

    CMU_ClockEnable(cmuClock_CORELE, true);
    CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_LFRCO);
    CMU_OscillatorEnable(cmuOsc_LFRCO, true, true);
    CMU_ClockSelectSet(cmuClock_RTC, cmuSelect_LFRCO);
    CMU_ClockEnable(cmuClock_RTC, true);

    rtt_init();
    SEGGER_RTT_printf(0, "\n\nHello RTT console; core clock freq = %u.\n", CMU_ClockFreqGet(cmuClock_CORE));

    leds_all_off();*/


    // ********** SENSOR TEST **********

    // Turn on the LDO to power up the sensor.
    /*GPIO_PinModeSet(REGMODE_PORT, REGMODE_PIN, gpioModePushPull, 1);
    SEGGER_RTT_printf(0, "LDO turned on\n");
    delay_ms(100); // make sure LDO has time to start up and sensor has time to
                   // power up
    sensor_init();
    delay_ms(100);
    // Set some sensible default gain and integration time values.
    sensor_write_reg(REG_ALS_MEAS_RATE, 0b0111011); // 350 ms integration, 500ms interval
    sensor_turn_on(GAIN_1X);
    delay_ms(10);

    for (;;) {
        int32_t gain, itime;
        sensor_wait_till_ready();
        sensor_reading sr = sensor_get_reading_auto(&gain, &itime);
        int32_t lux = sensor_reading_to_lux(sr, gain, itime);
        int32_t ev = lux_to_ev(lux);
        SEGGER_RTT_printf(0, "READING g=%u itime=%u c0=%u c1=%u lux=%u/%u (%u) ev=%s%u/%u (%u)\n", gain, itime, sr.chan0, sr.chan1, lux, 1<<EV_BPS, lux>>EV_BPS, sign_of(ev), iabs(ev), 1<<EV_BPS, ev>>EV_BPS);
        int ss_index;
        ev_to_shutter_iso100_f8(ev, &ss_index, 0);
        leds_all_off();
        led_on(6 + ss_index);
    }*/


    // ********** CAPSENSE TEST **********

    /*setup_capsense();

    for (unsigned i = 0;; i++) {
        if (i % (4*6) == 0) {
            touch_on = false;
            SEGGER_RTT_printf(0, "Count %u %u %u %u\n", touch_counts[0], touch_counts[1], touch_counts[2], touch_counts[3]);
            touch_on = true;
            clear_capcounts();
        }

        cycle_capsense();

        delay_ms(10);
    }*/

    // ********** LED TEST **********

    /*for (unsigned i = 1;; ++i) {
        leds_all_off();
        SEGGER_RTT_printf(0, "Led %u\n", i);
        led_on(i);

        delay_ms(300);
    }*/

    return 0;
}

int main()
{
    return testmain();

    return 0;
}