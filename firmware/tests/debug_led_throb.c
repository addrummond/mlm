#include <tests/include.h>

int test_main()
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

    return 0;
}