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
#include <rtc.h>
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

    //int v = GPIO_PinInGet(BUTTON_GPIO_PORT, BUTTON_GPIO_PIN);

    SEGGER_RTT_printf(0, "Hi!\n");
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

void wake_timer_rtc_count_callback()
{
    RTC_Enable(false);

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

    set_rtc_interrupt_handler(wake_timer_rtc_count_callback);

    RTC_Init_TypeDef init = {
        true, // Start counting when initialization is done
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
    // If it was a brief tap on the button, go to AWAKE_AT_REST.
    // Otherwise, if they've held the button down for a little bit,
    // start doing a reading.
    RTC->CNT = 0;
    RTC->CTRL |= RTC_CTRL_EN;
    int v = 1;
    while (((v = GPIO_PinInGet(BUTTON_GPIO_PORT, BUTTON_GPIO_PIN)) == 0) && RTC->CNT < (RTC_FREQ * LONG_BUTTON_PRESS_MS)/1000)
        ;
    RTC->CTRL &= ~RTC_CTRL_EN;
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
    // Set up button press interrupt for when we're in EM2.
    setup_button_press_interrupt();

#ifndef DEBUG // TODO changed to proper #if defined conditional (I forget the syntax...)
#ifndef NO_EM4
    // If we've been in EM2 for a while and nothing has happened,
    // we want to go into EM4.
    TEMPORARY_CANARY
    turn_on_wake_timer();
#endif
#endif

    // Display the current reading, if any.
    if (fresh_reading_is_saved()) {
        SEGGER_RTT_printf(0, "Fresh reading saved\n");
        g_state.mode = MODE_DISPLAY_READING;
        g_state.last_reading_flags &= ~(int32_t)LAST_READING_FLAGS_FRESH;
    } else {
        SEGGER_RTT_printf(0, "No fresh reading saved\n");
        EMU_EnterEM2(true); // true = restore oscillators, clocks and voltage scaling
        SEGGER_RTT_printf(0, "AFTER EM2\n");
        g_state.mode = MODE_JUST_WOKEN;
    }
}


void handle_MODE_SNOOZE()
{
    // Set up button press interrupt for when we're in EM2.
    setup_button_press_interrupt();

    // If we've been in EM2 for a while and nothing has happened,
    // we want to go into EM4.
    turn_on_wake_timer();

    SEGGER_RTT_printf(0, "Entering EM2 for snooze\n");
    EMU_EnterEM3(true); // true = restore oscillators, clocks and voltage scaling
    SEGGER_RTT_printf(0, "Woken up WTF?!\n");
    g_state.mode = MODE_JUST_WOKEN;
}

static void shift_wheel(int n, int *ap_index, int *ss_index)
{
    int ap = *ap_index;
    int ss = *ss_index;
    while (n != 0) {
        if (n > 0) {
            if (ap < AP_INDEX_MAX)
                ++ap;
            if (ss > SS_INDEX_MIN)
                --ss;
            if (ap == AP_INDEX_MAX)
                break;
            if (ss == SS_INDEX_MIN)
                break;
            --n;
        } else {
            if (ss < SS_INDEX_MAX)
                ++ss;
            if (ap > AP_INDEX_MIN)
                --ap;
            if (ss == SS_INDEX_MAX)
                break;
            if (ap == AP_INDEX_MIN)
                break;
            ++n;
        }
        *ap_index = ap;
        *ss_index = ss;
    }
}

static void leds_on_for_reading(int ap_index, int ss_index, int third)
{
    unsigned ss_led_n = LED_1S_N + ss_index;
    unsigned ap_led_n = (LED_F1_N + ap_index) % 24;
    uint32_t mask = (1 << ap_led_n) | (1 << ss_led_n);
    if (third == 1)
        mask |= (1 << LED_PLUS_1_3_N);
    else if (third == -1)
        mask |= (1 << LED_MINUS_1_3_N);
    leds_on(mask);
}

void handle_MODE_DISPLAY_READING()
{
    int ap_index, ss_index, third;
    ev_iso_aperture_to_shutter(g_state.last_reading_ev, g_state.iso, 6/*f8*/, &ap_index, &ss_index, &third);

    leds_all_off();

    if (ap_index == -1) {
        leds_on(0b100000000000000000000011);
    } else {
        leds_on_for_reading(ap_index, ss_index, third);
    }

    clear_capcounts();
    setup_capsense();

    uint32_t base_cycles = leds_on_for_cycles;
    int zero_touch_position = INVALID_TOUCH_POSITION;
    for (unsigned i = 0;; ++i) {
        if (i != 0 && i % 4 == 0) {
            touch_on = false;
            int tp = get_touch_position();
            
            if (tp == NO_TOUCH_DETECTED) {
                zero_touch_position = INVALID_TOUCH_POSITION;
            } else {
                base_cycles = leds_on_for_cycles;

                if (zero_touch_position == INVALID_TOUCH_POSITION) {
                    if (tp != INVALID_TOUCH_POSITION) {
                        SEGGER_RTT_printf(0, "Shifting wheel from %u %u (iso %u)\n", ap_index, ss_index, g_state.iso);
                        leds_all_off();
                        shift_wheel(tp == RIGHT_BUTTON ? 1 : -1, &ap_index, &ss_index);
                        SEGGER_RTT_printf(0, "...to %u %u\n", ap_index, ss_index);
                        leds_on_for_reading(ap_index, ss_index, third);
                    }
                    zero_touch_position = tp;
                } else {
                    zero_touch_position = tp;
                }
            }
            
            touch_on = true;
            clear_capcounts();
        }

        cycle_capsense();

        for (uint32_t base = leds_on_for_cycles; leds_on_for_cycles < base + RTC_CYCLES_PER_PAD_TOUCH_COUNT;)
            ;

        if (leds_on_for_cycles >= base_cycles + DISPLAY_READING_TIME_SECONDS * RTC_RAW_FREQ) {
            SEGGER_RTT_printf(0, "Reading display timeout\n");
            break;
        }
    }

    leds_all_off();
    SEGGER_RTT_printf(0, "Disabling capsense\n");
    disable_capsense();

    SEGGER_RTT_printf(0, "Going into MODE_SNOOZE\n");
    g_state.mode = MODE_SNOOZE;
}

void handle_MODE_DOING_READING()
{
    // Display the ISO.
    leds_all_off();
    led_on((LED_ISO6_N + g_state.iso) % LED_N_IN_WHEEL);

    // Turn on the LDO to power up the sensor.
    SEGGER_RTT_printf(0, "Turning on LDO.\n");
    GPIO_PinModeSet(REGMODE_PORT, REGMODE_PIN, gpioModePushPull, 1);
    SEGGER_RTT_printf(0, "LDO turned on\n");
    delay_ms(10); // make sure LDO has time to start up (datasheet says 1ms startup time is typical, so 10 is more than enough)
    sensor_init();
    delay_ms(100); // sensor requires 100ms initial startup time.

    // Turn the sensor on and give it time to wake up from standby.
    sensor_turn_on(GAIN_1X);
    delay_ms(10);

    // Get the raw sensor reading.
    int32_t gain, itime;
    SEGGER_RTT_printf(0, "Waiting for sensor\n");
    sensor_wait_till_ready();
    SEGGER_RTT_printf(0, "Sensor ready\n");
    sensor_reading sr = sensor_get_reading_auto(&gain, &itime);
    g_state.last_reading = sr;
    g_state.last_reading_itime = itime;
    g_state.last_reading_gain = gain;

    // Convert the raw reading to an EV value.
    int32_t lux = sensor_reading_to_lux(sr, gain, itime);
    int32_t ev = lux_to_ev(lux);
    g_state.last_reading_ev = ev;
    SEGGER_RTT_printf(0, "READING g=%u itime=%u c0=%u c1=%u lux=%u/%u (%u) ev=%s%u/%u (%u)\n", gain, itime, sr.chan0, sr.chan1, lux, 1<<EV_BPS, lux>>EV_BPS, sign_of(ev), iabs(ev), 1<<EV_BPS, ev>>EV_BPS);

    //
    int ss_index;
    ev_to_shutter_iso100_f8(ev, &ss_index, 0);
    leds_all_off();
    led_on(6 + ss_index);

    // Turn the LDO off to power down the sensor.
    GPIO_PinModeSet(REGMODE_PORT, REGMODE_PIN, gpioModePushPull, 0);

    g_state.mode = MODE_DISPLAY_READING;
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
            } break;
            case MODE_SNOOZE: {
                SEGGER_RTT_printf(0, "MODE_SNOOZE\n");

                handle_MODE_SNOOZE();
            } break;
        }
    }
}

void gpio_pins_to_initial_states()
{
    GPIO_PinModeSet(BUTTON_GPIO_PORT, BUTTON_GPIO_PIN, gpioModeInputPullFilter, 1);

    // Setting pins to input with a pulldown as the default should minimize power consumption.
    GPIO_PinModeSet(BATSENSE_PORT, BATSENSE_PIN, gpioModeInputPull, 0);
    GPIO_PinModeSet(gpioPortF, 1, gpioModeInputPull, 0);
    GPIO_PinModeSet(gpioPortC, 15, gpioModeInputPull, 0);
    GPIO_PinModeSet(gpioPortC, 14, gpioModeInputPull, 0);
    GPIO_PinModeSet(gpioPortD, 7, gpioModeInputPull, 0);
    GPIO_PinModeSet(gpioPortD, 6, gpioModeInputPull, 0);
    GPIO_PinModeSet(gpioPortB, 14, gpioModeInputPull, 0);
    GPIO_PinModeSet(gpioPortB, 13, gpioModeInputPull, 0);
    GPIO_PinModeSet(gpioPortB, 11, gpioModeInputPull, 0);
    GPIO_PinModeSet(gpioPortB, 8, gpioModeInputPull, 0);
    GPIO_PinModeSet(gpioPortB, 7, gpioModeInputPull, 0);
    GPIO_PinModeSet(gpioPortC, 0, gpioModeInputPull, 0);
    GPIO_PinModeSet(gpioPortC, 1, gpioModeInputPull, 0);
    GPIO_PinModeSet(gpioPortA, 0, gpioModeInputPull, 0);
    GPIO_PinModeSet(gpioPortE, 13, gpioModeInputPull, 0);
    GPIO_PinModeSet(gpioPortE, 12, gpioModeInputPull, 0);
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

    gpio_pins_to_initial_states();
}

int test_main()
{
    // ********** TEST LED CYCLING **********

    /*leds_all_off();
    leds_on(0b101);

    while (leds_on_for_cycles < DISPLAY_READING_TIME_SECONDS * RTC_RAW_FREQ) {
        __NOP();
        __NOP();
        __NOP();
        __NOP();
    }

    leds_all_off();

    for (;;) ;*/

    // ********** BATSENSE TEST **********

    /*for (;;) {
        int v = get_battery_voltage();
        SEGGER_RTT_printf(0, "V count %u\n", v);
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

int test_led_change_main()
{
    leds_all_off();
    uint32_t v = 1;
    for (;;) {
        SEGGER_RTT_printf(0, "MASK %u\n", v);
        leds_on(v);

        uint32_t base = leds_on_for_cycles;
        while (leds_on_for_cycles < base + RTC_RAW_FREQ / 4)
            ;

        leds_all_off();

        v <<= 1;
        if (v > (1 << 25))
            v = 1;
    }

    for (;;) {
        SEGGER_RTT_printf(0, "First pattern\n");
        leds_on(0b101);
        uint32_t base_cycles = leds_on_for_cycles;
        while (leds_on_for_cycles < base_cycles + RTC_RAW_FREQ)
            ;
        SEGGER_RTT_printf(0, "Second pattern\n");
        leds_all_off();
        CMU_ClockSelectSet(cmuClock_RTC, cmuSelect_LFRCO);
        CMU_ClockDivSet(cmuClock_RTC, RTC_CMU_CLK_DIV);
        CMU_ClockEnable(cmuClock_RTC, true);
        delay_ms(1000);
        //leds_on(0b00011);
        //base_cycles = leds_on_for_cycles;
        //while (leds_on_for_cycles < base_cycles + RTC_RAW_FREQ)
        //    ;
    }
}

int test_capsense_main()
{
    SEGGER_RTT_printf(0, "Capsense test...\n");

    setup_capsense();

    for (unsigned i = 0;; ++i) {
        if (i % 4 == 0) {
            touch_on = false;
            if (i % (4*6) == 0)
                SEGGER_RTT_printf(0, "count %u %u\n", touch_counts[1], touch_counts[0]);
            touch_on = true;
            clear_capcounts();
        }

        cycle_capsense();

        delay_ms(PAD_COUNT_MS);
    }
}

int test_capsense_with_wheel_main()
{
    int led_index = 0;

    leds_all_off();
    leds_on(1 << led_index);

    clear_capcounts();
    setup_capsense();

    int zero_touch_position = INVALID_TOUCH_POSITION;
    for (unsigned i = 0;; ++i) {
        if (i != 0 && i % 2 == 0) {
            touch_on = false;
            int tp = get_touch_position();
            //SEGGER_RTT_printf(0,"Pads %u %u (%s%u)\n", touch_counts[1], touch_counts[0], sign_of(tp), tp);
            
            if (tp == NO_TOUCH_DETECTED) {
                zero_touch_position = INVALID_TOUCH_POSITION;
            } else {
                //SEGGER_RTT_printf(0, "Touch: %s%u\n", sign_of(tp), iabs(tp));

                if (zero_touch_position == INVALID_TOUCH_POSITION) {
                    if (tp != INVALID_TOUCH_POSITION) {
                        leds_all_off();
                        led_index += (tp == LEFT_BUTTON ? -1 : 1);
                        if (led_index < 0)
                            led_index = LED_N + led_index;
                        else if (led_index >= LED_N)
                            led_index %= LED_N;
                        leds_on(1 << led_index);
                    }
                    zero_touch_position = tp;
                } else {
                    zero_touch_position = tp;
                }
            }
            
            
            touch_on = true;
            clear_capcounts();
        }

        cycle_capsense();

        for (uint32_t base = leds_on_for_cycles; leds_on_for_cycles < base + RTC_CYCLES_PER_PAD_TOUCH_COUNT;)
            ;
    }
}

int test_batsense_main()
{
    for (;;) {
        SEGGER_RTT_printf(0, "Low battery: %u\n", low_battery());
        delay_ms(1000);
    }
}

int reset_state_main()
{
    SEGGER_RTT_printf(0, "Erasing state pages...\n");
    erase_state_pages();
    SEGGER_RTT_printf(0, "Erased.\n");
    return 0;
}

int test_sensor_main()
{
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
        SEGGER_RTT_printf(0, "READING g=%u itime=%u c0=%u c1=%u lux=%u/%u (%u) ev=%s%u/%u (%u%s)\n", gain, itime, sr.chan0, sr.chan1, lux, 1<<EV_BPS, lux>>EV_BPS, sign_of(ev), iabs(ev), 1<<EV_BPS, ev>>EV_BPS, (ev % (1<<EV_BPS) >= (2<<EV_BPS/3)) ? "+2/3" : (ev % (1<<EV_BPS) >= (1<<EV_BPS/3) ? "+1/3" : ""));
        int ss_index, third;
        ev_to_shutter_iso100_f8(ev, &ss_index, &third);
        SEGGER_RTT_printf(0, "SSINDEX %s%u\n", sign_of(ss_index), iabs(ss_index));
        //leds_all_off();
        //led_on(LED_1S_N + ss_index);
        //if (third == -1)
        //    led_on(LED_MINUS_1_3_N);
        //else
        //    led_on(LED_PLUS_1_3_N);
    }
}

int real_main()
{
#if defined(DEBUG) || defined (NO_FLASHREAD)
    set_state_to_default();
#else
    TEMPORARY_CANARY
    read_state_from_flash();
#endif

    state_loop();

    return 0;
}

int main()
{
    common_init();

    return real_main();
    //return test_sensor_main();
    //return test_capsense_with_wheel_main();
    //return test_main();
    //return test_batsense_main();
    //return test_capsense_main();
    //return test_led_change_main();
    //return reset_state_main();
}
