#include <config.h>

#include <batsense.h>
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

void wake_timer_rtc_count_callback()
{
    leds_all_off();

    SEGGER_RTT_printf(0, "Entering EM4 (unless in debug mode)\n");

#ifndef DEBUG
    write_state_to_flash();
    sleep_awaiting_button_press();
#endif
}

void turn_on_wake_timer()
{
    RTC_Enable(false);

    RTC_IRQHandler_first_time = true;
    rtc_count_callback = wake_timer_rtc_count_callback;

    RTC_Init_TypeDef init = {
        __GCC_ATOMIC_TEST_AND_SET_TRUEVAL, // Start counting when initialization is done
        false, // Enable updating during debug halt.
        true  // Restart counting from 0 when reaching COMP0.
    };

    RTC_CompareSet(0, IDLE_TIME_BEFORE_DEEPEST_SLEEP_SECONDS * RTC_FREQ);

    // Enabling Interrupt from RTC
    RTC_IntEnable(RTC_IEN_COMP0);
    NVIC_EnableIRQ(RTC_IRQn);
    RTC_Init(&init);
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
        g_state.last_reading_flags |= LAST_READING_FLAGS_FRESH;
        g_state.mode = MODE_AWAKE_AT_REST;
    }
}

void handle_MODE_AWAKE_AT_REST()
{
    // Await further button presses in EM2.
    setup_button_press_interrupt();

    // If we've been in EM2 for a while and nothing has happened,
    // we want to go into EM4.
    turn_on_wake_timer();

    // Display the current reading, if any.
    if (fresh_reading_is_saved()) {
        g_state.mode = MODE_DISPLAY_READING;
        g_state.last_reading_flags &= ~(int32_t)LAST_READING_FLAGS_FRESH;
    } else {
        EMU_EnterEM2(true); // true = restore oscillators, clocks and voltage scaling
        g_state.mode = MODE_JUST_WOKEN;
    }
}

void handle_MODE_DISPLAY_READING()
{
    int ss_index;
    ev_to_shutter_iso100_f8(g_state.last_reading_ev, &ss_index, 0);
    leds_all_off();
    led_on(6 + ss_index);

    // The current wasted by looping is trivial compared to the current used
    // by the LEDs, so might as well do this the simple way.
    // TODO: we also need to incorporate capsense + slider into this wait loop.
    delay_ms(DISPLAY_READING_TIME_SECONDS * 1000);

    leds_all_off();

    g_state.mode = MODE_AWAKE_AT_REST;
}

void handle_MODE_DOING_READING()
{
    // Turn on the LDO to power up the sensor.
    SEGGER_RTT_printf(0, "Turning on LDO.\n");
    GPIO_PinModeSet(REGMODE_PORT, REGMODE_PIN, gpioModePushPull, 1);
    SEGGER_RTT_printf(0, "LDO turned on\n");
    delay_ms(100); // make sure LDO has time to start up and sensor has time to
                   // power up
    sensor_init();
    delay_ms(100);

    // Set some sensible default gain and integration time values.
    sensor_write_reg(REG_ALS_MEAS_RATE, 0b0111011); // 350 ms integration, 500ms interval
    sensor_turn_on(GAIN_1X);
    delay_ms(10);

    int32_t gain, itime;
    SEGGER_RTT_printf(0, "Waiting for sensor\n");
    sensor_wait_till_ready();
    SEGGER_RTT_printf(0, "Sensor ready\n");
    sensor_reading sr = sensor_get_reading_auto(&gain, &itime);
    g_state.last_reading = sr;
    g_state.last_reading_itime = itime;
    g_state.last_reading_gain = gain;
    int32_t lux = sensor_reading_to_lux(sr, gain, itime);
    int32_t ev = lux_to_ev(lux);
    g_state.last_reading_ev = ev;
    SEGGER_RTT_printf(0, "READING g=%u itime=%u c0=%u c1=%u lux=%u/%u (%u) ev=%s%u/%u (%u)\n", gain, itime, sr.chan0, sr.chan1, lux, 1<<EV_BPS, lux>>EV_BPS, sign_of(ev), iabs(ev), 1<<EV_BPS, ev>>EV_BPS);
    int ss_index;
    ev_to_shutter_iso100_f8(ev, &ss_index, 0);
    leds_all_off();
    led_on(6 + ss_index);

    // Turn the LDO off to power down the sensor.
    GPIO_PinModeSet(REGMODE_PORT, REGMODE_PIN, gpioModePushPull, 0);

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
            case MODE_DISPLAY_READING: {
                SEGGER_RTT_printf(0, "MODE_DISPLAY_READING\n");

                handle_MODE_DISPLAY_READING();
            }
        }
    }
}

void common_init()
{
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

    rtt_init();
    SEGGER_RTT_printf(0, "\n\nHello RTT console; core clock freq = %u.\n", CMU_ClockFreqGet(cmuClock_CORE));

    GPIO_PinModeSet(BUTTON_GPIO_PORT, BUTTON_GPIO_PIN, gpioModeInputPullFilter, 1);
    GPIO_PinModeSet(BATSENSE_PORT, BATSENSE_PIN, gpioModeInput, 0);
}

int testmain()
{
    // ********** BATSENSE TEST **********

    /*for (;;) {
        int v = get_battery_voltage();
        SEGGER_RTT_printf(0, "V count %u\n", v);
    }*/


    // ********** SENSOR TEST **********

    // Turn on the LDO to power up the sensor.
    GPIO_PinModeSet(REGMODE_PORT, REGMODE_PIN, gpioModePushPull, 1);
    SEGGER_RTT_printf(0, "LDO turned on\n");
    delay_ms(100); // make sure LDO has time to start up and sensor has time to
                   // power up
    sensor_init();
    delay_ms(100);

    // Turn the sensor on an give it time to get ready. (We have to set a gain
    // value when we turn the sensor on, but the choice here is immaterial.)
    sensor_turn_on(GAIN_1X);
    delay_ms(10);

    for (;;) {
        int32_t gain, itime;
        sensor_wait_till_ready();
        sensor_reading sr = sensor_get_reading_auto(&gain, &itime);
        int32_t lux = sensor_reading_to_lux(sr, gain, itime);
        int32_t ev = lux_to_ev(lux);
        SEGGER_RTT_printf(0, "READING g=%u itime=%u c0=%u c1=%u lux=%u/%u (%u) ev=%s%u/%u (%u)\n", gain, itime, sr.chan0, sr.chan1, lux, 1<<EV_BPS, lux>>EV_BPS, sign_of(ev), iabs(ev), 1<<EV_BPS, ev>>EV_BPS);
        int ss_index, third;
        ev_to_shutter_iso100_f8(ev, &ss_index, &third);
        SEGGER_RTT_printf(0, "SSINDEX %s%u\n", sign_of(ss_index), iabs(ss_index));
        leds_all_off();
        led_on(LED_1S_N + ss_index);
        //if (third == -1)
        //    led_on(LED_MINUS_1_3_N);
        //else
        //    led_on(LED_PLUS_1_3_N);
    }


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

int real_main()
{
    read_state_from_flash();

    state_loop();

    return 0;
}

int main()
{
    common_init();

    return real_main();
//    return testmain();
}