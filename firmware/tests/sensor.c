#include <tests/include.h>

int test_main()
{
    // Turn on the LDO to power up the sensor.
    GPIO_PinModeSet(REGMODE_PORT, REGMODE_PIN, gpioModePushPull, 1);
    SEGGER_RTT_printf(0, "LDO turned on\n");
    delay_ms_cyc(100); // make sure LDO has time to start up and sensor has time to
                   // power up
    sensor_init();
    delay_ms_cyc(100);

    // Turn the sensor on an give it time to get ready. (We have to set a gain
    // value when we turn the sensor on, but the choice here is immaterial.)
    sensor_turn_on(GAIN_1X);
    delay_ms_cyc(10);

    for (;;) {
        int32_t gain, itime;
        sensor_wait_till_ready();
        sensor_reading sr = sensor_get_reading_auto(&gain, &itime);
        int32_t lux = sensor_reading_to_lux(sr, gain, itime);
        int32_t ev = lux_to_reflective_ev(lux);
        int ap_index, ss_index, third;
        ev_iso_aperture_to_shutter(ev, ISO_100, F8_AP_INDEX, &ap_index, &ss_index, &third);
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

    return 0;
}