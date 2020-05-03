#include <tests/include.h>

int test_main()
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
            get_touch_count(&count, &chan, 17);
            touch_counts[chan] = count;
            int tp = get_touch_position(touch_counts[0], touch_counts[1], touch_counts[2]);
            
            if (tp == NO_TOUCH_DETECTED) {
                zero_touch_position = INVALID_TOUCH_POSITION;
            } else {
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

        delay_ms_cyc(PAD_COUNT_MS);
    }

    return 0;
}