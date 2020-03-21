#include <tests/include.h>

int test_main()
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
