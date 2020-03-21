#include <tests/include.h>

int test_main()
{
    leds_all_off();

    leds_on_for_reading(F22_AP_INDEX, SS500_INDEX, 0);

    for (;;);

    return 0;
}