#include <tests/include.h>

int test_main()
{
    SEGGER_RTT_printf(0, "Capsense test...\n");

    // Turn on the LDO to power up the temp sensor.
    GPIO_PinModeSet(REGMODE_PORT, REGMODE_PIN, gpioModePushPull, 1);
    delay_ms(100); // make sure LDO has time to start up and sensor has time to
    tempsensor_init();
    delay_ms(100);

    setup_capsense();

    int32_t temp_reading = tempsensor_get_reading(delay_ms);

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
            SEGGER_RTT_printf(0, "count %u %u %u pos = %s (temp %s%u)\n", touch_counts[1], touch_counts[0], touch_counts[2], tps, sign_of(temp_reading), iabs(temp_reading >> 8));
        
        cycle_capsense();

        delay_ms(PAD_COUNT_MS);
    }

    return 0;
}
