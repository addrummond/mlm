#include <config.h>

#include <batsense.h>
#include <capsense.h>
#include <em_acmp.h>
#include <em_chip.h>
#include <em_cmu.h>
#include <em_dbg.h>
#include <em_emu.h>
#include <em_device.h>
#include <em_emu.h>
#include <em_gpio.h>
#include <em_pcnt.h>
#include <em_prs.h>
#include <em_rmu.h>
#include <em_rtc.h>
#include <em_timer.h>
#include <em_wdog.h>
#include <init.h>
#include <iso.h>
#include <leds.h>
#include <myemu.h>
#include <pins.h>
#include <rtc.h>
#include <rtt.h>
#include <sensor.h>
#include <state.h>
#include <stdbool.h>
#include <stdint.h>
#include <tempsensor.h>
#include <time.h>
#include <units.h>
#include <util.h>

#ifdef TEST_MAIN
int test_main(void);
#endif

static void go_into_deep_sleep()
{
    SEGGER_RTT_printf(0, "Enter deep sleep (unless in DEBUG mode).\n");

    CMU_ClockEnable(cmuClock_CORELE, true);

    EMU_EM23Init_TypeDef dcdcInit = EMU_EM23INIT_DEFAULT;
    EMU_EM23Init(&dcdcInit);

    RTC_Enable(false);

    WDOG_Init_TypeDef wInit = {
        .clkSel = wdogClkSelULFRCO,
        .debugRun = true,
        .em2Run = true,
        .em3Run = true,
        .em4Block = true,
        .enable = true,
        .lock = false,
        .perSel = wdogPeriod_1k, // 1k 1kHz periods should give ~4 seconds in EM3
        .swoscBlock = false
    };

    WDOGn_Init(WDOG, &wInit);
    WDOGn_Feed(WDOG);

    RMU->CTRL &= ~0b111;
    RMU->CTRL |= 1; // limited watchdog reset

    my_emu_enter_em3(false);
}

static void go_into_deep_sleep_with_indication()
{
    // A visual indication that it's going into deep sleep mode is sometimes
    // useful for debugging.
#ifdef DEBUG_DEEP_SLEEP
    leds_all_off();
    leds_on(0b111);
    uint32_t start = leds_on_for_cycles;
    while (leds_on_for_cycles - start < RTC_RAW_FREQ)
        ;
    leds_all_off();
#endif

    go_into_deep_sleep();
}

static void maybe_sleep_deeper()
{
    static const int32_t DEEP_SLEEP_TIMING_FUDGE_FACTOR_NUMERATOR = 6;
    static const int32_t DEEP_SLEEP_TIMING_FUDGE_FACTOR_DENOMINATOR = 5;

    static const int32_t deep_sleep_timeout_seconds =
#ifdef DEBUG_DEEP_SLEEP
        DEEP_SLEEP_TIMEOUT_SECONDS_DEBUG_MODE;
#else
        DEEP_SLEEP_TIMEOUT_SECONDS;
#endif

#if !defined(DEBUG)
#   ifdef DISABLE_DEEP_SLEEP
    if ((g_state.deep_sleep_counter++) * DEEP_SLEEP_TIMING_FUDGE_FACTOR_NUMERATOR / DEEP_SLEEP_TIMING_FUDGE_FACTOR_DENOMINATOR > deep_sleep_timeout_seconds/LE_CAPSENSE_CALIBRATION_INTERVAL_SECONDS) {
        // We've been sleeping for a while now and nothing has happened.
        // Time to go into deep sleep.
        go_into_deep_sleep_with_indication();
    }
#   else
    le_capsense_slow_scan();
#   endif
#endif
}

static void handle_MODE_JUST_WOKEN()
{
    if (! g_state.watchdog_wakeup) {
        while (! check_lesense_irq_handler()) {
            maybe_sleep_deeper();

            recalibrate_le_capsense();
            SEGGER_RTT_printf(0, "EM2 snooze after calib.\n");
            my_emu_enter_em2(true); // true = restore oscillators, clocks and voltage scaling
            SEGGER_RTT_printf(0, "Woken up [2]!\n");
        }

        disable_le_capsense();

        int v = battery_voltage_in_10th_volts();
        g_state.bat_known_healthy = (v >= 27);
        SEGGER_RTT_printf(0, "BKH %u %u\n", v, g_state.bat_known_healthy);
#ifdef DEBUG
        g_state.bat_known_healthy = false;
#endif

    } else {
        PCNT_Reset(PCNT0);
        reset_lesense_irq_handler_state();
        reset_led_state();
        reset_capsense_state();
        SEGGER_RTT_printf(0, "Watchdog wake\n");
    }

    g_state.watchdog_wakeup = false;
    g_state.deep_sleep_counter = 0;

    // If it was a brief tap on the button, go to AWAKE_AT_REST.
    // If they've held the button down for a little bit,
    // start doing a reading. If it was a double tap, go to
    // ISO / exposure set mode.
    setup_capsense();
    press p = get_pad_press(CENTER_BUTTON, LONG_PRESS_MS);
    disable_capsense();

    rtc_init();

    if (p == PRESS_HOLD) {
        g_state.mode = MODE_DOING_READING;
    } else if (p == PRESS_TAP) {
        // It was just a tap.
        g_state.last_reading_flags |= LAST_READING_FLAGS_FRESH;
        g_state.mode = MODE_AWAKE_AT_REST;
    } else {
        SEGGER_RTT_printf(0, "Unknown but\n");
    }
}

static void handle_MODE_AWAKE_AT_REST()
{
    // Display the current reading, if any.
    if (fresh_reading_is_saved()) {
        g_state.mode = MODE_DISPLAY_READING;
        g_state.last_reading_flags &= ~(int32_t)LAST_READING_FLAGS_FRESH;
    } else {
        g_state.mode = MODE_SNOOZE;
    }
}

static void handle_MODE_SNOOZE()
{
    // Make sure LDO is off
    GPIO_PinModeSet(REGMODE_PORT, REGMODE_PIN, gpioModeDisabled, 0);

    setup_le_capsense_sleep();

    SEGGER_RTT_printf(0, "EM2 for snooze\n");
    my_emu_enter_em2(true); // true = restore oscillators, clocks and voltage scaling

    SEGGER_RTT_printf(0, "Woken!\n");
    g_state.mode = MODE_JUST_WOKEN;
}

static int shift_exposure_wheel(int n, int *ap_index, int *ss_index, bool using_long_ss)
{
    int ap = *ap_index;
    int ss = *ss_index;
    while (n != 0) {
        if (n > 0) {
            if (ap == AP_INDEX_MAX)
                break;
            if ((using_long_ss && ss == SS_INDEX_LONG_MIN) || (!using_long_ss && ss == SS_INDEX_MIN))
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

    return n;
}

static void handle_MODE_DISPLAY_READING()
{
    int32_t iso = iso_dial_pos_and_third_to_iso(g_state.iso_dial_pos, g_state.iso_third);
    int ap_index, ss_index, third;
    ev_iso_aperture_to_shutter(g_state.last_reading_ev, iso, g_state.last_ap, &ap_index, &ss_index, &third);
    bool using_long_ss = ss_index < SS_INDEX_MIN;
    SEGGER_RTT_printf(0, "ISOi=%u, api=%s%u, ssi=%s%u\n", iso, sign_of(ap_index), iabs(ap_index), sign_of(ss_index), iabs(ss_index));

    leds_all_off();
    set_led_throb_mask(0);
    set_led_flash_mask(0);
    leds_on(led_mask_for_reading(ap_index, ss_index, third));

    setup_capsense();

    uint32_t base_cycles = leds_on_for_cycles;
    int zero_touch_position = INVALID_TOUCH_POSITION;
    uint32_t touch_counts[] = { 0, 0, 0 };
    bool in_center_button_dead_zone = true;
    uint32_t ellapsed;
    for (unsigned i = 0;; ++i) {
        delay_ms_cyc_prepare {
            get_touch_count(0, 0, 0); // clear any nonsense value
        }
        ellapsed = delay_ms_cyc_loop(PAD_COUNT_MS);

        uint32_t count, chan;
        get_touch_count(&count, &chan, ellapsed);
        touch_counts[chan] = count;

        if (leds_on_for_cycles - base_cycles > (CENTER_BUTTON_DEAD_ZONE_MS * RTC_RAW_FREQ) / 1000)
            in_center_button_dead_zone = false;

        if (touch_counts[0] != 0 && touch_counts[1] != 0 && touch_counts[2] != 0) {
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
                            uint32_t over_base = leds_on_for_cycles;
                            int misses = 0;
                            touch_counts[0] = 0, touch_counts[1] = 0, touch_counts[2] = 0;
                            for (unsigned j = 0;; ++j) {
                                delay_ms_cyc_prepare {
                                    get_touch_count(0, 0, 0); // clear any nonsense value
                                }
                                ellapsed = delay_ms_cyc_loop(PAD_COUNT_MS);

                                get_touch_count(&count, &chan, ellapsed);
                                touch_counts[chan] = count;

                                if (touch_counts[0] != 0 && touch_counts[1] != 0 && touch_counts[2] != 0) {
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

                                if (leds_on_for_cycles - over_base >= (DOUBLE_BUTTON_SLOP_MS * RTC_RAW_FREQ / 1000))
                                    break;

                                cycle_capsense();
                            }
                        }

                        if (tp == LEFT_AND_RIGHT_BUTTONS && get_pad_press(LEFT_AND_RIGHT_BUTTONS, ISO_LONG_PRESS_MS) == PRESS_HOLD) {
                            goto handle_double_button_press;
                        }
                        else {
                            int r = shift_exposure_wheel(tp == RIGHT_BUTTON ? 1 : -1, &ap_index, &ss_index, using_long_ss);
                            g_state.last_ap = ap_index;
                            uint32_t mask = led_mask_for_reading(ap_index, ss_index, third);
                            leds_change_mask(mask);
                            if (r != 0) {
                                uint32_t flash_mask = (mask & ~((1 << LED_MINUS_1_3_N) | (1 << LED_PLUS_1_3_N)));
                                set_led_flash_mask(flash_mask);
                                delay_ms_cyc(1000);
                                set_led_flash_mask(0);
                            }
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

        if (leds_on_for_cycles - base_cycles >= DISPLAY_READING_TIME_SECONDS * RTC_RAW_FREQ) {
            SEGGER_RTT_printf(0, "Reading timeout\n");
            break;
        }

        cycle_capsense();
    }

    disable_capsense();
    leds_all_off();

    g_state.mode = MODE_SNOOZE;

    return;

handle_center_press:
    leds_all_off();
    press p = get_pad_press(CENTER_BUTTON, LONG_PRESS_MS);
    disable_capsense();
    if (p == PRESS_HOLD)
        g_state.mode = MODE_DOING_READING;
    else if (p == PRESS_TAP)
        g_state.mode = MODE_SNOOZE;
    return;

handle_double_button_press:
    leds_all_off();
    disable_capsense();
    g_state.mode = MODE_SETTING_ISO;
}

static void handle_MODE_SETTING_ISO()
{
    leds_all_off();

    uint32_t leds = 1 << iso_dial_pos_to_led_n(g_state.iso_dial_pos);
    set_led_throb_mask(leds);
    if (g_state.iso_third == -1)
        leds |= 1 << LED_MINUS_1_3_N;
    else if (g_state.iso_third == 1)
        leds |= 1 << LED_PLUS_1_3_N;
    leds_on(leds);

    setup_capsense();

    uint32_t base_cycles = leds_on_for_cycles;
    int zero_touch_position = INVALID_TOUCH_POSITION;
    uint32_t touch_counts[] = { 0, 0, 0 };
    for (;;) {
        delay_ms_cyc_prepare {
            get_touch_count(0, 0, 0);
        }
        uint32_t ellapsed = delay_ms_cyc_loop(PAD_COUNT_MS);
        uint32_t count, chan;
        get_touch_count(&count, &chan, ellapsed);
        touch_counts[chan] = count;

        if (touch_counts[0] != 0 && touch_counts[1] != 0 && touch_counts[2] != 0) {
            int tp = get_touch_position(touch_counts[0], touch_counts[1], touch_counts[2]);
            
            if (tp == NO_TOUCH_DETECTED) {
                zero_touch_position = INVALID_TOUCH_POSITION;
            } else {
                base_cycles = leds_on_for_cycles;

                if (zero_touch_position == INVALID_TOUCH_POSITION) {
                    if (tp == LEFT_BUTTON || tp == RIGHT_BUTTON) {
                        // Check if it's a tap or a hold, if we're on one
                        // of the ISOs that can be modified with +/- 1/3rd.
                        press p = PRESS_TAP;
                        bool cycled = false;
                        if ((tp == LEFT_BUTTON && iso_dial_pos_can_go_third_below(g_state.iso_dial_pos)) || (tp == RIGHT_BUTTON && iso_dial_pos_can_go_third_above(g_state.iso_dial_pos))) {
                            if (tp == LEFT_BUTTON) {
                                cycled = true;
                                cycle_capsense();
                            }
                        
                            p = get_pad_press(tp, ISO_LONG_PRESS_MS);
                        }

                        int mag = (tp == RIGHT_BUTTON ? 1 : -1);
                        if (p == PRESS_TAP) {
                            g_state.iso_third = 0;
                            g_state.iso_dial_pos += mag;
                            if (g_state.iso_dial_pos < 0)
                                g_state.iso_dial_pos += ISO_N_DIAL_POSITIONS;
                            else
                                g_state.iso_dial_pos %= ISO_N_DIAL_POSITIONS;
                        } else {
                            // Button press will only have registered as a hold if this is a thirdable point on the dial.
                            g_state.iso_third = mag;
                        }
                        
#ifdef DEBUG
                        int32_t iso = iso_dial_pos_and_third_to_iso(g_state.iso_dial_pos, g_state.iso_third);
                        SEGGER_RTT_printf(0, "ISO -> %u (i) (dial pos %u)\n", iso, g_state.iso_dial_pos);
#endif
                        
                        leds = 1 << iso_dial_pos_to_led_n(g_state.iso_dial_pos);
                        set_led_throb_mask(leds);
                        if (g_state.iso_third == -1)
                            leds |= 1 << LED_MINUS_1_3_N;
                        else if (g_state.iso_third == 1)
                            leds |= 1 << LED_PLUS_1_3_N;
                        leds_change_mask(leds);

                        SEGGER_RTT_printf(0, "ISO -> raw %u\n", iso_dial_pos_and_third_to_iso(g_state.iso_dial_pos, g_state.iso_third));

                        if (cycled) {
                            cycle_capsense();
                            cycle_capsense();
                        }
                    } else if (tp == CENTER_BUTTON) {
                        goto handle_center_press;
                    }
                    zero_touch_position = tp;
                } else {
                    zero_touch_position = tp;
                }
            }            
        }

        if (leds_on_for_cycles - base_cycles >= DISPLAY_READING_TIME_SECONDS * RTC_RAW_FREQ) {
            SEGGER_RTT_printf(0, "Reading timeout (ISO mode)\n");
            break;
        }

        cycle_capsense();
    }

    leds_all_off();
    disable_capsense();

    SEGGER_RTT_printf(0, "Enter MODE_SNOOZE\n");
    g_state.mode = MODE_SNOOZE;

    return;

handle_center_press:
    SEGGER_RTT_printf(0, "ISO button press\n");
    leds_all_off();
    press p = get_pad_press(CENTER_BUTTON, LONG_PRESS_MS);
    disable_capsense();
    if (p == PRESS_HOLD)
        g_state.mode = MODE_DOING_READING;
    else if (p == PRESS_TAP)
        g_state.mode = MODE_SNOOZE;
}

static uint32_t display_reading_interrupt_cycle_mask1;
static uint32_t display_reading_interrupt_cycle_mask2;
static uint32_t display_reading_interrupt_cycle_iso_mask;
static void display_reading_interrupt_cycle_interrupt_handler()
{
    if (leds_on_for_cycles % 16 != 0)
        return;

    if (leds_on_for_cycles % 32 != 0) {
        display_reading_interrupt_cycle_mask1 = (display_reading_interrupt_cycle_mask1 + 1) % LED_N_IN_WHEEL;
    }

    display_reading_interrupt_cycle_mask2 = (display_reading_interrupt_cycle_mask2 + 1) % LED_N_IN_WHEEL;

    leds_change_mask((1 << display_reading_interrupt_cycle_iso_mask) | (1 << (LED_N_IN_WHEEL - display_reading_interrupt_cycle_mask1 - 1)) | (1 << display_reading_interrupt_cycle_mask2));
}

static void handle_MODE_DOING_READING()
{
    // Light show while the reading is being done.
    leds_all_off();
    add_rtc_interrupt_handler(display_reading_interrupt_cycle_interrupt_handler);
    display_reading_interrupt_cycle_mask1 = 8;
    display_reading_interrupt_cycle_mask2 = 16;
    display_reading_interrupt_cycle_iso_mask = iso_dial_pos_to_led_n(g_state.iso_dial_pos);
    leds_on((1 << display_reading_interrupt_cycle_iso_mask) | (1 << display_reading_interrupt_cycle_mask1) | (1 << display_reading_interrupt_cycle_mask2));

    // Turn on the LDO to power up the sensor.
    GPIO_PinModeSet(REGMODE_PORT, REGMODE_PIN, gpioModePushPull, 1);
    delay_ms_cyc(10); // make sure LDO has time to start up (datasheet says 1ms startup time is typical, so 10 is more than enough)
    sensor_init();
    delay_ms_cyc(100); // sensor requires 100ms initial startup time.

    // Turn the sensor on and give it time to wake up from standby.
    sensor_turn_on(GAIN_1X);
    delay_ms_cyc(10);

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
    int32_t ev = lux_to_reflective_ev(lux);
    g_state.last_reading_ev = ev;
    SEGGER_RTT_printf(0, "READING g=%u itime=%u c0=%u c1=%u lux=%u/%u (%u) ev=%s%u/%u (%u)\n", gain, itime, sr.chan0, sr.chan1, lux, 1<<EV_BPS, lux>>EV_BPS, sign_of(ev), iabs(ev), 1<<EV_BPS, ev>>EV_BPS);

    g_state.led_brightness_ev_ref = ev;

    // Turn the LDO off to power down the sensor.
    GPIO_PinModeSet(REGMODE_PORT, REGMODE_PIN, gpioModeInput, 0);

    remove_rtc_interrupt_handler(display_reading_interrupt_cycle_interrupt_handler);
    leds_all_off();

    // If they're still holding down the button, display the reading
    // indefinitely until the button is released, then go into the
    // regular display reading mode.
    setup_capsense();
    uint32_t chans[] = { 0, 0, 0 };
    do {
        delay_ms_cyc_prepare {
            get_touch_count(0, 0, 0); // clear any nonsense value
        }
        uint32_t ellapsed = delay_ms_cyc_loop(PAD_COUNT_MS);
        uint32_t count, chan;
        get_touch_count(&count, &chan, ellapsed);
        chans[chan] = count;
        cycle_capsense();
    } while (chans[0] == 0 || chans[1] == 0 || chans[2] == 0);
    if (get_touch_position(chans[0], chans[1], chans[2]) == CENTER_BUTTON) {
        int32_t iso = iso_dial_pos_and_third_to_iso(g_state.iso_dial_pos, g_state.iso_third);
        int ap_index, ss_index, third;
        ev_iso_aperture_to_shutter(g_state.last_reading_ev, iso, g_state.last_ap, &ap_index, &ss_index, &third);
        SEGGER_RTT_printf(0, "Third: %s%u\n", sign_of(third), iabs(third));
        set_led_throb_mask(0);
        set_led_flash_mask(0);
        leds_on(led_mask_for_reading(ap_index, ss_index, third));

        chans[0] = 0, chans[1] = 0, chans[2] = 0;
        int misses = 0;
        for (;;) {
            delay_ms_cyc_prepare {
                get_touch_count(0, 0, 0); // clear any nonsense value
            }
            uint32_t ellapsed = delay_ms_cyc_loop(PAD_COUNT_MS);
            uint32_t count, chan;
            get_touch_count(&count, &chan, ellapsed);
            chans[chan] = count;

            if (chans[0] != 0 && chans[1] != 0 && chans[2] != 0) {
                if (get_touch_position(chans[0], chans[1], chans[2]) != CENTER_BUTTON) {
                    ++misses;
                    if (misses >= MISSES_REQUIRED_TO_BREAK_HOLD)
                        break;
                } else {
                    misses = 0;
                }
            }

            cycle_capsense();
        }
    }

    leds_all_off();
    disable_capsense();

    g_state.mode = MODE_DISPLAY_READING;
}

static void state_loop()
{
    for (;;) {
        switch (g_state.mode) {
            case MODE_JUST_WOKEN: {
                SEGGER_RTT_printf(0, "M=JW\n");

                handle_MODE_JUST_WOKEN();
            } break;
            case MODE_AWAKE_AT_REST: {
                SEGGER_RTT_printf(0, "M=AAR\n");

                handle_MODE_AWAKE_AT_REST();
            } break;
            case MODE_DOING_READING: {
                SEGGER_RTT_printf(0, "M=RD\n");

                handle_MODE_DOING_READING();
            } break;
            case MODE_DISPLAY_READING: {
                SEGGER_RTT_printf(0, "M=D_RD\n");

                handle_MODE_DISPLAY_READING();
            } break;
            case MODE_SETTING_ISO: {
                SEGGER_RTT_printf(0, "M=SISO\n");

                handle_MODE_SETTING_ISO();
            } break;
            case MODE_SNOOZE: {
                SEGGER_RTT_printf(0, "M=SNZ\n");

                handle_MODE_SNOOZE();
            } break;
        }
    }
}

static int real_main(bool watchdog_wakeup)
{
    set_state_to_default(watchdog_wakeup ? MODE_JUST_WOKEN : MODE_SNOOZE);
    g_state.watchdog_wakeup = watchdog_wakeup;

    state_loop();

    return 0;
}

int32_t deep_sleep_capsense_recalibration_counter __attribute__((section (".persistent")));

int main()
{
    // We have to do without the chip errata as running this after every watchdog reset
    // in deep sleep mode consumes too much current. I've verified that this function
    // doesn't do anything that we need.
    //
    // CHIP_Init();

#if defined(TEST_MAIN) || defined(DISABLE_DEEP_SLEEP)
    bool watchdog_wakeup = false;
#else
    uint32_t reset_cause = RMU_ResetCauseGet();
    RMU_ResetCauseClear();
    bool watchdog_wakeup = ((reset_cause & RMU_RSTCAUSE_WDOGRST) != 0);
#endif

    common_init(watchdog_wakeup);

#ifdef TEST_MAIN
    return test_main();
#else

    if (! watchdog_wakeup) {
        deep_sleep_capsense_recalibration_counter = 0;

        // Give a grace period before calibrating capsense, so that
        // the programming header can be disconnected first.
#if (!defined(DEBUG) && !defined(NOGRACE)) || defined(GRACE)
        leds_on(1);
        uint32_t base = leds_on_for_cycles;
        while (leds_on_for_cycles - base < RTC_RAW_FREQ * GRACE_PERIOD_SECONDS)
            WDOGn_Feed(WDOG); // make sure no reset is triggered if the watchdog is enabled
        leds_all_off();
#endif

#ifndef DEBUG
        if (! DBG_Connected()) {
            GPIO_PinModeSet(gpioPortF, 1, gpioModeDisabled, 0);
            GPIO_PinModeSet(gpioPortF, 0, gpioModeDisabled, 0);
            GPIO_DbgSWDClkEnable(false);
            GPIO_DbgSWDIOEnable(false);
        } else {
            // Give visual indication that (we think that) debugger is connected.
            leds_on(0b101);
            uint32_t base = leds_on_for_cycles;
            while (leds_on_for_cycles - base < RTC_RAW_FREQ)
                WDOGn_Feed(WDOG);
            leds_all_off();
        }
#endif

        calibrate_capsense();
        calibrate_le_capsense();
    } else {
        setup_le_capsense_oneshot();
        my_emu_enter_em2(true);

        if (! le_center_pad_is_touched(lesense_result)) {
            if (deep_sleep_capsense_recalibration_counter++ >= LE_CAPSENSE_DEEP_SLEEP_CALIBRATION_INTERVAL_SECONDS) {
                calibrate_le_capsense();
                deep_sleep_capsense_recalibration_counter = 0;
            }
            go_into_deep_sleep();
        }

        disable_le_capsense();

        CMU_ClockEnable(cmuClock_RTC, true);
        rtc_init();
    }

    return real_main(watchdog_wakeup);
#endif
}
