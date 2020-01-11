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
#include <em_wdog.h>
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

void handle_MODE_JUST_WOKEN()
{
    // If it was a brief tap on the button, go to AWAKE_AT_REST.
    // If they've held the button down for a little bit,
    // start doing a reading. If it was a double tap, go to
    // ISO / exposure set mode.
    press p = get_center_pad_press();

    if (p == PRESS_HOLD) {
        SEGGER_RTT_printf(0, "Hold\n");
        g_state.mode = MODE_DOING_READING;
    } else if (p == PRESS_TAP) {
        SEGGER_RTT_printf(0, "Tap\n");
        // It was just a tap.
        g_state.last_reading_flags |= LAST_READING_FLAGS_FRESH;
        g_state.mode = MODE_AWAKE_AT_REST;
    } else {
        SEGGER_RTT_printf(0, "Warning: unknown press\n");
    }
}

void handle_MODE_AWAKE_AT_REST()
{
    // Display the current reading, if any.
    if (fresh_reading_is_saved()) {
        SEGGER_RTT_printf(0, "Fresh reading saved\n");
        g_state.mode = MODE_DISPLAY_READING;
        g_state.last_reading_flags &= ~(int32_t)LAST_READING_FLAGS_FRESH;
    } else {
        g_state.mode = MODE_SNOOZE;
    }
}


void handle_MODE_SNOOZE()
{
    // Make sure LDO is off
    GPIO_PinModeSet(REGMODE_PORT, REGMODE_PIN, gpioModePushPull, 0);

    setup_le_capsense(LE_CAPSENSE_SLEEP);

    SEGGER_RTT_printf(0, "Entering EM2 for snooze\n");
    EMU_EnterEM2(true); // true = restore oscillators, clocks and voltage scaling

    disable_le_capsense();

    SEGGER_RTT_printf(0, "Woken up!\n");
    g_state.mode = MODE_JUST_WOKEN;
}

static void shift_exposure_wheel(int n, int *ap_index, int *ss_index)
{
    int ap = *ap_index;
    int ss = *ss_index;
    while (n != 0) {
        if (n > 0) {
            if (ap == AP_INDEX_MAX)
                break;
            if (ss == SS_INDEX_MIN)
                break;
            ++ap;
            --ss;
            --n;
        } else {
            if (ss == SS_INDEX_MAX)
                break;
            if (ap == AP_INDEX_MIN)
                break;
            ++ss;
            --ap;
            ++n;
        }
        *ap_index = ap;
        *ss_index = ss;
    }
}

void handle_MODE_DISPLAY_READING()
{
    int ap_index, ss_index, third;
    ev_iso_aperture_to_shutter(g_state.last_reading_ev, g_state.iso, F8_AP_INDEX, &ap_index, &ss_index, &third);
    SEGGER_RTT_printf(0, "ISO=%s, ap=%s, ss=%s\n", iso_strings[g_state.iso], ap_index == -1 ? "OOR" : ap_strings[ap_index], ss_index == -1 ? "OOR" : ss_strings[ss_index]);

    leds_all_off();

    if (ap_index == -1)
        leds_on(LED_OUT_OF_RANGE_MASK);
    else
        leds_on_for_reading(ap_index, ss_index, third);

    setup_capsense();

    uint32_t base_cycles = leds_on_for_cycles;
    int zero_touch_position = INVALID_TOUCH_POSITION;
    uint32_t touch_counts[] = { 0, 0, 0 };
    bool in_center_button_dead_zone = true;
    for (unsigned i = 0;; ++i) {
        uint32_t count, chan;
        get_touch_count(&count, &chan);
        touch_counts[chan] = count;

        if (leds_on_for_cycles - base_cycles > (CENTER_BUTTON_DEAD_ZONE_MS * RTC_RAW_FREQ) / 1000)
            in_center_button_dead_zone = false;

        if (i != 0 && i % 3 == 0) {
            int tp = get_touch_position(touch_counts[0], touch_counts[1], touch_counts[2]);
            
            if (tp == NO_TOUCH_DETECTED) {
                zero_touch_position = INVALID_TOUCH_POSITION;
            } else {
                base_cycles = leds_on_for_cycles;

                if (zero_touch_position == INVALID_TOUCH_POSITION) {
                    if (tp == LEFT_BUTTON || tp == RIGHT_BUTTON || tp == LEFT_AND_RIGHT_BUTTONS) {
                        // If we just have one of the left/right buttons, wait to see if the situation
                        // changes in the next split second.
                        if (tp != LEFT_AND_RIGHT_BUTTONS) {
                            uint32_t over = leds_on_for_cycles + (DOUBLE_BUTTON_SLOP_MS * RTC_RAW_FREQ / 1000);
                            get_touch_count(&count, 0);
                            int misses = 0;
                            for (unsigned j = 0;; ++j) {
                                for (uint32_t base = leds_on_for_cycles; leds_on_for_cycles < base + RAW_RTC_CYCLES_PER_PAD_TOUCH_COUNT;)
                                    __NOP(), __NOP(), __NOP(), __NOP();

                                get_touch_count(&count, &chan);
                                touch_counts[chan] = count;

                                if (j != 0 && j % 3 == 0) {
                                    int tp2 = get_touch_position(touch_counts[0], touch_counts[1], touch_counts[2]);
                                    if (tp2 == LEFT_AND_RIGHT_BUTTONS) {
                                        tp = tp2;
                                        break;
                                    }
                                    if (tp2 == NO_TOUCH_DETECTED) {
                                        ++misses;
                                        if (misses >= MISSES_REQUIRED_TO_BREAK_HOLD)
                                            break;
                                    } else {
                                        misses = 0;
                                    }
                                }

                                if (leds_on_for_cycles >= over)
                                    break;

                                cycle_capsense();
                                get_touch_count(&count, 0);
                            }
                        }

                        if (tp == LEFT_AND_RIGHT_BUTTONS) {
                            SEGGER_RTT_printf(0, "Both left and right buttons pressed.\n");
                            goto handle_double_button_press;
                        }
                        else {
                            leds_all_off();
                            shift_exposure_wheel(tp == RIGHT_BUTTON ? 1 : -1, &ap_index, &ss_index);
                            leds_on_for_reading(ap_index, ss_index, third);
                        }
                    } else if (tp == CENTER_BUTTON && ! in_center_button_dead_zone) {
                        goto handle_center_press;
                    }
                    zero_touch_position = tp;
                } else {
                    zero_touch_position = tp;
                }
            }            
        }

        cycle_capsense();

        for (uint32_t base = leds_on_for_cycles; leds_on_for_cycles < base + RAW_RTC_CYCLES_PER_PAD_TOUCH_COUNT;)
            __NOP(), __NOP(), __NOP(), __NOP();

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

    return;

handle_center_press:
    leds_all_off();
    disable_capsense();
    press p = get_center_pad_press();
    if (p == PRESS_HOLD)
        g_state.mode = MODE_DOING_READING;
    else if (p == PRESS_TAP)
        g_state.mode = MODE_SNOOZE;
    return;

handle_double_button_press:
    leds_all_off();
    g_state.mode = MODE_SETTING_ISO;
}

static int iso_to_led_n(int iso)
{
    int clockwise_led_n = (LED_ISO6_N + iso) % LED_N_IN_WHEEL;
    return (LED_N_IN_WHEEL - clockwise_led_n) % LED_N_IN_WHEEL;
}

void handle_MODE_SETTING_ISO()
{
    leds_all_off();

    uint32_t leds = 1 << iso_to_led_n(g_state.iso);
    set_led_throb_mask(leds);
    leds_on(leds);

    setup_capsense();

    uint32_t base_cycles = leds_on_for_cycles;
    int zero_touch_position = INVALID_TOUCH_POSITION;
    uint32_t touch_counts[] = { 0, 0, 0 };
    for (unsigned i = 0;; ++i) {
        uint32_t count, chan;
        get_touch_count(&count, &chan);
        touch_counts[chan] = count;

        if (i != 0 && i % 3 == 0) {
            int tp = get_touch_position(touch_counts[0], touch_counts[1], touch_counts[2]);
            
            if (tp == NO_TOUCH_DETECTED) {
                zero_touch_position = INVALID_TOUCH_POSITION;
            } else {
                base_cycles = leds_on_for_cycles;

                if (zero_touch_position == INVALID_TOUCH_POSITION) {
                    if (tp == LEFT_BUTTON || tp == RIGHT_BUTTON) {
                        leds_all_off();
                        
                        g_state.iso += (tp == RIGHT_BUTTON ? 1 : -1);
                        if (g_state.iso < 0)
                            g_state.iso = ISO_MAX + g_state.iso + 1;
                        if (g_state.iso > ISO_MAX)
                            g_state.iso = 0;

                        SEGGER_RTT_printf(0, "ISO set to %s\n", iso_strings[g_state.iso]);
                        
                        leds = 1 << iso_to_led_n(g_state.iso);
                        set_led_throb_mask(leds);
                        leds_on(leds);
                    } else if (tp == CENTER_BUTTON) {
                        goto handle_center_press;
                    }
                    zero_touch_position = tp;
                } else {
                    zero_touch_position = tp;
                }
            }            
        }

        cycle_capsense();

        for (uint32_t base = leds_on_for_cycles; leds_on_for_cycles < base + RAW_RTC_CYCLES_PER_PAD_TOUCH_COUNT;)
            ;

        if (leds_on_for_cycles >= base_cycles + DISPLAY_READING_TIME_SECONDS * RTC_RAW_FREQ) {
            SEGGER_RTT_printf(0, "Reading display timeout (in ISO set mode)\n");
            break;
        }
    }

    leds_all_off();
    SEGGER_RTT_printf(0, "Disabling capsense\n");
    disable_capsense();

    SEGGER_RTT_printf(0, "Going into MODE_SNOOZE\n");
    g_state.mode = MODE_SNOOZE;

    return;

handle_center_press:
    SEGGER_RTT_printf(0, "ISO button press\n");
    leds_all_off();
    disable_capsense();
    press p = get_center_pad_press();
    if (p == PRESS_HOLD)
        g_state.mode = MODE_DOING_READING;
    else if (p == PRESS_TAP)
        g_state.mode = MODE_SNOOZE;
}

static uint32_t display_reading_interrupt_cycle_mask1;
static uint32_t display_reading_interrupt_cycle_mask2;
static uint32_t display_reading_interrupt_cycle_mask3;
static void display_reading_interrupt_cycle_interrupt_handler()
{
    if (leds_on_for_cycles % 16 != 0)
        return;

    if (leds_on_for_cycles % 64 != 0) {
        display_reading_interrupt_cycle_mask1 = (display_reading_interrupt_cycle_mask1 + 1) % LED_N_IN_WHEEL;
    }

    if (leds_on_for_cycles % 32 != 0) {
        display_reading_interrupt_cycle_mask2 = (display_reading_interrupt_cycle_mask2 + 1) % LED_N_IN_WHEEL;
    }

    display_reading_interrupt_cycle_mask3 = (display_reading_interrupt_cycle_mask3 + 1) % LED_N_IN_WHEEL;

    leds_change_mask((1 << display_reading_interrupt_cycle_mask1) | (1 << (LED_N_IN_WHEEL - display_reading_interrupt_cycle_mask2 - 1)) | (1 << display_reading_interrupt_cycle_mask3));
}

void handle_MODE_DOING_READING()
{
    // Light show while the reading is being done.
    leds_all_off();
    add_rtc_interrupt_handler(display_reading_interrupt_cycle_interrupt_handler);
    display_reading_interrupt_cycle_mask1 = 0;
    display_reading_interrupt_cycle_mask2 = 8;
    display_reading_interrupt_cycle_mask3 = 16;
    leds_on((1 << display_reading_interrupt_cycle_mask1) | (1 << display_reading_interrupt_cycle_mask2) | (1 << display_reading_interrupt_cycle_mask3));

    // Turn on the LDO to power up the sensor.
    SEGGER_RTT_printf(0, "Turning on LDO.\n");
    GPIO_PinModeSet(REGMODE_PORT, REGMODE_PIN, gpioModePushPull, 1);
    SEGGER_RTT_printf(0, "LDO turned on\n");
    delay_ms_with_led_rtc(10); // make sure LDO has time to start up (datasheet says 1ms startup time is typical, so 10 is more than enough)
    sensor_init();
    delay_ms_with_led_rtc(100); // sensor requires 100ms initial startup time.

    // Turn the sensor on and give it time to wake up from standby.
    sensor_turn_on(GAIN_1X);
    delay_ms_with_led_rtc(10);

    // Get the raw sensor reading.
    int32_t gain, itime;
    SEGGER_RTT_printf(0, "Waiting for sensor\n");
    sensor_wait_till_ready(delay_ms_with_led_rtc);
    SEGGER_RTT_printf(0, "Sensor ready\n");
    sensor_reading sr = sensor_get_reading_auto(delay_ms_with_led_rtc, &gain, &itime);
    g_state.last_reading = sr;
    g_state.last_reading_itime = itime;
    g_state.last_reading_gain = gain;

    // Convert the raw reading to an EV value.
    int32_t lux = sensor_reading_to_lux(sr, gain, itime);
    int32_t ev = lux_to_ev(lux);
    g_state.last_reading_ev = ev;
    SEGGER_RTT_printf(0, "READING g=%u itime=%u c0=%u c1=%u lux=%u/%u (%u) ev=%s%u/%u (%u)\n", gain, itime, sr.chan0, sr.chan1, lux, 1<<EV_BPS, lux>>EV_BPS, sign_of(ev), iabs(ev), 1<<EV_BPS, ev>>EV_BPS);

    g_state.led_brightness_ev_ref = ev >> EV_BPS;

    // Turn the LDO off to power down the sensor.
    GPIO_PinModeSet(REGMODE_PORT, REGMODE_PIN, gpioModePushPull, 0);

    leds_all_off();
    clear_rtc_interrupt_handlers();

    // If they're still holding down the button, display the reading
    // indefinitely until the button is released, then go into the
    // regular display reading mode.
    setup_capsense();
    cycle_capsense();
    cycle_capsense(); // get to center button
    uint32_t chan;
    get_touch_count(&chan, 0);
    delay_ms(PAD_COUNT_MS);
    get_touch_count(&chan, 0);
    if (center_pad_is_touched(chan)) {
        int ap_index, ss_index, third;
        ev_iso_aperture_to_shutter(g_state.last_reading_ev, g_state.iso, F8_AP_INDEX, &ap_index, &ss_index, &third);
        SEGGER_RTT_printf(0, "Third: %s%u\n", sign_of(third), iabs(third));
        if (ap_index == -1)
            leds_on(LED_OUT_OF_RANGE_MASK);
        else
            leds_on_for_reading(ap_index, ss_index, third);

        get_touch_count(&chan, 0);
        int misses = 0;
        for (;;) {
            for (uint32_t base = leds_on_for_cycles; leds_on_for_cycles < base + RAW_RTC_CYCLES_PER_PAD_TOUCH_COUNT;)
                ;
            get_touch_count(&chan, 0);

            if (! center_pad_is_touched(chan)) {
                ++misses;
                if (misses >= MISSES_REQUIRED_TO_BREAK_HOLD)
                    break;
            } else {
                misses = 0;
            }
        }
    }

    leds_all_off();
    disable_capsense();

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
            case MODE_SETTING_ISO: {
                SEGGER_RTT_printf(0, "MODE_SETTING_ISO\n");

                handle_MODE_SETTING_ISO();
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
    // Setting pins to input with a pulldown as the default should minimize power consumption.
    GPIO_PinModeSet(BATSENSE_PORT, BATSENSE_PIN, gpioModeInputPull, 0);
    GPIO_PinModeSet(gpioPortF, 1, gpioModeInputPull, 0);
    GPIO_PinModeSet(gpioPortF, 2, gpioModeInputPull, 0);
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

void TIMER1_IRQHandler(void)
{ 
  TIMER_IntClear(TIMER1, TIMER_IF_OF);
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

int test_debug_led_throb_main()
{
    leds_all_off();
    int l1 = 0;
    int l2 = 4;
    int l3 = 7;

    for (;;) {
        set_led_throb_mask(1 << l1);
        leds_on((1 << l1) | (1 << l2) | (1 << l3));
        uint32_t offat = leds_on_for_cycles + RTC_RAW_FREQ;
        while (leds_on_for_cycles < offat)
            ;
        leds_all_off();
        l1 = (l1 + 1) % 24;
        l2 = (l2 + 1) % 24;
        l3 = (l3 + 1) % 24;
    }
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

int test_show_reading()
{
    leds_all_off();

    leds_on_for_reading(F22_AP_INDEX, SS500_INDEX, 0);

    for (;;);

    return 0;
}

int test_capsense_main()
{
    SEGGER_RTT_printf(0, "Capsense test...\n");

    setup_capsense();

    uint32_t touch_counts[] = { 0, 0, 0 };
    for (unsigned i = 0;; ++i) {
        uint32_t count, chan;
        get_touch_count(&count, &chan);
        touch_counts[chan] = count;

        touch_position tp = get_touch_position(touch_counts[0], touch_counts[1], touch_counts[2]);
        const char *tps = "UNKNOWN";
        switch (tp) {
            case INVALID_TOUCH_POSITION:
                tps = "INVALID"; break;
            case NO_TOUCH_DETECTED:
                tps = "NOTOUCH"; break;
            case LEFT_BUTTON:
                tps = "LEFT"; break;
            case RIGHT_BUTTON:
                tps = "RIGHT"; break;
            case LEFT_AND_RIGHT_BUTTONS:
                tps = "LEFT+RIGHT"; break;
            case CENTER_BUTTON:
                tps = "CENTER"; break;
        }

        if (i % (4*6) == 0)
            SEGGER_RTT_printf(0, "count %u %u %u pos = %s\n", touch_counts[1], touch_counts[0], touch_counts[2], tps);
        
        cycle_capsense();

        delay_ms(PAD_COUNT_MS);
    }
}

int test_capsense_with_wheel_main()
{
    int led_index = 0;

    leds_all_off();
    leds_on(1 << led_index);

    setup_capsense();

    int zero_touch_position = INVALID_TOUCH_POSITION;
    uint32_t touch_counts[] = { 0, 0, 0 };
    for (unsigned i = 0;; ++i) {
        if (i != 0 && i % 2 == 0) {
            uint32_t count, chan;
            get_touch_count(&count, &chan);
            touch_counts[chan] = count;
            int tp = get_touch_position(touch_counts[0], touch_counts[1], touch_counts[2]);
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
        }

        cycle_capsense();

        for (uint32_t base = leds_on_for_cycles; leds_on_for_cycles < base + RAW_RTC_CYCLES_PER_PAD_TOUCH_COUNT;)
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
        sensor_wait_till_ready(delay_ms);
        sensor_reading sr = sensor_get_reading_auto(delay_ms, &gain, &itime);
        int32_t lux = sensor_reading_to_lux(sr, gain, itime);
        int32_t ev = lux_to_ev(lux);
        int32_t evthird = ev & ((1<<EV_BPS)-1);
        int32_t thirdval = 0;
        if (evthird > (2<<EV_BPS)/3) {
            thirdval = 2;
        } else if (evthird > (1 << EV_BPS)/3) {
            thirdval = 1;
        }
        SEGGER_RTT_printf(0, "READING g=%u itime=%u c0=%u c1=%u lux=%u/%u (%u) ev=%s%u/%u (%u+%u/3)\n", gain, itime, sr.chan0, sr.chan1, lux, 1<<EV_BPS, lux>>EV_BPS, sign_of(ev), iabs(ev), 1<<EV_BPS, ev>>EV_BPS, thirdval);
        //leds_all_off();
        //led_on(LED_1S_N + ss_index);
        //if (third == -1)
        //    led_on(LED_MINUS_1_3_N);
        //else
        //    led_on(LED_PLUS_1_3_N);
    }
}

int test_tempsensor_main()
{
    // Turn on the LDO to power up the sensor.
    GPIO_PinModeSet(REGMODE_PORT, REGMODE_PIN, gpioModePushPull, 1);
    SEGGER_RTT_printf(0, "LDO turned on\n");
    delay_ms(100); // make sure LDO has time to start up and sensor has time to
                   // power up
    sensor_init();
    delay_ms(100);

    for (;;) {
        tempsensor_get_reading(delay_ms);
        delay_ms(500);
    }

    return 0;
}

int test_watchdog_wakeup_main()
{
    SEGGER_RTT_printf(0, "In test_watchdog_wakeup_main...\n");

    delay_ms(1000);

    leds_all_off();
    leds_on(1);

    CMU_ClockEnable(cmuClock_CORELE, true);

    for (;;) {
        EMU_EM23Init_TypeDef dcdcInit = EMU_EM23INIT_DEFAULT;
        EMU_EM23Init(&dcdcInit);

        WDOG_Init_TypeDef wInit = WDOG_INIT_DEFAULT;
        wInit.debugRun = true; // Run in debug
        wInit.clkSel = wdogClkSelULFRCO;
        wInit.em2Run = true;
        wInit.em3Run = true;
        wInit.perSel = wdogPeriod_4k; // 4k 1kHz periods should give ~4 seconds in EM3
        wInit.enable = true;

        WDOGn_Init(WDOG, &wInit);
        WDOGn_Feed(WDOG);

        SEGGER_RTT_printf(0, "Sleepy sleepy\n");
        EMU_EnterEM3(true); // true = restore oscillators, clocks and voltage scaling

        leds_all_off();
        leds_on(1);

        delay_ms(1000);
        
        leds_all_off();
    }

    return 0;
}

int test_le_capsense_main()
{
    for (;;) {
        SEGGER_RTT_printf(0, "LOOP\n");
        setup_le_capsense(LE_CAPSENSE_SLEEP);
        EMU_EnterEM2(true);
        disable_le_capsense();
        press p = get_center_pad_press();
        switch (p) {
            case PRESS_TAP:
                SEGGER_RTT_printf(0, "TAP!\n");
                break;
            case PRESS_HOLD:
                SEGGER_RTT_printf(0, "HOLD!\n");
                break;
            default:
                SEGGER_RTT_printf(0, "Unknown press type\n");
        }
    }
}

int real_main()
{
    set_state_to_default();

    state_loop();

    return 0;
}

int main()
{
    common_init();

    return real_main();
    //return test_debug_led_throb_main();
    //return test_led_interrupt_cycle();
    //return test_show_reading();
    //return test_sensor_main();
    //return test_tempsensor_main();
    //return test_capsense_with_wheel_main();
    //return test_main();
    //return test_batsense_main();
    //return test_capsense_main();
    //return test_le_capsense_main();
    //return test_watchdog_wakeup_main();
    //return test_led_change_main();
    //return reset_state_main();
}
